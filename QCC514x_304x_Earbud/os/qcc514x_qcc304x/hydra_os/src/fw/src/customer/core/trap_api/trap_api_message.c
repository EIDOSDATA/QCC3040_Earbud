/* Copyright (c) 2016 - 2018 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "trap_api/trap_api_private.h"

#include <message.h>
#include <sink.h>

#include "pmalloc/pmalloc.h"
#include "panic/panic.h"
#include "assert.h"
#include "hydra/hydra_macros.h"
#include "hydra_log/hydra_log.h"
#include "ipc/ipc.h"
#include "longtimer/longtimer.h"

#include "sched/sched.h"
#define IO_DEFS_MODULE_K32_CORE
#define IO_DEFS_MODULE_K32_DEBUG
#define IO_DEFS_MODULE_K32_MISC
#include "io/io.h"

/*
 * To correctly reproduce the API, which in practice takes pointers to uint16
 * and Task (which is in fact a pointer), we rely on the fact that uint16s on
 * the Kalimba really are 16 bits wide and pointers are 32 bits wide so that
 * masking a value to the expected number of bits is exactly equivalent to
 * casting it to its original type.
 */
COMPILE_TIME_ASSERT(sizeof(uint16)*CHAR_BIT == CONDITION_WIDTH_16BIT,
                    uint16_is_really_16_bits);
COMPILE_TIME_ASSERT(sizeof(Task)*CHAR_BIT == CONDITION_WIDTH_32BIT,
                    Task_is_really_32_bits);

/**
 * Macro for comparing times
 */
#define VM_DIFF(t,u) (((int32)(t)) - ((int32)(u)))

/** Not currently used. Can be enabled for logging primitives. */
#define vm_debug_message_send(x)

/** Not currently used. Can be enabled for logging primitives. */
#define vm_debug_message_cancel(x)

/** Queue of messages waiting to be delivered */
AppMessage *vm_message_queue;

/**
 * Queue of messages that have been delivered but not freed.
 * The head of the list contains the message we're currently processing.
 */
static AppMessage *vm_delivered_messages = NULL;

/**
  Messages sent to the api message task to reschedule it in the background,
  either because a wait has expired, an enabled event has been
  triggered, host comms has completed, or a connection is dirty.
*/
enum
{
    VM_MSG_STANDARD_RUN,
    VM_MSG_SIMPLE_EVENT
};

/**
 *  run_timer is non-NULL when we have a message DELAYED in the VM_QUEUE.
 *  Set to initial 0 by crt
 */
static msgid run_timer;

/**
 * Whether we have already posted a message to our own task to indicate we
 * have work to do. Used to prevent having more than one outstanding message
 * in the scheduler.
 */
static bool sched_event_pending;

/**
 * Array of App-Task/sched-taskid pairings for registered message handlers
 *
 * \note The initialiser for this array must correspond to the enumeration above
 */
Task registered_hdlrs[NUM_IPC_MSG_TYPES];

Task registered_pio_hdlrs[PIODEBOUNCE_NUMBER_OF_GROUPS];

static void handle_message_free(MessageId id, void *msg);

/**
 * @brief Decrement the AppMessage refcount and destroy if it reaches 0.
 *
 * @param [in] p  A pointer to the AppMessage pointer, this is a pointer to a
 * pointer so that the AppMessage can be unlinked from the vm_delivered_messages
 * list when it is destroyed.
 * @param [in] now_ms  The current time in milliseconds.
 */
static void app_message_release(AppMessage **p, MILLITIME now_ms);

/**
 * @brief Free the memory for an AppMessage.
 *
 * Used when the reference count hits zero, or we know it will hit zero as a
 * result of calling release.
 *
 * @param [in] a  The AppMessage to destroy.
 */
static void app_message_destroy(AppMessage *a);

#if TRAPSET_STREAM

/**
 * Entry in the dynamic array of \c Sink or \c Source handler \c Tasks
 */
typedef struct SINK_SOURCE_HDLR_ENTRY
{
    Sink sink;   /**< The \c Sink being handled */
    Source source; /**< The \c Source being handled */
    Task task;   /**< The \c Task/sched taskid doing the handling */
} SINK_SOURCE_HDLR_ENTRY;

/**
 * Combined array/linked-list structure for stream handlers
 */
typedef struct TRAP_API_SINK_SOURCE_MSG_HDLR
{
    SINK_SOURCE_HDLR_ENTRY entry;                /**< handler entry */
    struct TRAP_API_SINK_SOURCE_MSG_HDLR *next;  /**< Pointer to next block */
} TRAP_API_SINK_SOURCE_MSG_HDLR;

/**
 * The list of sink message handlers
 */
static TRAP_API_SINK_SOURCE_MSG_HDLR *sink_source_hdlrs;

static bool trap_api_remove_sink_source_hdlr_entry(Sink sink, Source source,
                                                   Task *old_task);
static void trap_api_remove_sink_source_hdlr_entries_for_task(Task task);
#endif /* TRAPSET_STREAM */

/*
 * In the trap API certain functions take parameters of type Message, which is
 * actually const void *, but they need to pass the argument on to functions
 * which take void *.  So we have to kludge away the const-ness via a uint32
 * cast.
 */
