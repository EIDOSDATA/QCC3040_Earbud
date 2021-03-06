#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""ACAT instrument"""

import logging
import os
import sys
import threading
import time

from kats.framework.library.instrument import Instrument
from kats.framework.library.log import log_input, log_output
from kats.library.registry import get_instance

TRANSPORT = 'transport'
TRANSPORT_KALSIM = 'kalsim'
INSTRUMENT = 'acat'
FIRMWARE = 'firmware'
INTERACTIVE = 'interactive'
PATH = 'path'
KAL_PATH = 'kal_path'
BUNDLE = 'bundle'
BUNDLE_DEFAULT = []
FIRMWARE_LOG_ENABLE = 'firmware_log_enable'
FIRMWARE_LOG_ENABLE_DEFAULT = False
FIRMWARE_LOG_LEVEL = 'firmware_log_level'
FIRMWARE_LOG_LEVEL_DEFAULT = 2
FIRMWARE_LOG_TIMER_UPDATE_PERIOD = 'firmware_log_timer_update_period'
FIRMWARE_LOG_TIMER_UPDATE_PERIOD_DEFAULT = 0.5
ANALYSES = 'analyses'
ANALYSES_DEFAULT = None
DUAL_CORE = 'dual_core'
DUAL_CORE_DEFAULT = False

ANALYSIS_HEAP_MEM = 'heapmem'
ANALYSIS_POOL_INFO = 'poolinfo'
ANALYSIS_PROFILER = 'profiler'
ANALYSIS_DEBUG_LOG = 'debuglog'


class KalcmdAcat:
    """Adaptation for ACAT to interface kalcmd

    This class bridges between ACAT expected kalcmd interface and kalcmd instrument actual interface

    Args:
        kalcmd:
            kats.instrument.kalcmd.kalcmd.Kalcmd
    """

    def __init__(self, kalcmd):
        import kalcmd2
        self.kalcmd2 = kalcmd2

        self._kalcmd = kalcmd

    def block_mem_read(self, memory_space, address, num_words):
        """Read kalimba memory in blocks of uint32_t data

        Args:
            memory_space (int): Memory space as specified in kalcmd2.kalmem_spaces
            address (int): Memory address
            num_words (int): Number of uint32_t data blocks to read

        Returns:
            list[int]: Data read
        """
        return self._kalcmd.block_mem_read(memory_space, address, num_words)

    def block_mem_write(self, memory_space, address, data):
        """Write kalimba memory in blocks of uint32_t data

        Args:
            memory_space (int): Memory space as specified in kalcmd2.kalmem_spaces
            address (int): Memory address
            data (list[int]): Data to be written
        """
        return self._kalcmd.block_mem_write(memory_space, address, data)

    def getKalArch(self):  # pylint: disable=invalid-name
        """Get Kalimba architecture

        Returns:
            int: Kalimba architecture
        """
        return self._kalcmd.get_kalimba_arch()

    def getKalimbaName(self):  # pylint: disable=invalid-name
        """Get Kalimba name

        Returns:
            str: Kalimba name
        """
        return self._kalcmd.get_kalimba_name()

    def querryDspReg(self, register):  # pylint: disable=invalid-name
        """Get kalimba register value

        Args:
            register (int): Register as in kalcmd2.kal_regs

        Returns:
            int: Register value
        """
        return self._kalcmd.query_dsp_register(register)


