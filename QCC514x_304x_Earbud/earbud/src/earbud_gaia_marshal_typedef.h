/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The earbud_gaia marshal type declarations. This file is generated by C:/Qualcomm_Prog/QCC514x_304x_Earbud/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _EARBUD_GAIA_MARSHAL_TYPEDEF_H__
#define _EARBUD_GAIA_MARSHAL_TYPEDEF_H__

#include <csrtypes.h>
#include <app/marshal/marshal_if.h>


#define EARBUD_GAIA_MARSHAL_TYPES_TABLE(ENTRY)\
    ENTRY(earbud_gaia_request_t)\
    ENTRY(earbud_gaia_response_t)

#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum EARBUD_GAIA_MARSHAL_TYPES
{
    EARBUD_GAIA_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_EARBUD_GAIA_MARSHAL_TYPES
} ;

#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const earbud_gaia_marshal_type_descriptors[];
#endif /* _EARBUD_GAIA_MARSHAL_TYPEDEF_H__ */

