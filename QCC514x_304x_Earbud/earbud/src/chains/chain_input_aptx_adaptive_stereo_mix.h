/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_input_aptx_adaptive_stereo_mix.h
    \brief The chain_input_aptx_adaptive_stereo_mix chain.

    This file is generated by C:\Qualcomm_Prog\QCC514x_304x_Earbud\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#ifndef _CHAIN_INPUT_APTX_ADAPTIVE_STEREO_MIX_H__
#define _CHAIN_INPUT_APTX_ADAPTIVE_STEREO_MIX_H__

/*!
\page chain_input_aptx_adaptive_stereo_mix
    @startuml
        object OPR_RTP_DECODER
        OPR_RTP_DECODER : id = CAP_ID_RTP_DECODE
        object OPR_APTX_ADAPTIVE_DECODER
        OPR_APTX_ADAPTIVE_DECODER : id = EB_CAP_ID_APTX_ADAPTIVE_DECODE
        OPR_APTX_ADAPTIVE_DECODER "IN(0)"<-- "OUT(0)" OPR_RTP_DECODER
        object EPR_SINK_MEDIA #lightgreen
        OPR_RTP_DECODER "IN(0)" <-- EPR_SINK_MEDIA
        object EPR_SOURCE_DECODED_PCM #lightblue
        EPR_SOURCE_DECODED_PCM <-- "OUT(0)" OPR_APTX_ADAPTIVE_DECODER
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_input_aptx_adaptive_stereo_mix_config_p0;

extern const chain_config_t chain_input_aptx_adaptive_stereo_mix_config_p1;

#endif /* _CHAIN_INPUT_APTX_ADAPTIVE_STEREO_MIX_H__ */

