/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_va_wuw_gva.h
    \brief The chain_va_wuw_gva chain.

    This file is generated by C:\Qualcomm_Prog\QCC514x_304x_Earbud\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#ifndef _CHAIN_VA_WUW_GVA_H__
#define _CHAIN_VA_WUW_GVA_H__

/*!
\page chain_va_wuw_gva
    @startuml
        object OPR_WUW
        OPR_WUW : id = CAP_ID_VA_GVA
        object EPR_VA_WUW_IN #lightgreen
        OPR_WUW "IN(0)" <-- EPR_VA_WUW_IN
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_va_wuw_gva_config;

#endif /* _CHAIN_VA_WUW_GVA_H__ */

