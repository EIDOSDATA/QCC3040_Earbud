/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */

#include "trap_api/trap_api_private.h"
#include "pmalloc/pmalloc.h"
#include "longtimer/longtimer.h"

/**
 * Number of elements in circular message log buffer.
 * Must be a power of 2
 */
#define MSG_LOG_BUF_SIZE    2048U
#define MSG_LOG_BUF_MASK    (MSG_LOG_BUF_SIZE - 1)

#define DELIMITER_MASK      0xffffff00UL

#define LOG_MSG_ELEMENT(x) log_msg_element((const uint8 *)&(x), (uint16)sizeof(x))

#define FAST_LOG_MSG_ELEMENT(v, addr) \
    do { \
        *addr = /*lint -e961 */*(const uint8 *)&(v); \
        if (/*lint -e774 */sizeof(v) > 1)  *(addr+1) = /*lint -e961*/*((const uint8 *)&(v)+1); \
        if (/*lint -e774 */sizeof(v) > 2)  *(addr+2) = /*lint -e961*/*((const uint8 *)&(v)+2); \
        if (/*lint -e774 */sizeof(v) > 3)  *(addr+3) = /*lint -e961*/*((const uint8 *)&(v)+3); \
        addr += sizeof(v); \
    } while (0)

/**
 * A circular message log buffer, its current logging poisition, and its
 * current start position (used to handle wrap around of the circular buffer).
 * These will be accessed (read-only) from off-chip.
 */
static uint8  trap_msg_log[MSG_LOG_BUF_SIZE];
static uint16 trap_msg_log_pos = 0;
static uint16 trap_msg_log_start = 0;

/* Arbitrary value used to ensure we get record alignment. */
static uint32 delimiter = 0x10910900UL;
static uint16 seq_num = 0;

/**
 * log_msg_start
 *   Checks that there is space for the message in the buffer, and removes old
 * messages to fit the new one in.
 *
 * Updates trap_msg_log_start
 */
static inline void log_msg_start(unsigned int length)
{
    unsigned int max_before_start;
    unsigned int log_msg_len;

    /* Advance the trap_msg_log_start to make room for the message we're about
     * to write to the buffer. This allows us to be atomic from the Python pov,
     * and also minimise the calculations during writing elements.
     */
    max_before_start = (trap_msg_log_start - trap_msg_log_pos) & MSG_LOG_BUF_MASK;

    /* Need to advance the start, throwing away the oldest message(s) */
    while ((length > max_before_start) && (trap_msg_log_start != trap_msg_log_pos))
    {
        /* We 'know' (ick) that at the start of a message is a uint16 of its length,
         * not including that uint16.
        */
        log_msg_len = sizeof(uint16) + (trap_msg_log[trap_msg_log_start] |
                      (trap_msg_log[(trap_msg_log_start + 1) & MSG_LOG_BUF_MASK] << 8));
        trap_msg_log_start = (trap_msg_log_start + log_msg_len) & MSG_LOG_BUF_MASK;
        max_before_start += log_msg_len;
    }
}


/**
 * log_msg_element
 *   Writes the given element into the buffer, taking care of wrapping.
 */
static void log_msg_element(const uint8 *data, unsigned int length, unsigned int offset)
{
    unsigned int max_before_wrap;

    /* Check the wrap to see how much we can write in one go. */
    max_before_wrap = MSG_LOG_BUF_SIZE - offset;

    /* Now we know there's enough room to write the new message element */
    if (length > max_before_wrap)
    {
        memcpy(&trap_msg_log[offset], data, max_before_wrap);
        memcpy(&trap_msg_log[0], data + max_before_wrap, length - max_before_wrap);
    }
    else
    {
        memcpy(&trap_msg_log[offset], data, length);
    }
}

#ifdef OS_OXYGOS
/**
 * Log the state of a message.
 * \param action: what is happening to the message.
 * \param msg: the AppMessage struture for the message.
 */
