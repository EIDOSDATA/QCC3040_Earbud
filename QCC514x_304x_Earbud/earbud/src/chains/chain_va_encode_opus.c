/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_va_encode_opus.c
    \brief The chain_va_encode_opus chain.

    This file is generated by C:\Qualcomm_Prog\QCC514x_304x_Earbud\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#include <chain_va_encode_opus.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include <hydra_macros.h>
#include <../earbud_cap_ids.h>
#include <kymera_chain_roles.h>
static const operator_config_t operators[] =
{
    MAKE_OPERATOR_CONFIG(CAP_ID_DOWNLOAD_OPUS_CELT_ENCODE, OPR_OPUS_ENCODER),
} ;

static const operator_endpoint_t inputs[] =
{
    {OPR_OPUS_ENCODER, EPR_VA_ENCODE_IN, 0},
} ;

static const operator_endpoint_t outputs[] =
{
    {OPR_OPUS_ENCODER, EPR_VA_ENCODE_OUT, 0},
} ;

const chain_config_t chain_va_encode_opus_config = {1, 0, operators, 1, inputs, 1, outputs, 1, NULL, 0};

