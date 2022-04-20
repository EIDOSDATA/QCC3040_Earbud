/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The mirror_profile marshal type declarations. This file is generated by C:/Qualcomm_Prog/QCC514x_304x_Earbud/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _MIRROR_PROFILE_MARSHAL_TYPEDEF_H__
#define _MIRROR_PROFILE_MARSHAL_TYPEDEF_H__

#include <csrtypes.h>
#include <kymera_adaptation_voice_protected.h>
#include <app/marshal/marshal_if.h>
#include <marshal_common.h>


#define MIRROR_PROFILE_MARSHAL_TYPES_TABLE(ENTRY)\
    ENTRY(mirror_profile_hfp_volume_ind_t)\
    ENTRY(mirror_profile_hfp_codec_and_volume_ind_t)\
    ENTRY(mirror_profile_a2dp_volume_ind_t)\
    ENTRY(mirror_profile_stream_context_t)\
    ENTRY(mirror_profile_stream_context_response_t)\
    ENTRY(mirror_profile_sync_a2dp_unmute_ind_t)\
    ENTRY(mirror_profile_sync_sco_unmute_ind_t)\
    ENTRY(mirror_profile_lea_unicast_audio_conf_req_t)

#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum MIRROR_PROFILE_MARSHAL_TYPES
{
    DUMMY = (NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1),
    MIRROR_PROFILE_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MIRROR_PROFILE_MARSHAL_TYPES
} ;

#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const mirror_profile_marshal_type_descriptors[];
#endif /* _MIRROR_PROFILE_MARSHAL_TYPEDEF_H__ */

