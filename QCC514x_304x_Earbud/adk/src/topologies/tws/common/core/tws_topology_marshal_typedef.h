/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The tws_topology marshal type declarations. This file is generated by C:/Qualcomm_Prog/QCC514x_304x_Earbud/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _TWS_TOPOLOGY_MARSHAL_TYPEDEF_H__
#define _TWS_TOPOLOGY_MARSHAL_TYPEDEF_H__

#include <csrtypes.h>
#include <app/marshal/marshal_if.h>
#include <marshal_common.h>


#define TWS_TOPOLOGY_MARSHAL_TYPES_TABLE(ENTRY)\
    ENTRY(tws_topology_remote_rule_event_t)

#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum TWS_TOPOLOGY_MARSHAL_TYPES
{
    DUMMY = (NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1),
    TWS_TOPOLOGY_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_TWS_TOPOLOGY_MARSHAL_TYPES
} ;

#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const tws_topology_marshal_type_descriptors[];
#endif /* _TWS_TOPOLOGY_MARSHAL_TYPEDEF_H__ */