COMPILE_TIME_ASSERT(sizeof(Message) == sizeof(uint32), Message_not_32_bits);
COMPILE_TIME_ASSERT(sizeof(void *) == sizeof(uint32), Pointer_not_32_bits);

static void vm_message_forget(Task task);
static uint32 vm_message_next(void);
static void vm_message_send_later(Task *task, bool multicast, uint16 id, void *message,
                           uint32 delay, const void * c,
                           CONDITION_WIDTH c_width);
static void vm_event_trigger(void);

static uint32 get_message_condition_value(const void *c,
                                          CONDITION_WIDTH c_width);


/**
 * Remove a task from the registered handlers list and from any
 * streams or operators.
 * @param task The task to be removed
 */
static void vm_message_forget(Task task)
{
    unsigned i;

    /* Forget about all this task for the static records
     * like vm_message_host_task */
    for(i = 0; i < ARRAY_DIM(registered_hdlrs); ++i)
        if(registered_hdlrs[i] == task)
           registered_hdlrs[i] = (Task)0;

#if defined(INSTALL_STREAM_MODULE) && defined(INSTALL_VM_STREAM_SUPPORT)
    /* Now forget about it if it's being used in streams */
    trap_api_remove_sink_source_hdlr_entries_for_task(task);
#endif /*INSTALL_STREAM_MODULE  && INSTALL_VM_STREAM_SUPPORT*/
}

/**
 * Get the next message or return the time in milliseconds until the next
 * message is due.
 */
static uint32 vm_message_next(void)
{
    AppMessage *a;
    AppMessage **p = &vm_message_queue;

    /* Find the first message which isn't blocked on a condition */
    while((a = *p) != 0)
    {
        const void * c = a->condition_addr;
        if(c==0 ||
                get_message_condition_value(c, a->c_width) == 0)
        {
            break;/* No condition or it's satisfied */
        }
        p = &a->next;
    }

    if(a)
    {
        /* Found a message */
        uint32 now  = get_milli_time();
        int32 delta = VM_DIFF(a->due, now);
        if(delta > 0)
        {
            /* Message isn't ready for delivery yet */
            return (uint32)delta;
        }
        else
        {
            /* Unlink the message from the queue */
            *p = a->next;

            /* Place at the head of the delivered list. */
            a->next = vm_delivered_messages;
            vm_delivered_messages = a;

            /* Deliver the message to the handler(s) */
            if (!a->multicast)
            {
                if(a->t.task && a->t.task->handler)
                {
                    trap_api_message_log_now(TRAP_API_LOG_DELIVER, a, now);
                    VALIDATE_FN_PTR(a->t.task->handler);
                    a->t.task->handler(a->t.task, a->id, a->message);
                }
            }
            else
            {
                Task *tptr = a->t.tlist;

                trap_api_message_log_now(TRAP_API_LOG_DELIVER, a, now);
                /* Loop through the tasks in the list, despatching to all */
                while (*tptr != NULL)
                {
                    if ((*tptr != (Task)INVALIDATED_TASK) && (*tptr)->handler)
                    {
                        VALIDATE_FN_PTR((*tptr)->handler);
                        (*tptr)->handler(*tptr, a->id, a->message);
                    }
                    tptr++;
                }
            }

            /* The current message is at the head of the delivered list.
               Once the message has been delivered it's possible the handler
               has called MessageRetain(), so we can't blindly call
               app_message_destroy. */
            app_message_release(&vm_delivered_messages, now);

            return 0;
        }
    }
    else
    {
        /* No messages; just wait for as long as possible */
        return ((uint32) -1);
    }
}

/**
 * Compares two messages and decides whether they are similar enough
 * that one could be dropped.
 * @param a First message
 * @param b Second message
 * @return TRUE if the two events are equivalent
 */
static bool similar(const AppMessage *a, const AppMessage *b)
{
    /* This is used only for P0->P1 messages, so we can ignore multicast */
    if(a->multicast)
    {
        return FALSE;
    }

    if(a->t.task == b->t.task && a->id == b->id)
    {
        switch(a->id)
        {
            case MESSAGE_MORE_DATA:
            {
                const MessageMoreData *ma = (const MessageMoreData *) a->message;
                const MessageMoreData *mb = (const MessageMoreData *) b->message;
                return ma->source == mb->source;
            }
            case MESSAGE_MORE_SPACE:
            {
                const MessageMoreSpace *ma = (const MessageMoreSpace *) a->message;
                const MessageMoreSpace *mb = (const MessageMoreSpace *) b->message;
                return ma->sink == mb->sink;
            }
            case MESSAGE_PSFL_FAULT:
            case MESSAGE_TX_POWER_CHANGE_EVENT:
            {
                return TRUE;
            }
            default:
                break;
        }
    }
    return FALSE;
}

/**
 * Replace one message with another. Used when a later message contains
 * more up to date information than an earlier one.
 * @param a Message to be replaced
 * @param b Message to put in place of @c a
 * @return TRUE if message @c a was replaced by message @c b
 */
