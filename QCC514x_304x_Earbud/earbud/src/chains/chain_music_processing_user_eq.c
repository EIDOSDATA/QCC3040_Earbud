/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \file chain_music_processing_user_eq.c
    \brief The chain_music_processing_user_eq chain.

    This file is generated by C:\Qualcomm_Prog\QCC514x_304x_Earbud\adk\tools\packages\chaingen\chaingen_mod\__init__.pyc.
*/

#include <chain_music_processing_user_eq.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include <hydra_macros.h>
#include <../earbud_cap_ids.h>
#include <kymera_chain_roles.h>
static const operator_config_t operators_p0[] =
{
    MAKE_OPERATOR_CONFIG(CAP_ID_BASIC_PASS, OPR_ADD_HEADROOM),
    MAKE_OPERATOR_CONFIG(CAP_ID_PEQ, OPR_SPEAKER_EQ),
    MAKE_OPERATOR_CONFIG(CAP_ID_PEQ, OPR_USER_EQ),
    MAKE_OPERATOR_CONFIG(CAP_ID_BASIC_PASS, OPR_REMOVE_HEADROOM),
} ;

static const operator_config_t operators_p1[] =
{
    MAKE_OPERATOR_CONFIG_P1(CAP_ID_BASIC_PASS, OPR_ADD_HEADROOM),
    MAKE_OPERATOR_CONFIG_P1(CAP_ID_PEQ, OPR_SPEAKER_EQ),
    MAKE_OPERATOR_CONFIG_P1(CAP_ID_PEQ, OPR_USER_EQ),
    MAKE_OPERATOR_CONFIG_P1(CAP_ID_BASIC_PASS, OPR_REMOVE_HEADROOM),
} ;

static const operator_endpoint_t inputs[] =
{
    {OPR_ADD_HEADROOM, EPR_MUSIC_PROCESSING_IN_L, 0},
} ;

static const operator_endpoint_t outputs[] =
{
    {OPR_REMOVE_HEADROOM, EPR_MUSIC_PROCESSING_OUT_L, 0},
} ;

static const operator_connection_t connections[] =
{
    {OPR_ADD_HEADROOM, 0, OPR_SPEAKER_EQ, 0, 1},
    {OPR_SPEAKER_EQ, 0, OPR_USER_EQ, 0, 1},
    {OPR_USER_EQ, 0, OPR_REMOVE_HEADROOM, 0, 1},
} ;

const chain_config_t chain_music_processing_user_eq_config_p0 = {0, 0, operators_p0, 4, inputs, 1, outputs, 1, connections, 3};

const chain_config_t chain_music_processing_user_eq_config_p1 = {0, 0, operators_p1, 4, inputs, 1, outputs, 1, connections, 3};

