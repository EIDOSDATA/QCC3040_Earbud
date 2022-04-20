/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_output_volume_mono.h
    \brief The chain_output_volume_mono chain.

    This file is generated by C:\Qualcomm_Prog\QCC514x_304x_Earbud\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#ifndef _CHAIN_OUTPUT_VOLUME_MONO_H__
#define _CHAIN_OUTPUT_VOLUME_MONO_H__

/*!
\page chain_output_volume_mono
    @startuml
        object OPR_SOURCE_SYNC
        OPR_SOURCE_SYNC : id = CAP_ID_SOURCE_SYNC
        object OPR_VOLUME_CONTROL
        OPR_VOLUME_CONTROL : id = CAP_ID_OUTPUT_VOL_CTRL
        OPR_VOLUME_CONTROL "MAIN_IN(0)"<-- "OUT(0)" OPR_SOURCE_SYNC
        object EPR_SINK_MIXER_MAIN_IN #lightgreen
        OPR_SOURCE_SYNC "IN(0)" <-- EPR_SINK_MIXER_MAIN_IN
        object EPR_VOLUME_AUX #lightgreen
        OPR_VOLUME_CONTROL "AUX_IN(1)" <-- EPR_VOLUME_AUX
        object EPR_SOURCE_MIXER_OUT #lightblue
        EPR_SOURCE_MIXER_OUT <-- "OUT(0)" OPR_VOLUME_CONTROL
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_output_volume_mono_config;

#endif /* _CHAIN_OUTPUT_VOLUME_MONO_H__ */