static bool replace(AppMessage *a, const AppMessage *b)
{
    /* This is used only for P0->P1 messages, so we can ignore multicast */
    if(a->multicast)
    {
        return FALSE;
    }

    if(a->t.task != b->t.task)
    {
        return FALSE;
    }
    switch(a->id)
    {
    case MESSAGE_USB_SUSPENDED:
        if(a->id == b->id)
        {
            MessageUsbSuspended *ma = (MessageUsbSuspended *) a->message;
            const MessageUsbSuspended *mb =
                                      (const MessageUsbSuspended *) b->message;
            *ma = *mb;
            return TRUE;
        }
        break;

    case MESSAGE_USB_ENUMERATED:
    case MESSAGE_USB_DECONFIGURED:
        switch(b->id)
        {
            /* This code is much reduced by giving the deconfigured message
             * the same kind of payload as the enumerated message. If the
             * deconfigured message didn't have a payload, you would have to
             * deal separately with the different permutations. */
        case MESSAGE_USB_ENUMERATED:
        case MESSAGE_USB_DECONFIGURED:
            {
                MessageUsbConfigValue *ma = (MessageUsbConfigValue *) a->message;
                const MessageUsbConfigValue *mb =
                                     (const MessageUsbConfigValue *) b->message;
                /* copy across payload and update id */
                *ma = *mb;
                a->id = b->id;
                return TRUE;
            }
        default:
            break;
        }
        break;

    case MESSAGE_USB_ATTACHED:
    case MESSAGE_USB_DETACHED:
        switch(b->id)
        {
        case MESSAGE_USB_ATTACHED:
        case MESSAGE_USB_DETACHED:
            a->id = b->id; /* No payload, just overwrite id */
            return TRUE;
        default:
            break;
        }
        break;

    case MESSAGE_USB_ALT_INTERFACE:
        if(a->id == b->id)
        {
            MessageUsbAltInterface *ma = (MessageUsbAltInterface *) a->message;
            const MessageUsbAltInterface *mb =
                                   (const MessageUsbAltInterface *) b->message;

            if(ma->interface == mb->interface)
            {
                *ma = *mb;
                return TRUE;
            }
        }
        break;

    default:
        break;
    }
    return FALSE;
}

/**
 * Checks whether a message can be combined with one already in the message
 * queue by using the @c similar() and @c replace() functions.
 * @param task Task the message will be posted to
 * @param id ID of the message
 * @param message Message contents
 * @return TRUE if the message was combined with a previous one.
 */
static bool already(Task task, uint16 id, uint16 *message)
{
    AppMessage *p;
    AppMessage temp;

    temp.multicast = 0;
    temp.t.task  = task;
    temp.id      = id;
    temp.message = message;
    for(p = vm_message_queue; p != 0; p = p->next)
    if(similar(p, &temp) || replace(p, &temp))
            return TRUE;
    return FALSE;
}

/**
 * Insert a message into the queue if a similar one isn't already present.
 * The queue is kept in time order.
 * @param a Message being posted
 * @return TRUE if the message was inserted into the queue or
 * FALSE if there was already a similar one present.
 */
static bool insert(AppMessage *a)
{
    uint32 due = a->due;
    AppMessage **p = &vm_message_queue, *prev = NULL;

    while(*p && VM_DIFF(due, (*p)->due) >= 0)
    { prev = *p; p = &prev->next; }

    if(prev && similar(prev, a))
    {
        return FALSE;
    }
    else
    {
        a->next = *p;
        *p = a;
        return TRUE;
    }
}

/**
 * Send a message immediately
 * @param task Task to deliver the message to
 * @param id ID of the message
 * @param message The message contents
 */
static void send_message(Task task, uint16 id, void *message)
{
    vm_message_send_later(&task, FALSE, id, message, D_IMMEDIATE, NULL, CONDITION_WIDTH_UNUSED);
}


/**
 * Send a message after a time delay
 * @param task Pointer to task(s) to deliver the message to
 * @param multicast Whether 'task' is a pointer to a list or not
 * @param id ID of the message
 * @param message The message contents
 * @param delay Number of milliseconds to wait before delivering the message
 * @param c Optional pointer to a condition which must be zero for the
 * message to be delivered.
 * @param c_width Whether to test the condition as a 16 or 32-bit variable
 * or @c CONDITION_WIDTH_UNUSED if @c c is NULL.
 */
