/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
#ifndef TIMED_EVENT_H_
#define TIMED_EVENT_H_

/**
 * \file
 * Shim layer redirecting clients to the appropriate implementation module
 */

#if defined(OS_OXYGOS) || defined(OS_FREERTOS)
#include "timed_event_oxygen/timed_event_oxygen.h"
#else
#include "timed_event_carlos/timed_event_carlos.h"
#endif

#endif /* TIMED_EVENT_H_ */