void trap_api_message_log_now(TRAP_API_LOG_ACTION action, AppMessage *msg, uint32 now)
{
    uint16  rec_len;
    uint16  msg_len = 0;
    uint32  null_value=0;
    uint8   type = (uint8) action;
    Task   *tptr;
    unsigned int total_len;

    /*
     * With the following constant calculation, give the compiler the
     * opportunity to work it out at compile time, or failing that it
     * should only get calculated once if done at run time.
     */
    static const uint16 fixed_size  = sizeof(rec_len)
                                    + sizeof(delimiter)
                                    + sizeof(seq_num)
                                    + sizeof(now)
                                    + sizeof(type)      /* action */
                                    + sizeof(msg->id)
                                    + sizeof(msg->condition_addr)
                                    + sizeof(msg->due)
                                    + sizeof(msg_len)
                                    + sizeof(msg->t)
                                    + sizeof(msg->t.task->handler);
                                    /* Don't bother logging the msg->c_width */

    /*
     * The length of the pmalloc block holding the message payload can be
     * determined from the size of the pool that has been allocated. This
     * indicates the maximum possible size for the payload. Note that in some
     * messages, the payload was allocated in P0, so psizeof will fail(safe)
     * returning zero. pfree knows about P0 memory, so all is well in the end.
     */
    if (NULL != msg->message)
    {
        msg_len = (uint16)psizeof((const void *) msg->message);
    }

    /*
     * Calculate the length of the data to be written to the circular buffer,
     * and the actual record length (doesn't incl the length of rec_len itself).
     */
    total_len = (uint16)(fixed_size + msg_len);

    delimiter = delimiter & DELIMITER_MASK;

    if (msg->multicast)
    {
        /* Update the delimiter and increase the overall length */
        delimiter |= msg->multicast;
        total_len += (msg->multicast - 1) * (sizeof(msg->t)+sizeof(msg->t.task->handler));
    }

    rec_len = (uint16)(total_len - sizeof(rec_len));

    log_msg_start(total_len);

    if ((total_len-msg_len) < (MSG_LOG_BUF_SIZE - trap_msg_log_pos))
    {
        /* Quick code for the fixed-size data, and the task/handlers.
         * It's not going to wrap, so it's a straight linear copy from A to B.
         */
        uint8  *bufpos;

        bufpos = &(trap_msg_log[trap_msg_log_pos]);

        FAST_LOG_MSG_ELEMENT(rec_len, bufpos);
        FAST_LOG_MSG_ELEMENT(delimiter, bufpos);
        FAST_LOG_MSG_ELEMENT(seq_num, bufpos);
        FAST_LOG_MSG_ELEMENT(now, bufpos);
        FAST_LOG_MSG_ELEMENT(type, bufpos);
        if (msg->multicast)
        {
            tptr = msg->t.tlist;
            while (*tptr != NULL)
            {
                FAST_LOG_MSG_ELEMENT(*tptr, bufpos);
                if (*tptr != (Task)INVALIDATED_TASK)
                    FAST_LOG_MSG_ELEMENT((*tptr)->handler, bufpos);
                else
                    FAST_LOG_MSG_ELEMENT(null_value, bufpos);
                tptr++;
            }
        }
        else
        {
            FAST_LOG_MSG_ELEMENT(msg->t.task, bufpos);
            if(msg->t.task)
                FAST_LOG_MSG_ELEMENT(msg->t.task->handler, bufpos);
            else
                FAST_LOG_MSG_ELEMENT(null_value, bufpos);
        }
        FAST_LOG_MSG_ELEMENT(msg->id, bufpos);
        FAST_LOG_MSG_ELEMENT(msg->condition_addr, bufpos);
        FAST_LOG_MSG_ELEMENT(msg->due, bufpos);
        FAST_LOG_MSG_ELEMENT(msg_len, bufpos);
    }
    else
    {
        /* (fast) copy the data into a temporary buffer, which is then written
         * (slowly) to the log buffer as one 'element'
         */
#ifdef DESKTOP_TEST_BUILD
        uint8   tmp_buf[40]; /* Worst case of 4octets/element. Because -ansi barfs in unit tests :( */
#else
        uint8   tmp_buf[fixed_size + (MAX_MULTICAST_RECIPIENTS * (sizeof(msg->t)+sizeof(msg->t.task->handler)))];
#endif
        uint8  *bufpos;

        bufpos = tmp_buf;

        FAST_LOG_MSG_ELEMENT(rec_len, bufpos);
        FAST_LOG_MSG_ELEMENT(delimiter, bufpos);
        FAST_LOG_MSG_ELEMENT(seq_num, bufpos);
        FAST_LOG_MSG_ELEMENT(now, bufpos);
        FAST_LOG_MSG_ELEMENT(type, bufpos);
        if (msg->multicast)
        {
            tptr = msg->t.tlist;
            while (*tptr != NULL)
            {
                FAST_LOG_MSG_ELEMENT(*tptr, bufpos);
                if (*tptr != (Task)INVALIDATED_TASK)
                    FAST_LOG_MSG_ELEMENT((*tptr)->handler, bufpos);
                else
                    FAST_LOG_MSG_ELEMENT(null_value, bufpos);
                tptr++;
            }
        }
        else
        {
            FAST_LOG_MSG_ELEMENT(msg->t.task, bufpos);
            if(msg->t.task)
                FAST_LOG_MSG_ELEMENT(msg->t.task->handler, bufpos);
            else
                FAST_LOG_MSG_ELEMENT(null_value, bufpos);
        }
        FAST_LOG_MSG_ELEMENT(msg->id, bufpos);
        FAST_LOG_MSG_ELEMENT(msg->condition_addr, bufpos);
        FAST_LOG_MSG_ELEMENT(msg->due, bufpos);
        FAST_LOG_MSG_ELEMENT(msg_len, bufpos);

        log_msg_element((const uint8 *)tmp_buf, (total_len-msg_len), trap_msg_log_pos);
    }
    if (msg_len > 0)
    {
        /* Size unknown at compile-time, so may wrap */
        log_msg_element((const uint8 *)msg->message, msg_len,
                        (trap_msg_log_pos + (total_len-msg_len)) & MSG_LOG_BUF_MASK);
    }

    trap_msg_log_pos = (trap_msg_log_pos + total_len) & MSG_LOG_BUF_MASK;

    seq_num++;
}