static void vm_message_send_later(Task *task, bool multicast, uint16 id, void *message,
                           uint32 delay, const void * c,
                           CONDITION_WIDTH c_width)
{
    AppMessage *a;
    uint32 timenow = get_milli_time();
    Task *tptr;
    uint8 lc = 1; /* Pre-account for the null entry */

    a = pnew(AppMessage);
    if (multicast)
    {
        if (task == NULL || *task == NULL)
        {
            /* We can't tolerate no list or an empty list */
            panic(PANIC_P1_VM_MESSAGE_NULL_TASK_LIST);
        }

        /* We need to walk the list we've been given to copy it */
        tptr = task;

        while (*tptr != NULL)
        {   
            tptr++;
            lc +=1;
        }

        if (lc > MAX_MULTICAST_RECIPIENTS)
        {
            /* We can't cope with more than this many recipients (stack usage
             * in the message logger)
             */
            panic(PANIC_P1_VM_MESSAGE_TOO_MANY_RECIPIENTS);
        }
        a->t.tlist    = pmalloc(sizeof(Task)*lc);
        memcpy(a->t.tlist, task, sizeof(Task)*lc);
        a->multicast  = (uint8)(lc-1);
    }
    else
    {
#ifdef PANIC_ON_VM_MESSAGE_NULL_TASK_LIST
        if (task == NULL || *task == NULL)
        {
            /* Panic if PANIC_ON_VM_MESSAGE_NULL is set */
            panic(PANIC_P1_VM_MESSAGE_NULL_TASK_LIST);
        }
#endif /* PANIC_ON_VM_MESSAGE_NULL_TASK_LIST */
        a->t.task     = *task;
        a->multicast  = 0;
    }
    a->id             = id;
    a->message        = message;
    a->condition_addr = c;
    a->c_width        = c_width;
    a->due            = delay + timenow;
    a->refcount       = 1;

    if(insert(a))
    {
        vm_debug_message_send(a);
        trap_api_message_log_now(TRAP_API_LOG_SEND, a, timenow);
        vm_event_trigger();
    }
    else
    {
        app_message_destroy(a);
    }
}

/**
 * Trigger the scheduler to run the handler that will deliver a queued message.
 */
static void vm_event_trigger(void)
{
    if(!sched_event_pending)
    {
        sched_event_pending = put_message(trap_api_sched_queue_id,
                                          VM_MSG_SIMPLE_EVENT, NULL);
    }
}

/**
 * Handler to call when system messages are destroyed, either after delivering
 * them to the consumer task or when cancelling them or flushing their task
 * @param id  The message ID
 * @param msg The message body, cast to a non-const
 */
static void handle_message_free(MessageId id, void *msg)
{
    if(MESSAGE_BLUESTACK_BASE_ <= id && id < MESSAGE_BLUESTACK_END_)
    {
        /* Notify P0's bluestack stream tracking logic that the App has seen
         * this primitive.  It also gets freed at that point. */
        IPC_BLUESTACK_PRIM prim;
        prim.protocol = (uint16)(id - MESSAGE_BLUESTACK_BASE_);
        prim.prim = msg;
        ipc_send(IPC_SIGNAL_ID_BLUESTACK_PRIM_RECEIVED, &prim, sizeof(prim));
    }
    else if(id == MESSAGE_MORE_SPACE || id == MESSAGE_MORE_DATA)
    {
        /* Notify P0's bluestack stream that the App has seen this message.*/
        IPC_APP_MESSAGE_RECEIVED rcvd_msg;
        rcvd_msg.id = id;
        rcvd_msg.msg = msg;
        ipc_send(IPC_SIGNAL_ID_APP_MESSAGE_RECEIVED, &rcvd_msg, sizeof(rcvd_msg));
    }
    else
    {
        pfree(msg);
    }
}

/**
 * Register a task for handling a certain message type
 * @param task Task to receive the messages
 * @param msg_type_id Type of message the task is to be passed
 * @return The previous task registered for this message type
 */
Task trap_api_register_message_task(Task task, IPC_MSG_TYPE msg_type_id)
{
    Task old_atask; 
    uint32 i;

    assert(msg_type_id < ARRAY_DIM(registered_hdlrs));

    if (msg_type_id != IPC_MSG_TYPE_PIO)
    {
        old_atask = registered_hdlrs[msg_type_id];
        registered_hdlrs[msg_type_id] = task;        
    }
    else
    {       
        old_atask = registered_pio_hdlrs[0];
        for(i = 0; i < PIODEBOUNCE_NUMBER_OF_GROUPS; ++i)
        {
            registered_pio_hdlrs[i] = task;
        }
    }
    return old_atask;    
}

/**
 * Called by the scheduler when this scheduler task is started
 * @param ppriv Unused context
 */
void trap_api_sched_init(void **ppriv)
{
    UNUSED(ppriv);
}

/**
 * Call from the scheduler with a message posted to this scheduler queue to
 * cause a message to be delivered from the message queue. This function
 * posts a new scheduler message if there is more work to be done (messages
 * in the queue).
 * @param ppriv Unused context
 */
void trap_api_sched_msg_handler(void **ppriv)
{
    uint16 id;
    void *mv;

    UNUSED(ppriv);

    /* We handle a single message at a time to minimise the risk of the App
     * blocking out the background (casual timer processing) for too long. */
    if (get_message(trap_api_sched_queue_id, &id, &mv))
    {
        uint32 delay;
        switch(id)
        {
        case VM_MSG_STANDARD_RUN:
            /* We have removed the VMrun timer */
            run_timer = 0;
            break;
        case VM_MSG_SIMPLE_EVENT:
            sched_event_pending = 0;
            break;
        default:
            break;
        }
        if(run_timer)
        {
            /* If we still have VM_MSG_STANDARD_RUN in the scheduler queue
             *  then cancel it */
            (void)cancel_timed_message(trap_api_sched_queue_id, run_timer,
                                                                NULL, NULL);
            run_timer = 0;
        }

        /* We're waiting for a message */
        delay = vm_message_next();
        if(delay == 0)
        {
            /* A message has been handled so come back round the scheduler
             * again for the next pending message */
            vm_event_trigger();
        }
        else if(delay != (uint32)-1)
        {
            /* Avoid setting a timer for more than 1/4 of the wrap distance */
            uint32 limit = 0x3ffffffful / 1000u;

            if(delay > limit) delay = limit;
            run_timer = put_message_in((INTERVAL)(delay * 1000),
                                        trap_api_sched_queue_id,
                                        VM_MSG_STANDARD_RUN,
                                        NULL);
        }
    }
}

