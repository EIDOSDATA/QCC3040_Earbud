/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The audio_router c type definitions. This file is generated by C:/Qualcomm_Prog/qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _AUDIO_ROUTER_TYPEDEF_H__
#define _AUDIO_ROUTER_TYPEDEF_H__

#include <csrtypes.h>
#include <marshal_common.h>
#include <task_list.h>
#include <audio_router.h>

#include "audio_router.h"

#define MAX_NUM_SOURCES ((max_audio_sources - 1) + (max_voice_sources - 1))

/*! Audio router data structure */
typedef struct 
{
    /*! Array of audio source info */
    audio_router_data_t data[MAX_NUM_SOURCES];
    /*! Most recently routed audio source */
    audio_source_t last_routed_audio_source;
} audio_router_data_container_t;

#endif /* _AUDIO_ROUTER_TYPEDEF_H__ */