#endif /* OS_OXYGOS */

#ifdef OS_FREERTOS
#define MSG_HEADER_SIZE_BYTES (offsetof(trap_msg_header_t, _sentinel))

void trap_api_multitask_message_log(trap_msg_header_t *header, Message msg)
{
    size_t msg_len = psizeof(msg);
    unsigned int total_len = MSG_HEADER_SIZE_BYTES + msg_len;

    /* Record Length */
    U16_OFF0_SET(header->record_length, total_len - sizeof(uint16));

    /* Delimiter */
    U32_OFF2_SET(header->delimiter, delimiter);

    /* Current Time (ms) */
    {
        uint32 now_ms = get_milli_time();
        U32_OFF2_SET(header->now_ms, now_ms);
    }

    /* Message Length */
    U16_OFF1_SET(header->msg_len, msg_len);

    /* Block interrupts must be kept as short as possible. This duration
       currently depends on the message length, at 32MHz the duration doesn't
       exceed 100us until the logged messages are hundreds of bytes long. */
    block_interrupts();
    {
        /* Sequence Number - Must be updated whilst interrupts are blocked. */
        U16_OFF0_SET(header->seq_num, seq_num);
        ++seq_num;

        /* Update read offset */
        log_msg_start(total_len);

        /* Write Header */
        log_msg_element((const uint8 *)header, MSG_HEADER_SIZE_BYTES,
                        trap_msg_log_pos);

        /* Write Message */
        log_msg_element(msg, msg_len,
                (trap_msg_log_pos + MSG_HEADER_SIZE_BYTES) & MSG_LOG_BUF_MASK);

        /* Update write offset. */
        trap_msg_log_pos = (trap_msg_log_pos + total_len) & MSG_LOG_BUF_MASK;
    }
    unblock_interrupts();
}

#endif /* OS_FREERTOS */