void MessageSendLater(Task task, MessageId id, void *message, uint32 delay)
{
    vm_message_send_later(&task, FALSE, id, message, delay, NULL, CONDITION_WIDTH_UNUSED);
}

void MessageSendMulticastLater(Task *tasklist, MessageId id, void *message, uint32 delay)
{
    vm_message_send_later(tasklist, TRUE, id, message, delay, NULL, CONDITION_WIDTH_UNUSED);
}


/*
 * This isn't needed because our MessageLoop is the real scheduler.  However,
 * some customers might want to write their own MessageLoop, so we might need to
 * implement this in due course.
 */
void MessageWait(void * m)
{
    UNUSED(m);
}

/**
 * Dereference the given void pointer, assuming it has the given width.
 * @param c Pointer to dereference (a message condition pointer)
 * @param c_width Width to cast to
 * @return Value in the pointer or 0 if it is NULL
 */
static uint32 get_message_condition_value(const void *c,
                                          CONDITION_WIDTH c_width)
{
    if (c != NULL)
    {
        switch(c_width)
        {
        case CONDITION_WIDTH_16BIT:
            return *(const uint16 *)c;
        case CONDITION_WIDTH_32BIT:
            return *(const uint32 *)c;
        default:
            break;
        }
    }
    return 0;
}

/*!
 *  \brief Send a message to be be delivered when the corresponding uint16 is zero.
 *  \param t The task to deliver the message to.
 *  \param id The message identifier.
 *  \param m The message data.
 *  \param c The condition that must be zero for the message to be delivered.
 *
 * \ingroup trapset_core
 */
void MessageSendConditionally(Task t, MessageId id, Message m, const uint16 * c)
{
    void *msg;
    /* Cast away the const from the message. The messages sent through this
     * interface will not be modified but can't be passed through as const
     * because they have to be queued alongside ones that are not const.
     */
    msg = (uint32 *)((uint32) m);
    vm_message_send_later(&t, FALSE, id, msg, D_IMMEDIATE, c, CONDITION_WIDTH_16BIT);
}

/*!
 *  \brief Send a message to be be delivered when the corresponding uint16 is zero.
 *  \param tlist Pointer to the tasks to deliver the message to.
 *  \param id The message identifier.
 *  \param m The message data.
 *  \param c The condition that must be zero for the message to be delivered.
 *
 * \ingroup trapset_core
 */
void MessageSendMulticastConditionally(Task *tlist, MessageId id, Message m, const uint16 * c)
{
    void *msg;
    /* Cast away the const from the message. The messages sent through this
     * interface will not be modified but can't be passed through as const
     * because they have to be queued alongside ones that are not const.
     */
    msg = (uint32 *)((uint32) m);
    vm_message_send_later(tlist, TRUE, id, msg, D_IMMEDIATE, c, CONDITION_WIDTH_16BIT);
}

/*!
  @brief Send a message to be be delivered when the corresponding Task is zero.

  @param t The task to deliver the message to.
  @param id The message identifier.
  @param m The message data.
  @param c The task that must be zero for the message to be delivered.
*/
void MessageSendConditionallyOnTask(Task t, MessageId id, Message m,
                                    const Task *c)
{
    void *msg;
    /* Cast away the const from the message. The messages sent through this
     * interface will not be modified but can't be passed through as const
     * because they have to be queued alongside ones that are not const.
     */
    msg = (uint32 *)((uint32) m);
    vm_message_send_later(&t, FALSE, id, msg, D_IMMEDIATE, c, CONDITION_WIDTH_32BIT);
}

/*!
  @brief Send a message to be be delivered when the corresponding Task is zero.

  @param tlist Pointer to the tasks to deliver the message to.
  @param id The message identifier.
  @param m The message data.
  @param c The task that must be zero for the message to be delivered.
*/
void MessageSendMulticastConditionallyOnTask(Task *tlist, MessageId id, Message m,
                                    const Task *c)
{
    void *msg;
    /* Cast away the const from the message. The messages sent through this
     * interface will not be modified but can't be passed through as const
     * because they have to be queued alongside ones that are not const.
     */
    msg = (uint32 *)((uint32) m);
    vm_message_send_later(tlist, TRUE, id, msg, D_IMMEDIATE, c, CONDITION_WIDTH_32BIT);
}

/*!
  @brief Prevent the VM from automatically freeing a message.

  @param id The message identifier.
  @param m The message data.
 */
