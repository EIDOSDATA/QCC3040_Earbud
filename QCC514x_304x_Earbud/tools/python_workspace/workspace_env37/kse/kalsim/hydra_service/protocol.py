#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra Control and Configuration protocol for kalsim firmware"""

import logging

from kats.framework.library.log import log_input, log_output, log_input_output
from .constants import SYSTEM_BUS_APPS_SYS, START_SERVICE_REQ, START_SERVICE_RESP, \
    STATUS_OK, STOP_SERVICE_REQ, STOP_SERVICE_RESP, PANIC_REPORT, FAULT_REPORT, \
    SET_SCO_PARAMS_REQ, SET_SCO_PARAMS_RESP, BUS_INT_STATUS, INT_SOURCE_TBUS_MSG_ADPTR_EVENT, \
    SSSM_SERVICE_ADVICE_IND, FTP_SERV_TO_FW_CTRL, FTP_SERV_TO_FW_DATA, LMGR_SERVICE_START_RESP, \
    LMGR_LICENSE_QUERY_RESP, SET_ISO_PARAMS_REQ, SET_ISO_PARAMS_RESP

TIMER_KICK_COMPLETED = 0.0005


class HydraProtocol:
    """Hydra Messaging Protocol handler

    Args:
        uut (kats.core.uut_base.UutBase): Uut abstraction instance
        subsystem (int): Subsystem ID
    """

    def __init__(self, uut, subsystem=SYSTEM_BUS_APPS_SYS):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        self.uut = uut
        self._uut = uut
        self._subsystem = subsystem
        self._kick_pending = []
        self._timer = None
        self._header = self._uut.message_register_handler([subsystem])

    @log_input(logging.DEBUG)
    def _timer_callback(self, _id):
        self._timer = None

        if self._can_kick() and self._kick_pending:
            self._kick()

    @log_output(logging.DEBUG)
    def _can_kick(self):
        return self._timer is None
        # return self._uut.mem_peek(1, BUS_INT_STATUS, 4) == 0

    def _kick(self):
        status_to_kick = self._kick_pending.pop(0)
        self._uut.mem_poke(1, BUS_INT_STATUS, 4, status_to_kick)
        self._uut.interrupt(INT_SOURCE_TBUS_MSG_ADPTR_EVENT)
        self._timer = self._uut.timer_add_relative(TIMER_KICK_COMPLETED, self._timer_callback)

    def install_message_handler(self, callback, flt=None):
        """Install message handler for hydra services

        If filter is provided then it will be evaluated and the handler only called if the
        filter returns True

        Args:
            callback (func(list[int])): Message reception callback
            flt (func): Filtering function receiving one parameter being the message received and
                returning True if the message is accepted in the filter, False otherwise

        Returns:
            dict: Message handler
        """
        return self._uut.message_register_handler([self._subsystem], callback=callback, flt=flt)

    def uninstall_message_handler(self, handler):
        """Unregister previously register accmd messsage handler

        Args:
            handler (dict): Message handler
        """
        return self._uut.message_unregister_handler(handler)

    @log_input_output(logging.INFO, logging.INFO,
                      formatters={'service_type': '0x%04x', 'payload': '0x%04x'})
    def start_service(self, service_type, payload=None):
        """Start a hydra service

        Args:
            service_type (int): Service type
            payload (list[int]): Specific service payload data

        Returns:
            tuple:
                int: Service tag
                list[int]: Service payload
        """
        if not payload:
            payload = []
        msg = [START_SERVICE_REQ, service_type, len(payload)] + payload

        response = self._uut.message_send_recv(self._header, msg)
        if len(response) < 4:
            raise RuntimeError('start_service response length:%s invalid' % (len(response)))
        if response[0] != START_SERVICE_RESP:
            raise RuntimeError('start_service response cmd_id:%04x invalid' % (response[0]))
        if response[2] != STATUS_OK:
            raise RuntimeError('start_service response status:%s invalid' % (response[2]))
        if response[3] != len(response[4:]):
            raise RuntimeError('start_service response length:%s does not match message %s' %
                               (response[3], response[4:]))

        service_tag = response[1]
        payload = response[4:]
        return service_tag, payload

    @log_input(logging.INFO, formatters={'service_type': '0x%04x'})
    def stop_service(self, service_type, service_tag):
        """Stop a hydra service

        Args:
            service_type (int): Service type
            service_tag int): Service tag obtained in start_service

        Raises:
            RuntimeError if the stop_service response is invalid
        """
        msg = [STOP_SERVICE_REQ, service_type, service_tag]
        response = self._uut.message_send_recv(self._header, msg)
        if len(response) != 3:
            raise RuntimeError('stop_service response length:%s invalid' % (len(response)))
        if response[0] != STOP_SERVICE_RESP:
            raise RuntimeError('stop_service response cmd_id:%04x invalid' % (response[0]))
        if response[1] != service_tag:
            raise RuntimeError('stop_service response service tag:%s invalid' % (response[1]))
        if response[2] != STATUS_OK:
            raise RuntimeError('stop_service response status:%s invalid' % (response[2]))

    @log_input(logging.INFO)
    def set_sco_params(self, service_tag, **kwargs):
        """Set SCO parameters for service instance identified by service_tag

        Args:
            service_tag (int): Service tag obtained in start_service
            tesco (uint16): SPS BT link TeSCO, the interval between transmissions in slots
            wesco (uint16): SPS BT link WeSCO, the retransmission window in slots
            to_air_length (uint16): SPS to-air packet length in octets
            from_air_length (uint16): SPS from-air packet length in octets
            next_slot_time (uint32): BT clock of first packet to be received from-air
                                     (this is received in a field of metadata)
            to_air_latency (int): interval to/from the start of first reserved slot when the audio
                                  graph should have data ready for transmission in microseconds
            from_air_latency (int): interval to/from the start of first reserved slot when data
                                    should be ready for the audio graph to process in microseconds

        Raises:
            RuntimeError if the set_sco_params response is invalid
        """
        msg = [
            SET_SCO_PARAMS_REQ,
            service_tag,
            8,
            kwargs['tesco'],
            kwargs['wesco'],
            kwargs['to_air_length'],
            kwargs['from_air_length'],
            (kwargs['next_slot_time'] >> 16) & 0xFFFF,
            kwargs['next_slot_time'] & 0xFFFF,
            kwargs['to_air_latency'],
            kwargs['from_air_latency'],
        ]
        response = self._uut.message_send_recv(self._header, msg)
        if len(response) != 3:
            raise RuntimeError('set_sco_params response length:%s invalid' % (len(response)))
        if response[0] != SET_SCO_PARAMS_RESP:
            raise RuntimeError('set_sco_params response cmd_id:%04x invalid' % (response[0]))
        if response[1] != service_tag:
            raise RuntimeError('set_sco_params response service tag:%s invalid' % (response[1]))
        if response[2] != STATUS_OK:
            raise RuntimeError('set_sco_params response status:%s invalid' % (response[2]))

    @log_input(logging.INFO)
    def set_iso_params(self, service_tag, **kwargs):
        """Set ISO parameters for service instance identified by service_tag

        Args:
            service_tag (int): Service tag obtained in start_service
            iso_interval (uint16): Fundamental interval between air events, in BT slot-pairs
                (units of 1.25ms). [valid values 4-3200]
            sdu_interval_to_air (uint32): SDU interval for data being transmitted, in microseconds
                [20 bits significant]
            sdu_interval_from_air (uint32): SDU interval for data being received, in microseconds
                [20 bits significant]
            max_sdu_to_air (uint16): maximum SDU length, in octets [uint16, 12 bits significant]
            max_sdu_from_air (uint16): maximum SDU length, in octets [uint16, 12 bits significant]
            next_slot_time (uint32): next reserved slot time in BT ticks (312.5 us)
            stream_sync_delay (uint32): CIS/BIS Synchronisation Delay in microseconds
            group_sync_delay (uint32): CIG/BIG Synchronisation Delay in microseconds
            flush_timeout_to_air (uint16): Flush timeout for data being tx, in ISO intervals
            flush_timeout_from_air (uint16): Flush timeout for data being received, in ISO intervals
            to_air_latency (uint32): latency in the to-air direction, in microseconds
            from_air_latency (uint32): latency in the from-air direction, in microseconds
            sdu_number_to_air (uint16): sequence number of first SDU, integer (unused for now,
                leave 0)
            sdu_number_from_air (uint16): sequence number of first SDU, integer (unused for now,
                leave 0)

        Raises:
            RuntimeError if the set_iso_params response is invalid
        """
        msg = [
            SET_ISO_PARAMS_REQ,
            service_tag,
            21,
            kwargs['iso_interval'],
            (kwargs['sdu_interval_to_air'] >> 16) & 0xFFFF,
            kwargs['sdu_interval_to_air'] & 0xFFFF,
            (kwargs['sdu_interval_from_air'] >> 16) & 0xFFFF,
            kwargs['sdu_interval_from_air'] & 0xFFFF,
            kwargs['max_sdu_to_air'],
            kwargs['max_sdu_from_air'],
            (kwargs['next_slot_time'] >> 16) & 0xFFFF,
            kwargs['next_slot_time'] & 0xFFFF,
            (kwargs['stream_sync_delay'] >> 16) & 0xFFFF,
            kwargs['stream_sync_delay'] & 0xFFFF,
            (kwargs['group_sync_delay'] >> 16) & 0xFFFF,
            kwargs['group_sync_delay'] & 0xFFFF,
            kwargs['flush_timeout_to_air'],
            kwargs['flush_timeout_from_air'],
            (kwargs['to_air_latency'] >> 16) & 0xFFFF,
            kwargs['to_air_latency'] & 0xFFFF,
            (kwargs['from_air_latency'] >> 16) & 0xFFFF,
            kwargs['from_air_latency'] & 0xFFFF,
            kwargs['sdu_number_to_air'],
            kwargs['sdu_number_from_air'],
        ]
        response = self._uut.message_send_recv(self._header, msg)
        if len(response) != 3:
            raise RuntimeError('set_iso_params response length:%s invalid' % (len(response)))
        if response[0] != SET_ISO_PARAMS_RESP:
            raise RuntimeError('set_iso_params response cmd_id:%04x invalid' % (response[0]))
        if response[1] != service_tag:
            raise RuntimeError('set_iso_params response service tag:%s invalid' % (response[1]))
        if response[2] != STATUS_OK:
            raise RuntimeError('set_iso_params response status:%s invalid' % (response[2]))

    @log_input(logging.INFO)
    def send_service_advice_indication(self):
        """Send service advice indication"""
        self._uut.message_send(self._header, [SSSM_SERVICE_ADVICE_IND])

    @log_input(logging.INFO, formatters={'payload': '0x%04x'})
    def send_ftp_server_to_fw_control(self, payload):
        """Send ftp server message control path

        Args:
            payload (list[int]): Message to send
        """
        self._uut.message_send(self._header, [FTP_SERV_TO_FW_CTRL] + payload)

    @log_input(logging.INFO, formatters={'payload': '0x%04x'})
    def send_ftp_server_to_fw_data(self, payload):
        """Send ftp server message data path

        Args:
            payload (list[int]): Message to send
        """
        self._uut.message_send(self._header, [FTP_SERV_TO_FW_DATA] + payload)

    @log_input(logging.INFO,
               formatters={'ccp_header': '0x%04x', 'service_tag': '0x%04x', 'payload': '0x%04x'})
    def send_license_start_response(self, ccp_header, service_tag, payload=None):
        """Start response from license manager

        Args:
            ccp_header (int): CCP Header
            service_tag (int): Service tag
            payload (list[int]): Message to send
        """
        payload = [] if not payload else payload
        self._uut.message_send(self._header,
                               [LMGR_SERVICE_START_RESP, ccp_header, service_tag] + payload)

    @log_input(logging.INFO,
               formatters={'ccp_header': '0x%04x', 'service_tag': '0x%04x', 'status': '0x%04x',
                           'payload': '0x%04x'})
    def send_license_query_response(self, ccp_header, service_tag, status, payload):
        """Response to license query from license manager

        Args:
            ccp_header (int): CCP Header
            service_tag (int): Service tag
            status (int): License status
            payload (list[int]): Message to send
        """
        self._uut.message_send(self._header,
                               [LMGR_LICENSE_QUERY_RESP, ccp_header, service_tag, status] + payload)

    @log_input(logging.INFO)
    def kick(self, status_to_kick):
        """Kick firmware

        Args:
            status_to_kick (int): Buffer to kick
        """
        if not status_to_kick in self._kick_pending:
            self._kick_pending.append(status_to_kick)
        else:
            self._log.warning('trying to kick without previous acknowledgement')

        if self._can_kick():
            self._kick()

    @log_input_output(logging.INFO, formatters={'msg': '0x%04x'})
    def send(self, msg):
        """Send a generic hydra service message

        Args:
            msg (list[int]): Message payload
        """
        self._uut.message_send(self._header, msg)

    @log_input_output(logging.INFO, formatters={'msg': '0x%04x', 'return': '0x%04x'})
    def send_recv(self, msg):
        """Send a generic hydra service message and get a response

        Args:
            msg (list[int]): Message payload

        Returns:
            list[int]: Message payload
        """
        return self._uut.message_send_recv(self._header, msg)

    @log_output(logging.INFO, formatters={'return': '0x%04x'})
    def recv(self):
        """Receive a hydra service message

        Returns:
            list[int]: Message payload
        """
        response = self._uut.message_recv(self._header)
        if response[0] == PANIC_REPORT:
            raise RuntimeError('received firmware panic report id:0x%04x' % response[1])
        if response[0] == FAULT_REPORT:
            raise RuntimeError('received firmware fault report id:0x%04x' % response[1])
        return response