class Acat(Instrument):
    """ACAT instrument.

    This instrument instantiates an ACAT class instance

    Args:
        firmware (str): Path to firmware
        transport (str): kalaccess transport
        interactive (bool): Run in interactive mode
        path (str): Path to ACAT package directory, if not set then the module will be imported as
            ACAT
        kal_path (str): Path to kalimba python tools package directory, if not set then the module
            will be imported as kal_python_tools
        bundle (list[str]): Path to additional firmware files to be loaded
        firmware_log_enable (bool): Enable logging firmware messages
        firmware_log_level (int): Firmware defined log level
    """

    interface = 'acat'
    schema = {
        'type': 'array',
        'minItems': 1,
        'uniqueItems': True,
        'items': {
            'type': 'object',
            'required': [FIRMWARE],
            'properties': {
                FIRMWARE: {'type': 'string'},
                TRANSPORT: {'type': 'string', 'default': TRANSPORT_KALSIM},
                INTERACTIVE: {'type': 'boolean', 'default': False},
                PATH: {'type': 'string'},
                KAL_PATH: {'type': 'string'},
                BUNDLE: {'type': 'array', 'item': {'type': 'string'}, 'default': BUNDLE_DEFAULT},
                FIRMWARE_LOG_ENABLE: {'type': 'boolean', 'default': FIRMWARE_LOG_ENABLE_DEFAULT},
                FIRMWARE_LOG_LEVEL: {'type': 'integer', 'default': FIRMWARE_LOG_LEVEL_DEFAULT},
                FIRMWARE_LOG_TIMER_UPDATE_PERIOD: {
                    'type': 'number',
                    'default': FIRMWARE_LOG_TIMER_UPDATE_PERIOD_DEFAULT,
                    'minimum': 0.01},
                ANALYSES: {
                    'type': 'array',
                    'default': [],
                    'items': {
                        'type': 'string'
                    }
                },
                DUAL_CORE: {'type': 'boolean', 'default': DUAL_CORE_DEFAULT},
            }
        }
    }

    def __init__(self, firmware, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self._log.info('init firmware:%s kwargs:%s', firmware, str(kwargs))

        self._transport = kwargs.pop(TRANSPORT, TRANSPORT_KALSIM)
        self._firmware = firmware
        self._interactive = kwargs.pop(INTERACTIVE, False)
        self._path = kwargs.pop(PATH, None)
        self._kal_path = kwargs.pop(KAL_PATH, None)
        self._bundle = kwargs.pop(BUNDLE, BUNDLE_DEFAULT)
        self._fw_log_enable = kwargs.pop(FIRMWARE_LOG_ENABLE, FIRMWARE_LOG_ENABLE_DEFAULT)
        self._fw_log_level = kwargs.pop(FIRMWARE_LOG_LEVEL, FIRMWARE_LOG_LEVEL_DEFAULT)
        self._fw_log_timer_update_period = kwargs.pop(
            FIRMWARE_LOG_TIMER_UPDATE_PERIOD,
            FIRMWARE_LOG_TIMER_UPDATE_PERIOD_DEFAULT)
        self._analyses = kwargs.pop(ANALYSES, ANALYSES_DEFAULT)
        self._dual_core = kwargs.pop(DUAL_CORE, DUAL_CORE_DEFAULT)
        self._lock = None
        self._data_background_thread = None

        if self._path:
            par_path = os.path.abspath(os.path.join(self._path, os.pardir))
            if par_path not in sys.path:
                sys.path.insert(1, par_path)
        import ACAT  # @UnresolvedImport pylint: disable=import-error

        if self._kal_path:
            kal_dir = os.path.abspath(self._kal_path)
        else:
            import kal_python_tools  # @UnresolvedImport pylint: disable=import-error
            kal_dir = os.path.abspath(os.path.dirname(kal_python_tools.__file__))

        self._acat = ACAT

        args = ['-i'] if self._interactive else []
        args += ['-b', self._firmware,
                 '-t', kal_dir]

        args += ['-s', self._transport]
        if self._transport == TRANSPORT_KALSIM:
            kalcmd = get_instance('kalcmd')
            self._kalcmd_acat = KalcmdAcat(kalcmd)
            args += ['-a', self._kalcmd_acat]

        if self._dual_core:
            args += ['-d']

        for entry in self._bundle:
            args.append('-j')
            args.append(entry)
        self._acat.parse_args(args)

        self._acat_session = None

    def _background_thread(self):
        analysis = []

        self._log.debug('acat firmware log background thread starting')
        proc_analysis = getattr(self._acat_session, 'p0')
        analysis.append(proc_analysis.available_analyses[ANALYSIS_DEBUG_LOG])
        analysis[0].set_debug_log_level(self._fw_log_level)

        for processor in range(1, 8):
            proc = 'p%s' % (processor)
            try:
                proc_analysis = getattr(self._acat_session, proc)
                analysis.append(proc_analysis.available_analyses[ANALYSIS_DEBUG_LOG])
            except Exception:  # pylint:disable=broad-except
                break

        self._log.info('acat firmware log retrieval started, cores:%s', len(analysis))

        def _callback(timer_id):
            _ = timer_id
            try:
                for ind, ana in enumerate(analysis):
                    data = ana.get_debug_log()
                    if data:
                        for entry in data:
                            self._log.info('audio' + str(ind) + ': ' + str(entry).rstrip('\r\n'))

            except Exception:  # pylint:disable=broad-except
                if not ind:
                    self._log.error('unable to get firmware log core:%s', ind)

            if not self._lock.is_set():
                uut.timer_add_relative(self._fw_log_timer_update_period, callback=_callback)

        uut = get_instance('uut')
        _callback(0)

        while True:
            time.sleep(0.2)
            if self._lock.is_set():
                break

        self._log.info('acat firmware log background thread exiting')

    @log_input(logging.INFO)
    def connect(self):
        """Connect to ACAT an create a session"""
        if self._acat_session:
            raise RuntimeError('acat session is already connection')

        # NOBUG if specific analyses are required (non empty) and firmware log is enabled then
        # debuglog analysis should be included or the log will fail
        # Here we are not enforcing that analysis as it might change in the future
        self._acat_session = self._acat.load_session(self._analyses)

        if self._fw_log_enable:
            self._lock = threading.Event()
            self._data_background_thread = threading.Thread(target=self._background_thread, args=())
            self._data_background_thread.daemon = True
            self._data_background_thread.setName('acat firmware log background')
            self._data_background_thread.start()

    def check_connected(self):
        """Check if instrument is connected to ACAT and ready for operation

        Returns:
            bool: acat connected
        """
        return self._acat_session is not None

    @log_input(logging.INFO)
    def disconnect(self):
        """Disconnect instrument."""
        if self._acat_session:
            if self._fw_log_enable:
                self._lock.set()
                time.sleep(0.4)

            self._acat_session = None

    @log_output(logging.INFO)
    def get_version(self):
        """Get ACAT version

        Returns
            str: Version
        """

        try:
            # starting with ACAT 1.7.1 this is supported
            return self._acat.__version__
        except AttributeError:
            # earlier versions have
            return self._acat._version.__version__  # pylint: disable=protected-access

    def get_session(self):
        """Get ACAT instance

        Returns
            acat session
        """
        return self._acat_session

    @log_output(logging.INFO)
    def get_heap_mem_info(self, processor=0):
        """Get Heap memory information

        Args:
            processor (int): Kalimba processor number, starting with 0

        Returns:
            tuple:
                int: Total heap size in bytes
                int: Free heap size in bytes
                int: Mininum free heap size in bytes
        """
        proc = 'p%s' % (processor)
        proc_analysis = getattr(self._acat_session, proc)
        analysis = proc_analysis.available_analyses[ANALYSIS_HEAP_MEM]
        return analysis.ret_get_watermarks()

    @log_output(logging.INFO)
    def get_pool_mem_info(self, processor=0):
        """Get Pool memory information

        Args:
            processor (int): Kalimba processor number, starting with 0

        Returns:
            tuple:
                int: Total pool size in bytes
                int: Free pool size in bytes
                int: Mininum free pool size in bytes
        """
        proc = 'p%s' % (processor)
        proc_analysis = getattr(self._acat_session, proc)
        analysis = proc_analysis.available_analyses[ANALYSIS_POOL_INFO]
        return analysis.ret_get_watermarks()

    @log_output(logging.INFO)
    def get_mips_info(self, processor=0, period=1):
        """Get MIPS information

        Args:
            processor (int): Kalimba processor number, starting with 0
            period (int): Period in seconds

        Returns:
            tuple:
                float: MIPS usage in MIPS
                float: MIPS percent
                int: Chip CPU speed in MHz
        """
        proc = 'p%s' % (processor)
        proc_analysis = getattr(self._acat_session, proc)
        analysis = proc_analysis.available_analyses[ANALYSIS_PROFILER]
        return analysis.ret_run_clks(period)