void MessageRetain(MessageId id, Message m)
{
    /* Get the message we're currently processing,
       that's the only one we should be retaining. */
    AppMessage *a = vm_delivered_messages;

    /* The application message could be NULL if this is called from outside a VM
       task, for example 'main'.

       If the message pointer is NULL there's no memory to retain. We could
       continue silently as MessageFree() ignores NULL pointers, or fault,
       as we aren't in an unrecoverable state. However, faults are largely
       ignored and retaining a NULL message is likely a bug in the
       application. */
    if(a == NULL || m == NULL || a->id != id || a->message != m)
    {
        panic(PANIC_P1_VM_MESSAGE_RETAIN_BAD_PARAMETERS);
    }

    ++a->refcount;
}

void MessageFree(MessageId id, Message data)
{
    AppMessage **p;

    /* NULL data pointers are ignored, they can't be retained so shouldn't be on
       the queue once message delivery has completed. */
    if(NULL == data)
    {
        return;
    }

    /* Find the matching ID and message pointer and decrement the refcount. */
    for(p = &vm_delivered_messages; *p != NULL; p = &(*p)->next)
    {
        if((*p)->id == id && (*p)->message == data)
        {
            /* The time is only used if this is the last refcount on the message
               which is the case unless this is a multicast message that was
               retained by multiple tasks. */
            uint32 now = get_milli_time();
            app_message_release(p, now);
            return;
        }
    }

    /* If the message is not found free 'data'. Synergy relies on this
       behaviour. */
    pfree(MESSAGE_REMOVE_CONST(data));
}

static void app_message_release(AppMessage **p, MILLITIME now_ms)
{
    AppMessage *a = *p;

    --a->refcount;
    if(a->refcount == 0)
    {
        /* Refcount hit zero, unlink and destroy. */
        *p = (*p)->next;

        trap_api_message_log_now(TRAP_API_LOG_FREE, a, now_ms);
        app_message_destroy(a);
    }
}

static void app_message_destroy(AppMessage *a)
{
    handle_message_free(a->id, a->message);

    if(a->multicast)
    {
        pfree(a->t.tlist);
    }
    pfree(a);
}

#if TRAPSET_STREAM || TRAPSET_OPERATOR

/**
 * Find a entry in the list of sink or source message handlers corresponding
 * to the given Sink or Source
 * @param sink The given Sink
 * @param source The given Source
 * @param create Whether to create a new entry if none found.
 * @return
 */
SINK_SOURCE_HDLR_ENTRY *trap_api_get_sink_source_hdlr_entry(Sink sink,
                                                            Source source,
                                                            bool create)
{
    TRAP_API_SINK_SOURCE_MSG_HDLR **phdlrs;

    phdlrs = &sink_source_hdlrs;

    while(*phdlrs != NULL)
    {
        if ((sink && (*phdlrs)->entry.sink == sink) ||
            (source && (*phdlrs)->entry.source == source))
        {
            return &(*phdlrs)->entry;
        }
        phdlrs = &((*phdlrs)->next);
    }
    if (create)
    {
        *phdlrs = zpnew(TRAP_API_SINK_SOURCE_MSG_HDLR);
        return &(*phdlrs)->entry;
    }
    return NULL;
}

/**
 * Find a entry in the list of sink or source message handlers corresponding
 * to the given Sink or Source and remove it (freeing its memory).
 * The old task is returned to the caller through the \c old_task parameter.
 * @param sink The given Sink
 * @param source The given Source
 * @param old_task Pointer to the location to return the Task structure of
 * the removed task.
 * @return TRUE if an entry has been found and removed. If so, the \c old_task
 * parameter is set with the values from the deleted entry.
 */
static bool trap_api_remove_sink_source_hdlr_entry(Sink sink,
                                                   Source source,
                                                   Task *old_task)
{
    TRAP_API_SINK_SOURCE_MSG_HDLR **phdlrs;

    phdlrs = &sink_source_hdlrs;

    while(*phdlrs != NULL)
    {
        if ((sink && (*phdlrs)->entry.sink == sink) ||
            (source && (*phdlrs)->entry.source == source))
        {
            TRAP_API_SINK_SOURCE_MSG_HDLR * old_handler = *phdlrs;
            *phdlrs = (*phdlrs)->next;
            if(old_task)
            {
                *old_task = old_handler->entry.task;
            }
            pfree(old_handler);
            return TRUE;
        }
        phdlrs = &((*phdlrs)->next);
    }
    return FALSE;
}

/**
 * Find entries in the list of sink or source message handlers
 * corresponding to the given Task and remove them (freeing memory).
 * @param task Remove entries for this task
 */
static void trap_api_remove_sink_source_hdlr_entries_for_task(Task task)
{
    TRAP_API_SINK_SOURCE_MSG_HDLR **phdlrs;

    phdlrs = &sink_source_hdlrs;

    while(*phdlrs != NULL)
    {
        if ((*phdlrs)->entry.task == task)
        {
            TRAP_API_SINK_SOURCE_MSG_HDLR * old_handler = *phdlrs;
            *phdlrs = (*phdlrs)->next;
            pfree(old_handler);
        }
        else
        {
            phdlrs = &((*phdlrs)->next);
        }
    }
}
#endif /* TRAPSET_STREAM || TRAPSET_OPERATOR */

/*
 * From BC vm_trap_core.c
 */
