#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra Data Services Constants"""

SYSTEM_BUS_APPS_SYS = 4

# hydra commands
START_SERVICE_REQ = 0x2001  # to request kymera to start a service
STOP_SERVICE_REQ = 0x2002  # to request kymera to stop a service
SET_SCO_PARAMS_REQ = 0x2003  # to set sco service parameters
KICK_SERVICE_CONSUMER = 0x2004
PANIC_REPORT = 0x2006  # from kymera when a panic happens
FAULT_REPORT = 0x2007  # from kymera when a fault happens
AUDIO_DATA_SERVICE_AUX_MSG = 0x2008
SCO_PROC_SERV_RUN_STATE_MSG = 0x2009  # from kymera when sco stream is ready
SSSM_SERVICE_ADVICE_IND = 0x200a  # to indicate kymera that the hydra ftp server is available
FTP_SERV_TO_FW_CTRL = 0x200b  # to send kymera a ftp control message
FTP_SERV_TO_FW_DATA = 0x200c  # to send kymera ftp data
LMGR_SERVICE_START_RESP = 0x200d  # response from host to audio - LMGR Service start response
LMGR_LICENSE_QUERY_RESP = 0x200e  # response from host to audio - LMGR license query response
SET_ISO_PARAMS_REQ = 0x200f  # to set iso service parameters

# hydra responses
START_SERVICE_RESP = 0x6001  # from kymera response to START_SERVICE_REQ
STOP_SERVICE_RESP = 0x6002  # from kymera response to STOP_SERVICE_REQ
SET_SCO_PARAMS_RESP = 0x6003  # from kymera response SET_SCO_PARAM_REQ
FTP_FW_TO_SERV_CTRL = 0x6004  # from kymera ftp message
FTP_FW_TO_SERV_DATA = 0x6005  # from kymera ftp data acknowledgement
CCP_SIGNAL_ID_OPERATIONAL_IND = 0x6006  # from kymera it is ready (after ftp transaction)
LMGR_SERVICE_START_REQ = 0x6007  # query from audio to host - LMGR Service start request
LMGR_LICENSE_QUERY_REQ = 0x6008  # query from audio to host - LMGR license query request
SET_ISO_PARAMS_RESP = 0x6009  # from kymera response SET_ISO_PARAM_REQ

STATUS_OK = 0x0000

SERVICE_TYPE_AUDIO_DATA_SINK_SERVICE = 0
SERVICE_TYPE_AUDIO_DATA_SOURCE_SERVICE = 1
SERVICE_TYPE_AUDIO_DATA_CAP_DOWNLOAD_SERVICE = 2
SERVICE_TYPE_SCO_PROCESSING = 3
SERVICE_TYPE_AUDIO_DATA_FILE_MANAGER_SERVICE = 4
SERVICE_TYPE_INVALID_TYPE = 1000

DEVICE_TYPE_SCO = 0x0009
DEVICE_TYPE_FILE = 0x000A
DEVICE_TYPE_L2CAP = 0x000C
DEVICE_TYPE_CAP_DOWNLOAD = 0x000F
DEVICE_TYPE_TIMESTAMPED = 0x0011
DEVICE_TYPE_FILE_MANAGER = 0x0012
DEVICE_TYPE_TESTER = 0x007E

# SCO Processing Service direction
SCO_DIR_TO_AIR = 0x01
SCO_DIR_FROM_AIR = 0x02

# SCO Processing Service endpoint type
ENDPOINT_TYPE_SCO = 0
ENDPOINT_TYPE_ISO = 1

# crescendo
INT_SOURCE_TBUS_MSG_ADPTR_EVENT = 0x05
BUS_INT_STATUS = 0xFFFFC00C
TIMER_TIME = 0xFFFFE0E0
