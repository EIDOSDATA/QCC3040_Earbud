/****************************************************************************
 * COMMERCIAL IN CONFIDENCE
* Copyright (c) 2010 - 2017 Qualcomm Technologies International, Ltd.
 *
 *
 *************************************************************************/
/*
 * \file boot.c
 * \brief Startup code
 * MODULE : Boot
 *
 */

/****************************************************************************
Include Files
*/
#if defined(INSTALL_HTOL) || defined(RUNNING_ON_KALSIM)
#include "audio.h"
#endif /* INSTALL_HTOL || RUNNING_ON_KALSIM */
#include "sched_oxygen/sched_oxygen.h"
#include "pmalloc/pl_malloc.h"
#include "platform/pl_error.h"
#include "pl_timers/pl_timers.h"
#include "platform/profiler_c.h"
#include "hal.h"
#include "id/id.h"
#include "capability_database.h"
#if defined(INSTALL_CLK_MGR)
#include "clk_mgr/clk_mgr.h"
#endif
#ifdef INSTALL_HTOL
#include "htol.h"
#endif
#include "fault/fault.h"
#ifdef INSTALL_PIO_SUPPORT
#include "pio.h"
#endif
#include "platform/pl_interrupt.h"
#ifdef INSTALL_HYDRA

#if defined(RUNNING_ON_KALSIM) && defined(FIRMWARE_TALKING_TO_KALCMD)
#include "kalsim_msgif.h"
#endif /* RUNNING_ON_KALSIM && FIRMWARE_TALKING_TO_KALCMD */
#include "submsg/submsg.h"
#include "hydra_sssm/sssm_ss.h"
#include "hydra_mmu_buff.h"
#endif /* INSTALL_HYDRA */
#ifdef CHIP_BASE_BC7
#include "bc_comms.h"
#include "kalimba_messages.h"
#endif /* CHIP_BASE_BC7 */
#ifdef INSTALL_CAP_DOWNLOAD_MGR
#include "malloc_pm/malloc_pm.h"
#include "cap_download_mgr.h"
#endif
#ifdef INSTALL_ROM_TEST
#include "rom_test.h"
#endif
#ifdef INSTALL_FILE_MGR
#include "file_mgr.h"
#endif /* INSTALL_FILE_MGR */

#ifdef INSTALL_AUDIO_DATA_SERVICE
#include "audio_data_service.h"
#endif

#if defined(SUPPORTS_MULTI_CORE)
#include "ipc/ipc_heap_config.h"
#include "kip_mgr/kip_mgr.h"
#endif

#ifndef NONSECURE_PROCESSING
#include "../../lib_private/security/security_library_c.h"
#endif /* NONSECURE_PROCESSING */
#include "sys_events.h"
#include "preserved/preserved.h"

#ifdef INSTALL_AOV
#include "aov_task.h"
#endif

/****************************************************************************
Private Macro Declarations
*/
/* The HTOL rig has to set TEST_REG_0 to this value in order to put us into
 * HTOL mode. */
#define DO_HTOL_SIGNATURE 0x13579B
/****************************************************************************
Extern declarations
*/
/* This shouldn't really be global so isn't in the public header */
extern uint24 slt_fingerprint;
extern char PM_RAM_PATCH_CODE_START_HEADER;

#if defined(SUPPORTS_MULTI_CORE)
/**
 * \brief sub main function for second cores
 *        hal_init() is done common at main() and rest will be
 *        initialised here.
 *
 */
static int cpux_main(void)
{
    /* initialise KiP mgr. The kip mgr would be
     * calling ipc_register_comms and registering the
     * processor with ipc.
     */
    kip_init();

    /* Enable error handlers for Kalimba libs.
     * These all end up in panic so probably not useful to
     * do this any earlier. */
    error_enable_exception_handlers(TRUE);

    /**
     * Initialise the fault system here because on P0 it is initialised by Hydra
     * SSSM which does not run other on P1.
     */
    init_fault();

#ifndef NONSECURE_PROCESSING
    /* On Px, all security requests will be forwarded to P0 */
    security_init();
#endif /* NONSECURE_PROCESSING */

    /**
     * Profiler initialisation creates a scheduled_event to update the profiler
     * every sec.
     */
     PROFILER_INIT();

    /** Run Scheduler */
    sched(RUNLEVEL_BOOT);

    return 0;

}
#else
#define cpux_main() 0
#endif /* defined(SUPPORTS_MULTI_CORE) */

/**
 * \brief  Startup entry function, which initialises & runs platform scheduler
 *
 * \return Ideally scheduler should run for ever(!).
 *
 * Initialises IRQ and scheduler and runs the scheduler.
 */