bool MessageCancelFirst(Task task, uint16 id)
{
    struct AppMessage **p = &vm_message_queue;
    bool rc = FALSE;

    while(*p)
    {
        struct AppMessage *a = *p;
        bool valid_tasks = TRUE;

        if (a->id == id)
        {
            if (a->multicast)
            {
                /* Iterate over the task list to see if we want to remove it. */
                Task *tptr = a->t.tlist;

                valid_tasks = FALSE;
                /* Loop through the tasks in the list to find one to cancel */
                while (*tptr != NULL)
                {
                    if (*tptr == task)
                    {
                        *tptr = (Task)INVALIDATED_TASK;
                        rc = TRUE;
                    }

                    if (*tptr != (Task)INVALIDATED_TASK)
                    {
                        valid_tasks = TRUE;
                    }
                    tptr++;
                }
            }

            /* If it's multicast, then t.task can't be task so we don't need
             * to complexify the check.
            */
            if (a->t.task == task || !valid_tasks)
            {
                /* No tasks on this list, cancel/free the message */
                *p = a->next;
                vm_debug_message_cancel(a);
                trap_api_message_log(TRAP_API_LOG_CANCEL, a);
                app_message_destroy(a);
                return TRUE;
            }

        }
        p = &(*p)->next;
    }
    return rc;
}

uint16 MessageCancelAll(Task task, MessageId id)
{
    uint16 count = 0;
    while (MessageCancelFirst(task, id))
    {
        count++;
    }
    return count;
}

/*
 * From BC vm_trap_core.c
 */
uint16 MessageFlushTask(Task task)
{
    uint16 count = 0;
    struct AppMessage **p = &vm_message_queue;

    vm_message_forget(task);

    while(*p)
    {
        struct AppMessage *a = *p;
        bool valid_tasks = TRUE;

        if (a->multicast)
        {
            /* Iterate over the task list for this task */
            Task *tptr = a->t.tlist;

            valid_tasks = FALSE;
            /* Loop through the tasks in the list, deleting ours */
            while (*tptr != NULL)
            {
                if (*tptr == task)
                {
                    *tptr = (Task)INVALIDATED_TASK;
                }

                if (*tptr != (Task)INVALIDATED_TASK)
                {
                    valid_tasks = TRUE;
                }
                tptr++;
            }
        }

        /* Check if we want to flush the message */
        if (a->t.task == task || !valid_tasks)
        {
            *p = a->next;
            vm_debug_message_cancel(a);
            trap_api_message_log(TRAP_API_LOG_CANCEL, a);
            app_message_destroy(a);
            ++count;
        }
        else
        {
            p = &(*p)->next;
        }
    }
    return count;
}

void MessageLoop(void)
{
#ifndef DESKTOP_TEST_BUILD
    /* Do not insert anything between here and the scheduler call */
    L2_DBG_MSG3("MAIN Boot took %d instructions, %d stalls, %d clocks",
            hal_get_reg_num_instrs(), hal_get_reg_num_core_stalls(),
            hal_get_reg_num_run_clks());
#endif

#ifdef SCHEDULER_WITHOUT_RUNLEVELS
    sched();
#else
    sched(RUNLEVEL_FINAL);
#endif

}

#if TRAPSET_STREAM || TRAPSET_OPERATOR
/**
 * Helper function for the various flavours of "register the task against
 * this sink or source" trap
 *
 * \param source The source to register the task against
 * \param sink The sink to register the task against
 * \param task The task to register against the sink or source
 */
static Task trap_api_message_get_stream_task(Sink sink, Source source,
                                             Task task)
{
    SINK_SOURCE_HDLR_ENTRY *entry;
    Task old_task;

    if(!task)
    {
        /*
         * If the task parameter is NULL we need to remove the handler
         * and free up the memory.
         */
        if(trap_api_remove_sink_source_hdlr_entry(sink, source, &old_task))
        {
            return old_task;
        }
        return NULL;
    }

    /* Loop through the list of handlers looking for a matching one, and
     * adding a new one in if not */
    entry = trap_api_get_sink_source_hdlr_entry(sink, source, TRUE);
    old_task = entry->task;
    entry->task = task;
    entry->source = source;
    entry->sink = sink;
    return old_task;

}

/* Note that this function is aliased to MessageOperatorTask because
 * operators are created in the same number space as Sinks. That means
 * that this function has to be present for either trapset.
 */
Task MessageStreamTaskFromSink(Sink sink, Task task)
{
    Source source = (task != NULL) ? StreamSourceFromSink(sink) : NULL;
    return trap_api_message_get_stream_task(sink, source, task);
}

Task MessageStreamTaskFromSource(Source source, Task task)
{
    Sink sink = (task != NULL) ? StreamSinkFromSource(source) : NULL;
    return trap_api_message_get_stream_task(sink, source, task);
}

Task MessageStreamGetTaskFromSink(Sink sink)
{
    SINK_SOURCE_HDLR_ENTRY *entry = trap_api_get_sink_source_hdlr_entry(sink,
                                                                        NULL,
                                                                        FALSE);
    if (entry)
    {
        return entry->task;
    }
    return NULL;
}

Task MessageStreamGetTaskFromSource(Source source)
{
    SINK_SOURCE_HDLR_ENTRY *entry = trap_api_get_sink_source_hdlr_entry(NULL,
                                                                        source,
                                                                        FALSE);
    if (entry)
    {
        return entry->task;
    }
    return NULL;
}

