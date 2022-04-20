/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The mirror_profile c type definitions. This file is generated by C:/Qualcomm_Prog/qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _MIRROR_PROFILE_TYPEDEF_H__
#define _MIRROR_PROFILE_TYPEDEF_H__

#include <csrtypes.h>
#include <kymera_adaptation_voice_protected.h>
#include <marshal_common.h>

/*! HFP volume update sent from Primary to Secondary */
typedef struct 
{
    /*! Voice source */
    uint8 voice_source;
    /*! HFP volume */
    uint8 volume;
} mirror_profile_hfp_volume_ind_t;

/*! HFP codec type and volume sent from Primary to Secondary */
typedef struct 
{
    /*! Voice source */
    uint8 voice_source;
    /*! HFP codec */
    hfp_codec_mode_t codec_mode;
    /*! HFP volume */
    uint8 volume;
} mirror_profile_hfp_codec_and_volume_ind_t;

/*! A2DP volume update sent from Primary to Secondary */
typedef struct 
{
    /*! Audio source */
    uint8 audio_source;
    /*! A2DP volume */
    uint8 volume;
} mirror_profile_a2dp_volume_ind_t;

/*! A2DP media stream context sent from Primary to Secondary */
typedef struct 
{
    /*! The BT address */
    bdaddr addr;
    /*! L2CAP CID */
    uint16 cid;
    /*! L2CAP maximum transmission unit */
    uint16 mtu;
    /*! A2DP volume */
    uint8 volume;
    /*! Defines the current audio state - disconnected, connected, active */
    uint8 audio_state;
    /*! Negotiated AVDTP SEID (stream endpoing ID)  */
    uint8 seid;
    uint8 codec_sub_type;
    /*! Codec bitrate bps */
    uint32 bitrate;
    /*! Audio sample rate (/25Hz) */
    uint16 sample_rate;
    /*! Audio sample bit width */
    uint8 sample_width;
    /*! Audio channel type */
    uint8 channel_type;
    /*! Audio source */
    uint8 audio_source;
    /*! Other flags */
    uint8 flags;
    /*! Content protection type */
    uint16 content_protection_type;
    /*! Length of extra CPI (always 0) */
    uint8 content_protection_information_length;
    /*! Q2Q Mode */
    uint8 q2q_mode;
    /*! Split Mode */
    uint32 aptx_features;
} mirror_profile_stream_context_t;

/*! A2DP media stream context response sent from Secondary to Primary */
typedef struct 
{
    /*! L2CAP CID */
    uint16 cid;
    /*! Negotiated AVDTP SEID (stream endpoing ID)  */
    uint8 seid;
} mirror_profile_stream_context_response_t;

/*! Secondary sends this message to primary to tell it when to unmute during a mute-sync start */
typedef struct 
{
    /*! The time at which to unmute */
    marshal_rtime_t unmute_time;
} mirror_profile_sync_a2dp_unmute_ind_t;

/*! Secondary sends this message to primary to tell it when to unmute during a SCO mute-sync start */
typedef struct 
{
    /*! The time at which to unmute */
    marshal_rtime_t unmute_time;
} mirror_profile_sync_sco_unmute_ind_t;

/*! Unicast audio stream context sent from Primary to Secondary */
typedef struct 
{
    /*! Audio source */
    uint8 audio_source;
    /*! Voice source */
    uint8 voice_source;
    /*! Audio context */
    uint16 audio_context;
    /*! CIG identifier */
    uint8 cig_id;
    /*! CIS identifier */
    uint8 cis_id;
    /*! Sink ase identifier */
    uint8 sink_ase_id;
    /*! Sink target latency */
    uint8 sink_target_latency;
    /*! Sink target phy */
    uint8 sink_target_phy;
    /*! Sink codec identifier */
    uint8 sink_codec_id;
    /*! Sink sample frequency */
    uint16 sink_sampling_frequency;
    /*! Sink frame duration */
    uint16 sink_frame_duration;
    /*! Sink sdu interval */
    uint32 sink_sdu_interval;
    /*! Sink framing */
    uint8 sink_framing;
    /*! Sink phy */
    uint8 sink_phy;
    /*! Sink max sdu size */
    uint16 sink_max_sdu_size;
    /*! Sink retransmission number */
    uint8 sink_retransmission_num;
    /*! Sink max transport latency */
    uint16 sink_max_transport_latency;
    /*! Sink presentation dalay */
    uint32 sink_presentation_delay;
    /*! Source ase identifier */
    uint8 source_ase_id;
    /*! Source target latency */
    uint8 source_target_latency;
    /*! Source target phy */
    uint8 source_target_phy;
    /*! Source codec identifier */
    uint8 source_codec_id;
    /*! Source sample frequency */
    uint16 source_sampling_frequency;
    /*! Source frame duration */
    uint16 source_frame_duration;
    /*! Source sdu interval */
    uint32 source_sdu_interval;
    /*! Source framing */
    uint8 source_framing;
    /*! Source phy */
    uint8 source_phy;
    /*! Source max sdu size */
    uint16 source_max_sdu_size;
    /*! Source retransmission number */
    uint8 source_retransmission_num;
    /*! Source max transport latency */
    uint16 source_max_transport_latency;
    /*! Source presentation dalay */
    uint32 source_presentation_delay;
} mirror_profile_lea_unicast_audio_conf_req_t;

#endif /* _MIRROR_PROFILE_TYPEDEF_H__ */