int main(void)
{
    heap_config *pheap_config = NULL;

    /* Common hardware initialisation */
    hal_init();

    /* Initialise IRQs and the scheduler */
    interrupt_initialise();

#ifdef INSTALL_CLK_MGR
    clk_mgr_init();
#endif /* INSTALL_CLK_MGR */

#if defined(SUPPORTS_MULTI_CORE)
    /*
    *  This must be called before init_pmalloc to
    *  initialise the IPC LUT. The pmalloc uses LUT
    *  to share heap information between processors
    */
    ipc_init();

    if (PROC_SECONDARY_CONTEXT())
    {
        /* Retrieve pointer to processor heap info list from LUT */
        pheap_config = ipc_lut_get_heap_config();
        /* NULL is handled in 'ipc_lut_get_heap_config' */
    }
#endif

    /* Initialise malloc; NULL parameter is expected for P0. */
    init_pmalloc(pheap_config);

    /* Init scheduler */
    init_sched();
    init_pl_timers();
    init_preserved();

#ifdef INSTALL_CAP_DOWNLOAD_MGR
    cap_download_mgr_init();
    capability_database_init_download_list();
#endif /* INSTALL_CAP_DOWNLOAD_MGR */

    if (PROC_SECONDARY_CONTEXT())
    {
        return cpux_main();
    }
    /* All code below executes only on the primary */


#ifdef INSTALL_PIO_SUPPORT
    /* Initialise PIO system - done early incase the
       rest of the boot depends on pio state */
    init_pio();
#endif

#ifndef NONSECURE_PROCESSING
    security_init();
#endif /* NONSECURE_PROCESSING */


    init_system_event();

#if defined(CHIP_BASE_HYDRA)
    /* Initialise the MMU hardware */
    mmu_buffer_initialise();
#endif /* CHIP_BASE_HYDRA */

// for BC7 we need some XAP <-> DSP messaging in order to be able to handle any ports comms, use asm for now
#ifdef CHIP_BASE_BC7
   // initialise the message library
   bc_message_initialise();
#endif /* CHIP_BASE_BC7 */

#ifdef INSTALL_HTOL
    /* Should we do HTOL instead of running normally?
     * We figure this out by comparing TEST_REG_0 with the HTOL signature.
     * The register is zeroed (by hardware) when the CPU is reset, so in order
     * for this test to pass, we must have had some external influence.
     * TODO: In future, we will start HTOL under direct instruction from the
     * Curator (probably via a service request); this mechanism is only temporary. */
    if (hal_get_reg_test_reg_0() == DO_HTOL_SIGNATURE)
    {
        /* We need to initialise the audio hardware here
         * In a normal boot this happens later, after any config file download
         * but we don't get that far if doing HTOL
         */
        init_audio();
        hal_set_reg_test_reg_0(0);
        put_message(HTOL_QUEUE_ID, MSG_HTOL_START, NULL);
    }
    else
#endif /* INSTALL_HTOL */
    {
#ifdef INSTALL_HYDRA

#if defined(RUNNING_ON_KALSIM) && defined(FIRMWARE_TALKING_TO_KALCMD)
        kalcmd_configure_communication();
#else
        /* Normal Hydra subsystem initialisation */
        submsg_init();
#endif /* RUNNING_ON_KALSIM && FIRMWARE_TALKING_TO_KALCMD */
        /* This just queues a message to start the SSSM process off.
         * Nothing substantial happens (e.g., patch download) until the
         * scheduler actually starts running (sched() below). */
        sssm_init();
#ifdef INSTALL_AUDIO_DATA_SERVICE
        audio_data_service_init();
#endif /* INSTALL_AUDIO_DATA_SERVICE */
#elif !defined(INSTALL_PM_HEAP_SHARE) && defined(INSTALL_CAP_DOWNLOAD_MGR)
    /* Platforms that have malloc_pm, all of PM can be used by the PM heap */
    init_malloc_pm();
#endif /* INSTALL_HYDRA */
    }

#ifdef INSTALL_FILE_MGR
    file_mgr_init();
#endif

#ifdef INSTALL_AOV
    aov_init();
#endif

#ifndef TODO_CRESCENDO_CHIPVER_CHECKING
    /* Check that we're running on the hardware this code was built for
     * We don't bother doing this on ROM builds - we really should get it right
     * and if not there's not much we can do about it once it's in a ROM.
     */
#ifdef INSTALL_LPC_SUPPORT
    {
        int minor_version = hal_get_chip_version_minor();

        if (minor_version != CHIP_BUILD_VER)
        {
            /* For builds targetting "latest" hardware, assume it's OK
             * and don't fault if the version is later than expected
             * (if it isn't OK, there should be a new "latest" build).
             */
            if ((CHIP_BUILD_VER != CHIP_BUILD_VER_LATEST) ||
                (minor_version < CHIP_BUILD_VER))
            {
                fault_diatribe(FAULT_AUDIO_WRONG_HARDWARE_VERSION,
                               hal_get_reg_sub_sys_chip_version());
            }
        }
    }
#endif /* INSTALL_LPC_SUPPORT */
#endif /* TODO_CRESCENDO_CHIPVER_CHECKING */

    /* Enable error handlers for Kalimba libs.
     * These all end up in panic so probably not useful to do this any earlier. */
    error_enable_exception_handlers(TRUE);

    /* Profiler initialisation creates a scheduled_event to update the profiler every sec. */
    PROFILER_INIT();

    /* Run scheduler. We're still not ready until the SSSM has finished, though.
     * Any init code that needs to communicate with other subsystems should be
     * called from sssm_init_operational(). */
    sched(RUNLEVEL_BOOT);

    /* Scheduler isn't expected to return... */

#ifdef INSTALL_ROM_TEST
    /* Shouldn't get here, but we need to reference the ROM performance
     * test code to get it linked in */
    rom_performance_test();
#endif

    /* TODO: This is a shortcut to make sure we reference the SLT.*/
    return(slt_fingerprint);

}
