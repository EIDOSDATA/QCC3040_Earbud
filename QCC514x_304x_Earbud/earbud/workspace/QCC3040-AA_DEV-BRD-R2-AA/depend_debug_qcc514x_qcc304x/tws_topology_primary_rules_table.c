/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The tws_topology_primary rule table definitions. This file is generated by rulegen.py.
*/

#include <rules_engine.h>
#include <tws_topology_common_primary_rule_functions.h>
#include <tws_topology_twm_primary_rule_functions.h>
#include <tws_topology_rule_events.h>
#include <tws_topology_primary_ruleset.h>
#include <tws_topology_goals.h>

const size_t tws_topology_primary_rules_count = 36;

/*! TWS Topology rules deciding behaviour in a Primary role. */
const rule_entry_t tws_topology_primary_rules[] =
{
    /*! When we are shutting down, disconnect everything. */
    RULE(TWSTOP_RULE_EVENT_SHUTDOWN, ruleTwsTopPriShutDown, tws_topology_goal_system_stop),

    /*! Decide if peer pairing should run. */
    RULE(TWSTOP_RULE_EVENT_NO_PEER, ruleTwsTopPriPairPeer, tws_topology_goal_pair_peer),

    /*! Decide if Primary address should be set, when peer pairing completes whilst in the case. */
    RULE(TWSTOP_RULE_EVENT_PEER_PAIRED, ruleTwsTopPriPeerPairedInCase, tws_topology_goal_set_address),

    /*! Decide if Primary address should be set and role selection started, when peer pairing completes out of the case. */
    RULE(TWSTOP_RULE_EVENT_PEER_PAIRED, ruleTwsTopPriPeerPairedOutCase, tws_topology_goal_set_primary_address_and_find_role),

    /*! When Primary role has been selected, decide if switch to become Primary role should be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_PRIMARY, ruleTwsTopPriSelectedPrimary, tws_topology_goal_become_primary),

    /*! When Secondary role has been selected, decide if switch to become Secondary role should be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY, ruleTwsTopPriNoRoleSelectedSecondary, tws_topology_goal_become_secondary),

    /*!  When Acting Primary role has been selected, decide if switch to become Acting Primary role should be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_ACTING_PRIMARY, ruleTwsTopPriSelectedActingPrimary, tws_topology_goal_become_acting_primary),

    /*! When an Earbud in an acting primary role has received decision to be secondary, decide if the switch to secondary role should be executed. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SELECTED_SECONDARY, ruleTwsTopPriPrimarySelectedSecondary, tws_topology_goal_role_switch_to_secondary),

    /*! After switching to primary role, decide if Primary should enable page scan for Secondary to connect. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH, ruleTwsTopPriEnableConnectablePeer, tws_topology_goal_primary_connectable_peer),

    /*! After peer BREDR connection established, decide if Primary should disable page scan for Secondary. */
    RULE(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR, ruleTwsTopPriDisableConnectablePeer, tws_topology_goal_primary_connectable_peer),

    /*! When peer BREDR link established by Secondary, decide which profiles to connect. */
    RULE(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR, ruleTwsTopPriConnectPeerProfiles, tws_topology_goal_primary_connect_peer_profiles),

    /*! When peer BREDR link established by Secondary, but we have since entered case, release the peer link (dont wait for disconnect as other rules handle). */
    RULE(TWSTOP_RULE_EVENT_PEER_CONNECTED_BREDR, ruleTwsTopPriReleasePeer, tws_topology_goal_release_peer),

    /*! When the peer BREDR link is disconnected, ensure the peer profiles are inactive by disconnecting them. */
    RULE(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR, ruleTwsTopPriDisconnectPeerProfiles, tws_topology_goal_primary_disconnect_peer_profiles),

    /*! When handset BREDR link is lost, decide if Primary Earbud should attempt to reconnect. */
    RULE(TWSTOP_RULE_EVENT_HANDSET_LINKLOSS, ruleTwsTopPriHandsetLinkLossReconnect, tws_topology_goal_connect_handset),

    /*! When role switch complete, decide if connection manager should accept handset connections. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH, ruleTwsTopPriAllowHandsetConnect, tws_topology_goal_allow_handset_connect),

    /*! When role switch completed, decide if LE advertisement should be enabled for handset connections. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH, ruleTwsTopPriEnableLeConnectableHandset, tws_topology_goal_le_connectable_handset),

    /*! Connect handset after role switch to Primary */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH, ruleTwsTopPriRoleSwitchConnectHandset, tws_topology_goal_connect_handset),

    /*! Decide what to do after handset connection has been prohibited */
    RULE(TWSTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET, ruleTwsTopPriDisconnectHandset, tws_topology_goal_disconnect_handset),

    /*! Connect handset requested by topology user */
    RULE(TWSTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET, ruleTwsTopPriUserRequestConnectHandset, tws_topology_goal_connect_handset),

    /*! Disconnect all handsets requested by topology user */
    RULE(TWSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS, ruleTwsTopPriDisconnectHandset, tws_topology_goal_disconnect_handset),

    /*! Start watchdog timer */
    RULE(TWSTOP_RULE_EVENT_IN_CASE, ruleTwsTopPriInCaseWatchdogStart, tws_topology_goal_start_watchdog),

    /*! Start watchdog timer */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_CLOSED, ruleTwsTopPriInCaseWatchdogStart, tws_topology_goal_start_watchdog),

    /*! Stop watchdog timer */
    RULE(TWSTOP_RULE_EVENT_OUT_CASE, ruleTwsTopPriOutOfCaseWatchdogStop, tws_topology_goal_stop_watchdog),

    /*! Stop watchdog timer */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_OPEN, ruleTwsTopPriOutOfCaseWatchdogStop, tws_topology_goal_stop_watchdog),

    /*! Disconnect LRU handset requested by topology user */
    RULE(TWSTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET, ruleTwsTopPriDisconnectLruHandset, tws_topology_goal_disconnect_lru_handset),

    /*! When Earbud is taken out of the case, decide if role selection should be executed. */
    RULE(TWSTOP_RULE_EVENT_OUT_CASE, ruleTwsTopTwmPriFindRole, tws_topology_goal_find_role),

    /*! When case is opened, decide if role selection should be executed. */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_OPEN, ruleTwsTopTwmPriFindRole, tws_topology_goal_find_role),

    /*! When peer BREDR link is lost, decide if Primary Earbud should start role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_LINKLOSS, ruleTwsTopPriPeerLostFindRole, tws_topology_goal_primary_find_role),

    /*! When peer BREDR link is disconnected, decide if Primary Earbud should start role selection. */
    RULE(TWSTOP_RULE_EVENT_PEER_DISCONNECTED_BREDR, ruleTwsTopPriPeerLostFindRole, tws_topology_goal_primary_find_role),

    /*! When Secondary fails to connect BREDR ACL after role selection, decide if Primary Earbud should restart role selection. */
    RULE(TWSTOP_RULE_EVENT_FAILED_PEER_CONNECT, ruleTwsTopPriPeerLostFindRole, tws_topology_goal_primary_find_role),

    /*! When Primary Earbud goes in the case, decide if links should be dropped and it should transition to no role. */
    RULE(TWSTOP_RULE_EVENT_IN_CASE, ruleTwsTopTwmPriNoRoleIdle, tws_topology_goal_no_role_idle),

    /*! Case lid is closed, decide if links should be dropped and it should transition to no role. */
    RULE(TWSTOP_RULE_EVENT_CASE_LID_CLOSED, ruleTwsTopTwmPriNoRoleIdle, tws_topology_goal_no_role_idle),

    /*! When role switch completed, decide if page scan should be enabled for handset connections. */
    RULE(TWSTOP_RULE_EVENT_ROLE_SWITCH, ruleTwsTopPriEnableConnectableHandset, tws_topology_goal_connectable_handset),

    /*! After HDMA recommendation, decide whether to initiate Handover. */
    RULE(TWSTOP_RULE_EVENT_HANDOVER, ruleTwsTopTwmHandoverStart, tws_topology_goal_dynamic_handover),

    /*! If Handover fails and connect_handset was stopped for handover then resume connect_handset to Primary. */
    RULE(TWSTOP_RULE_EVENT_HANDOVER_FAILED, ruleTwsTopPriRoleSwitchConnectHandset, tws_topology_goal_connect_handset),

    /*! Decide if need to find the role after no role is set */
    RULE(TWSTOP_RULE_EVENT_NO_ROLE, ruleTwsTopTwmPriFindRole, tws_topology_goal_find_role),

} ;
