#ifndef SCHED_SUBSYSTEM_H_
#define SCHED_SUBSYSTEM_H_
/* Autogenerated scheduler task header created by fw/tools/make/header_autogen.py */

#include "core/appcmd/appcmd_sched.h"
#include "core/fault/fault_sched.h"
#include "core/ipc/ipc_sched.h"
#include "core/piodebounce/piodebounce_sched.h"
#include "customer/core/trap_api/trap_api_sched.h"

#define SCHED_TASK_LIST(m)\
 CORE_APPCMD_SCHED_TASK(m) \
 CORE_FAULT_SCHED_TASK(m) \
 CORE_IPC_SCHED_TASK(m) \
 CORE_PIODEBOUNCE_SCHED_TASK(m) \
 CUSTOMER_CORE_TRAP_API_SCHED_TASK(m)

#endif /* SCHED_SUBSYSTEM_H_ */