#endif /* TRAPSET_STREAM || TRAPSET_OPERATOR */


/* *************************************************************************
 *  Lower interface
 *************************************************************************** */

void trap_api_send_message_filtered(IPC_MSG_TYPE msg_type,
                                    uint16 id,
                                    void *message,
                                    bool allow_duplicates)
{
    /* Is there a handler for this message channel? */
    Task task = registered_hdlrs[msg_type];

    trap_api_send_message_to_task_filtered(task, id, message, allow_duplicates);
}

void trap_api_send_pio_message_filtered(uint16 group,
                                    uint16 id,
                                    void *message,
                                    bool allow_duplicates)
{
    /* Is there a handler for this message channel? */
    Task task = registered_pio_hdlrs[group];

    trap_api_send_message_to_task_filtered(task, id, message, allow_duplicates);
}

void trap_api_send_message_to_task_filtered(Task task,
                                            uint16 id,
                                            void *message,
                                            bool allow_duplicates)
{
    /* Is there any reason to actually post the message? */
    if (task != NULL && (allow_duplicates || !already(task, id, message)))
    {
        send_message(task, id, message);
    }
    else
    {
        pfree(message);
    }
}

#if TRAPSET_STREAM || TRAPSET_OPERATOR
void trap_api_send_sink_source_message_filtered(uint16 stream_id,
                                                uint16 id,
                                                void *message,
                                                bool allow_duplicates)
{
    /* Is there a handler for this message channel? And if there is, is there
     * any reason to actually post the message? */
    SINK_SOURCE_HDLR_ENTRY *entry;
    Sink sink = SINK_FROM_ID(stream_id);

    /* Find entry by checking for sink first */
    entry = trap_api_get_sink_source_hdlr_entry(sink, NULL, FALSE);
    if (entry == NULL)
    {
        /* Check for source as well */
        Source source = SOURCE_FROM_ID(stream_id);
        entry = trap_api_get_sink_source_hdlr_entry(NULL, source, FALSE);
    }

    if (entry != NULL &&
            (allow_duplicates || !already(entry->task, id, message)))
    {
        send_message(entry->task, id, message);
    }
    else
    {
        handle_message_free(id, message);
    }
}
#endif /* TRAPSET_STREAM || TRAPSET_OPERATOR */

Task MessageSystemTask(Task task)
{
    return trap_api_register_message_task(task, IPC_MSG_TYPE_SYSTEM);
}

#if TRAPSET_NFC
Task MessageNfcTask(Task task)
{
    return trap_api_register_message_task(task, IPC_MSG_TYPE_NFC);
}
#endif /* TRAPSET_NFC */

uint16 MessagesPendingForTask(Task task, int32 *first_due)
{
    AppMessage *p;
    uint16 count = 0;

    for (p = vm_message_queue; p; p = p->next)
    {
        Task *tptr = NULL;

        if (p->multicast)
        {
            /* Iterate over the task list for this task */
            tptr = p->t.tlist;

            while (*tptr != NULL)
            {
                if (*tptr == task)
                {
                    break;
                }
                tptr++;
            }
            if (*tptr == NULL)
            {
                tptr = NULL;
            }
        }

        if (p->t.task == task || tptr != NULL)
        {
            if (!count && first_due)
            {
                uint32 now  = get_milli_time();
                *first_due = VM_DIFF(p->due, now);
            }
            ++count;
        }
    }
    return count;
}

bool MessagePendingFirst(Task task, MessageId id, int32 *first_due)
{
    AppMessage *p;
    for (p = vm_message_queue; p; p = p->next)
    {
        Task *tptr = NULL;

        if (p->multicast)
        {
            /* Iterate over the task list for this task */
            tptr = p->t.tlist;

            while (*tptr != NULL)
            {
                if (*tptr == task)
                {
                    break;
                }
                tptr++;
            }
            if (*tptr == NULL)
            {
                tptr = NULL;
            }
        }

        if ((p->t.task == task || tptr != NULL) && p->id == id)
        {
            if (first_due)
            {
                uint32 now  = get_milli_time();
                *first_due = VM_DIFF(p->due, now);
            }
            return TRUE;
        }
    }
    return FALSE;
}

uint16 MessagePendingMatch(Task task, bool once, MessageMatchFn match_fn)
{
    AppMessage *p;
    uint16 count = 0;

    if (match_fn == NULL)
    {
        return 0;
    }

    for (p = vm_message_queue; p; p = p->next)
    {
        Task *tptr = NULL;

        if (p->multicast)
        {
            /* Iterate over the task list for this task */
            tptr = p->t.tlist;

            while (*tptr != NULL)
            {
                if (*tptr == task)
                {
                    break;
                }
                tptr++;
            }
            if (*tptr == NULL)
            {
                tptr = NULL;
            }
        }

        if (p->t.task == task || tptr != NULL)
        {
            if (match_fn(task, p->id, p->message))
            {
                count++;
                if (once)
                {
                    break;
                }
            }
        }
    }
    return count;
}

Task trap_api_register_message_group_task(Task task, uint16 group)
{
    UNUSED(task);
    UNUSED(group);
    return NULL;
}

