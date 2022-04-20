/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_aec.c
    \brief The chain_aec chain.

    This file is generated by C:\Qualcomm_Prog\QCC514x_304x_Earbud\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#include <chain_aec.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include <hydra_macros.h>
#include <../earbud_cap_ids.h>
#include <kymera_chain_roles.h>
static const operator_config_t operators[] =
{
    MAKE_OPERATOR_CONFIG(CAP_ID_AEC_REF, OPR_AEC),
} ;

static const operator_endpoint_t inputs[] =
{
    {OPR_AEC, EPR_AEC_INPUT1, 0},
    {OPR_AEC, EPR_AEC_INPUT2, 1},
    {OPR_AEC, EPR_AEC_MIC1_IN, 2},
    {OPR_AEC, EPR_AEC_MIC2_IN, 3},
    {OPR_AEC, EPR_AEC_MIC3_IN, 4},
    {OPR_AEC, EPR_AEC_MIC4_IN, 5},
} ;

static const operator_endpoint_t outputs[] =
{
    {OPR_AEC, EPR_AEC_REFERENCE_OUT, 0},
    {OPR_AEC, EPR_AEC_SPEAKER1_OUT, 1},
    {OPR_AEC, EPR_AEC_SPEAKER2_OUT, 2},
    {OPR_AEC, EPR_AEC_MIC1_OUT, 3},
    {OPR_AEC, EPR_AEC_MIC2_OUT, 4},
    {OPR_AEC, EPR_AEC_MIC3_OUT, 5},
    {OPR_AEC, EPR_AEC_MIC4_OUT, 6},
} ;

const chain_config_t chain_aec_config = {1, 0, operators, 1, inputs, 6, outputs, 7, NULL, 0};

