/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/****************************************************************************
FILE
    longtimer.c  -  64 bit timer library

CONTAINS
    update_long_timer    -  update the long timer to the current time
        update_event         -  timed event to check for clock wrap.
    get_long_milli_time  -  get a 64 bit millisecond time.
*/

#include "longtimer/longtimer_private.h"
#include "int/int.h"

static bool init; /* = FALSE */
static longtimer timer;

#define WAKEUP_PERIOD (10 * MINUTE)

/****************************************************************************
NAME
    update_long_timer  -  update the long timer to the current time

    This function must be called within a critical section on platforms where
    the long timer can be queried from tasks that can pre-empt one another.
*/

static void update_long_timer(void)
{
    uint32 t;

    t = (uint32) get_time();
    if ((uint32) t < uint64_lo32(timer)) /* Note: unsigned comparison *not* time_lt() */
        timer = uint64_add(timer, uint64_from32x2(1,0));
    /* Set the lower 32 bits of timer to the contents of t */
    timer = uint64_from32x2(uint64_hi32(timer), t);
}

/****************************************************************************
NAME
    update_event  - timed event to check for clock wrap
*/

/* ARGSUSED */
static void update_event(uint16 m, void *p)
{
    UNUSED(m);
    UNUSED(p);

    /* This function is executed in a casual timer callback, i.e. in a
       background interrupt so update needs to happen in a critical section. */
    block_interrupts();
    {
        update_long_timer();
    }
    unblock_interrupts();
    (void)timed_event_in(WAKEUP_PERIOD,update_event,0,NULL);
}

/****************************************************************************
NAME
    get_long_milli_time  -  get a 64 bit millisecond time
*/

void get_long_milli_time(longtimer *t)
{
    /* Initialisation needs to be in a critical section as multiple tasks could
       call this for the first time at the same time. */
    block_interrupts();
    {
        if (!init)
        {
            /* initialisation.
               this is nice as we only start the timed event
               if someone is actually using longtimer! */
            init = TRUE;
            timer = uint64_from32(get_time());
            (void)timed_event_in(WAKEUP_PERIOD,update_event,0,NULL);
        }

        update_long_timer();
    }
    unblock_interrupts();

    *t = uint64_div16(timer, MILLISECOND);
}

/* BCHS_EXPORT_POINT_END */
