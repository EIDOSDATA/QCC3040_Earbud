/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The tws_topology_secondary rule table definitions. This file is generated by rulegen.py.
*/

#include <rules_engine.h>
#include <tws_topology_common_secondary_rule_functions.h>
#include <tws_topology_rule_events.h>
#include <tws_topology_secondary_ruleset.h>
#include <tws_topology_goals.h>

const size_t tws_topology_secondary_rules_count = 12;

/*! Table of rules for a TWS TWM earbud in the Secondary role */
const rule_entry_t tws_topology_secondary_rules[] =
{
    /*! When we are shutting down, disconnect everything. */
    RULE(TWSTOP_RULE_EVENT_SHUTDOWN, ruleTwsTopSecShutDown, tws_topology_goal_system_stop),

    /*! If failed to connect the ACL to the Primary, decide if Secondary should return to no role and restart role selection. */
    RULE(TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT, ruleTwsTopSecFailedConnectFindRole, tws_topology_goal_no_role_find_role),

    /*! After switching to secondary role, decide if Secondary Earbud should create BREDR link to Primary Earbud. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH, ruleTwsTopSecRoleSwitchPeerConnect, tws_topology_goal_secondary_connect_peer),

    /*! When Earbud put in the case, decide if it should drop links and transition to no role */
    RULE(TWSTOP_RULE_EVENT_IN_CASE, ruleTwsTopSecNoRoleIdle, tws_topology_goal_no_role_idle),

    /*! When lid is closed, decide if it should drop links and transition to no role */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_CLOSED, ruleTwsTopSecNoRoleIdle, tws_topology_goal_no_role_idle),

    /*! If failed to switch to being a secondary, decide if device should return to no role and restart role selection. */
    RULE(TWSTOP_RULE_EVENT_FAILED_SWITCH_SECONDARY, ruleTwsTopSecFailedSwitchSecondaryFindRole, tws_topology_goal_no_role_find_role),

    /*! Start watchdog timer */
    RULE(TWSTOP_RULE_EVENT_IN_CASE, ruleTwsTopSecInCaseWatchdogStart, tws_topology_goal_start_watchdog),

    /*! Start watchdog timer */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_CLOSED, ruleTwsTopSecInCaseWatchdogStart, tws_topology_goal_start_watchdog),

    /*! Stop watchdog timer */
    RULE(TWSTOP_RULE_EVENT_OUT_CASE, ruleTwsTopSecOutOfCaseWatchdogStop, tws_topology_goal_stop_watchdog),

    /*! Stop watchdog timer */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_OPEN, ruleTwsTopSecOutOfCaseWatchdogStop, tws_topology_goal_stop_watchdog),

    /*! On linkloss with Primary Earbud, decide if Secondary should run role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_LINKLOSS, ruleTwsTopSecPeerLostFindRole, tws_topology_goal_no_role_find_role),

    /*! On BREDR disconnect from Primary Earbud, decide if Secondary should run role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR, ruleTwsTopSecPeerLostFindRole, tws_topology_goal_no_role_find_role),

} ;

