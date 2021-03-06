#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra hw streams"""

import argparse
import logging
import os
from abc import ABCMeta, abstractproperty

from kats.core.stream_base import STREAM_TYPE_SOURCE, STREAM_TYPE_SINK, STREAM_NAME, \
    STREAM_RATE, STREAM_DATA_WIDTH, CALLBACK_EOF, STATE_STARTED
from kats.framework.library.log import log_input
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.kalsim.library.period import compute_period
from kats.kalsim.stream.kalsim_helper import get_file_kalsim_stream_config, get_user_stream_config
from kats.kalsim.stream.kalsim_stream import KalsimStream
from kats.kalsim.stream.packet.packetiser import Packetiser
from kats.library.audio_file.audio import audio_get_instance
from kats.library.registry import get_instance_names, get_instance_num, get_instance
from kats.library.test_base import add_output_file

BACKING = 'backing'
BACKING_FILE = 'file'
BACKING_DATA = 'data'
DEVICE = 'device'
FILENAME = 'filename'
CHANNELS = 'channels'
CHANNELS_DEFAULT = 1
CHANNEL = 'channel'
CHANNEL_DEFAULT = 0
SAMPLE_RATE = 'sample_rate'
SAMPLE_WIDTH = 'sample_width'
FRAME_SIZE = 'frame_size'
FRAME_SIZE_DEFAULT = 1
DELAY = 'delay'
DELAY_DEFAULT = 0
LOOP = 'loop'
LOOP_DEFAULT = 1
CALLBACK_DATA_RECEIVED = 'callback_data_received'
CALLBACK_CONSUME = 'callback_consume'

PARAM_SCHEMA = {
    'oneOf': [
        {
            'type': 'object',
            'required': [BACKING, FILENAME],
            'properties': {
                BACKING: {'type': 'string', 'enum': [BACKING_FILE]},
                DEVICE: {'type': 'integer', 'minimum': 0},
                FILENAME: {'type': 'string'},
                CHANNELS: {'type': 'integer', 'minimum': 1},
                CHANNEL: {'type': 'integer', 'minimum': 0},
                SAMPLE_RATE: {'type': 'number', 'minimum': 0, 'exclusiveMinimum': 'true'},
                SAMPLE_WIDTH: {'type': 'integer', 'enum': [8, 16, 24, 32]},
                FRAME_SIZE: {'type': 'integer', 'minimum': 1},
                DELAY: {'type': 'number', 'minimum': 0},
                LOOP: {'type': 'integer', 'minimum': 1},
            }
        },
        {
            'type': 'object',
            'required': [BACKING],
            'properties': {
                BACKING: {'type': 'string', 'enum': [BACKING_DATA]},
                DEVICE: {'type': 'integer', 'minimum': 0},
                SAMPLE_WIDTH: {'type': 'integer', 'enum': [8, 16, 24, 32]},
                FRAME_SIZE: {'type': 'integer', 'minimum': 1},
            }
        }
    ]
}

HYDRA_TYPE = 'hydra_type'
HYDRA_TYPE_AUDIO_SLOT = 'audio_slot'
HYDRA_AUDIOSLOT = 'hydra_audioslot'


class StreamHw(KalsimStream, metaclass=ABCMeta):
    """Hydra hw streams

    Args:
        stream_type (str): Type of stream

            - "source". Data is pushed into kymera
            - "sink". Data is extracted from kymera

        backing (str): Origin (for sources) or destination (for sinks) of data.
            This parameter defines what other parameters are needed.

            - "file". Data is backed by a file, if it is "source" is the contents to be
              streamed, if it is "sink" is the file where data will be written to.
            - "data". Data is coming from ("source") or going to ("sink") an external software
              component. This allow to have loops where a sink stream loops back to a
              source stream.

        device (int): Mapping between stream and endpoint, being the first hw source stream
            mapped to the first hw source endpoint and so on, sources and sinks mapping is
            independent. **Currently unused**
        filename (str): Filename to back the stream

            - backing=="file". Mandatory

                - raw files only store audio data but no information about number of channels,
                  sampling frequency or data format.
                  This information (*channels*, *sample_rate*, *sample_width*) has to be supplied
                - wav files store number of channels, sampling frequency and sample data format.
                  Note that if *sample_rate* is provided then information in the file is overridden
                - qwav files store number of channels, sampling frequency, sample data format,
                  optional metadata and optional packet based information

            - backing =="data". Unavailable

        channels (int): Number of channels/streams in the audio file

            - backing=="file". Only for source streams, sink streams are always created with 1
              channel (optional default=1).
            - backing =="data". Unavailable

        channel (int): Channel index in the audio file

            - backing=="file". Only for source streams, sink streams are always created with 1
              channel (optional default=0).
            - backing =="data". Unavailable

        sample_rate (int): Sample rate in hertzs.

            - backing=="file". For raw source files (mandatory), wav source files (optional)
              and all sink files (mandatory).
            - backing=="data". Valid for all sink files (mandatory).

        sample_width (int): Number of bits per sample

            - backing=="file". For raw source files (mandatory) and all sink files (mandatory).
            - backing=="data". Valid for all file types and stream types (mandatory).

        frame_size (int): Number of frames per transfer

            - backing=="file". Valid for all file types and stream types (optional, default=1).
            - backing=="data". Only used in sink streams (optional default=1).

        delay (float): Delay in seconds from start to real start.
            Indicates the delay between the stream start command and the actual start in seconds

            - backing=="file". Only for source streams (optional default=0.0)
            - backing =="data". Unavailable

        loop (int): Number of times the source is played, when the source gets to end of file
            it is rewinded and replayed, only for source streams (optional default=1)

            - backing=="file". Available
            - backing =="data". Unavailable

        callback_consume (function(int)): Callback function when data is received

            - backing=="file". Unavailable
            - backing=="data". Only used in sink streams (mandatory) but can be set in the
              config method.
    """

    @abstractproperty
    def platform(self):
        """list[str]: Platform name"""

    @abstractproperty
    def interface(self):
        """str: Interface name"""

    def __init__(self, stream_type, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        DefaultValidatingDraft4Validator(PARAM_SCHEMA).validate(kwargs)

        self.__helper = argparse.Namespace()  # helper modules
        self.__helper.uut = get_instance('uut')

        self.__config = argparse.Namespace()  # configuration values
        self.__config.backing = kwargs.pop(BACKING)
        self.__config.device = kwargs.pop(DEVICE, 0)  # currently unused
        self.__config.callback_data_received = None  # backing=data sink stream callback
        self.__config.delay = None

        self.__data = argparse.Namespace()  # data values
        self.__data.loop_timer_id = None  # timer when looping
        self.__data.loop = 1  # number of loops pending
        self.__data.source_packetiser = None  # source with packet based streaming
        self.__data.sink_timer_id = None  # sink manual streaming timer id
        self.__data.sink_timer_remain = 0.0  # sink manual timer remainder
        self.__data.sink_audio_buffer = None  # sink manual streaming audio data buffer

        if self.__config.backing == BACKING_FILE:
            self.__config.filename = kwargs.pop(FILENAME)
            add_output_file(self.__config.filename)
            if stream_type == STREAM_TYPE_SOURCE:
                self.__config.delay = kwargs.pop(DELAY, DELAY_DEFAULT)
            else:
                self.__config.delay = None
            self.__config.loop = kwargs.pop(LOOP, LOOP_DEFAULT)
            self.__config.user_callback_eof = kwargs.get(CALLBACK_EOF, lambda *args, **kwargs: None)

            params = getattr(self, '_init_%s_file' % (stream_type))(kwargs)
            params[CALLBACK_EOF] = self.__eof  # required for loop
        else:  # BACKING_DATA
            self.__config.sample_width = kwargs.pop(SAMPLE_WIDTH)
            if stream_type == STREAM_TYPE_SINK:
                self.__config.sample_rate = kwargs.pop(SAMPLE_RATE)
                self.__config.frame_size = kwargs.pop(FRAME_SIZE, FRAME_SIZE_DEFAULT)
                self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME, None)
            else:
                self.__config.frame_size = None
            self.__config.loop = 1

            params = get_user_stream_config(self.__config.sample_width)

        self.__parameters = {}
        self.__parameters[HYDRA_TYPE] = HYDRA_TYPE_AUDIO_SLOT
        self.__parameters[HYDRA_AUDIOSLOT] = self._compute_audioslot(stream_type)

        if kwargs:
            self._log.warning('unknown kwargs:%s', str(kwargs))

        super().__init__(stream_type, **params)

    @staticmethod
    def _compute_audioslot(stream_type):
        # to compute the audioslot we have to count how many instances have
        # we registered of name stream_XXX that are of our type (source or sink)
        # and that are of type StreamHw
        val = 0
        names = get_instance_names()
        for stream_name in names:
            if stream_name.startswith('stream_'):
                val += sum([get_instance(stream_name, ind).get_type() == stream_type
                            for ind in range(get_instance_num(stream_name))
                            if isinstance(get_instance(stream_name, ind), StreamHw)])
        return val

    def _build_default_packet_info(self, audio_data, frame_size, sample_rate):
        # FIXME be careful with loop as all loops but first should not zero first packet
        period_usec = (1000000.0 * frame_size) / sample_rate
        packet_info = [
            [0 if ind == 0 else int(period_usec * ind - int(period_usec * (ind - 1))),  # timestamp
             ind * frame_size,  # offset
             frame_size]  # length
            for ind in range(int(len(audio_data) / frame_size))]
        if len(audio_data) % frame_size:
            self._log.warning('excluding %s bytes as it is not a multiple of frame_size',
                              len(audio_data) % frame_size)
        return packet_info

    def _init_source_file(self, kwargs):
        channels = kwargs.pop(CHANNELS, CHANNELS_DEFAULT)
        channel = kwargs.pop(CHANNEL, CHANNEL_DEFAULT)
        self.__config.sample_rate = kwargs.pop(SAMPLE_RATE, None)
        self.__config.sample_width = kwargs.pop(SAMPLE_WIDTH, None)
        self.__config.frame_size = kwargs.pop(FRAME_SIZE, FRAME_SIZE_DEFAULT)

        audio_kwargs = {
            'channels': channels,  # some formats do not require
            'sample_rate': self.__config.sample_rate,  # some formats do not require
            'sample_width': self.__config.sample_width,  # some formats do not require
            'allow_encoded': False,
        }
        audio_instance = audio_get_instance(self.__config.filename, **audio_kwargs)
        channels = audio_instance.get_audio_stream_num()
        # we allow overriding sample_rate from what is in the file
        self.__config.sample_width = audio_instance.get_audio_stream_sample_width(channel)

        if channel >= channels:
            raise RuntimeError('channels:%s channel:%s inconsistency' % (channels, channel))
        if audio_instance.get_audio_stream_sample_rate(channel) is None:
            raise RuntimeError('stream filename:%s sample_rate not set' % (self.__config.filename))
        if self.__config.sample_width is None:
            raise RuntimeError('stream filename:%s sample_width not set' % (self.__config.filename))

        # kalsim supports raw and wav files with only 1 stream in the file at the file designated
        # sample_rate
        file_sample_rate = audio_instance.get_audio_stream_sample_rate(channel)
        if (channels == 1 and not self.__config.filename.endswith('.qwav') and
                (self.__config.sample_rate == file_sample_rate or
                 self.__config.sample_rate is None)):
            if self.__config.sample_rate is None:
                self.__config.sample_rate = file_sample_rate
            params = get_file_kalsim_stream_config(self.__config.filename,
                                                   self.__config.frame_size,
                                                   self.__config.sample_rate,
                                                   self.__config.sample_width)
        else:
            audio_data = audio_instance.get_audio_stream_data(channel)
            if self.__config.sample_rate is None:
                self.__config.sample_rate = file_sample_rate

            # check if the format supports packet_info and that packet_info actually is there
            if (hasattr(audio_instance, 'get_packet_data_size') and
                    audio_instance.get_packet_data_size('audio', channel)):
                packet_info = audio_instance.get_packet_data('audio', channel)
            else:
                # if packet_info is not available or empty then build default
                # packet based information
                packet_info = self._build_default_packet_info(audio_data, self.__config.frame_size,
                                                              self.__config.sample_rate)
            self.__data.source_packetiser = Packetiser(self, audio_data, packet_info)
            params = get_user_stream_config(self.__config.sample_width)
            params[STREAM_NAME] = self.__config.filename
            params[STREAM_RATE] = self.__config.sample_rate  # pointless in qwav with packet_info
            params[STREAM_DATA_WIDTH] = self.__config.sample_width

        del audio_instance
        return params

    def _init_sink_file(self, kwargs):
        self.__config.channels = 1
        self.__config.channel = 0
        self.__config.sample_rate = kwargs.pop(SAMPLE_RATE)
        self.__config.sample_width = kwargs.pop(SAMPLE_WIDTH)
        self.__config.frame_size = kwargs.pop(FRAME_SIZE, FRAME_SIZE_DEFAULT)

        # kalsim supports raw and wav files with only 1 stream
        if not self.__config.filename.endswith('.qwav'):
            params = get_file_kalsim_stream_config(self.__config.filename,
                                                   self.__config.frame_size,
                                                   self.__config.sample_rate,
                                                   self.__config.sample_width)
        else:
            self.__data.sink_audio_buffer = []
            params = get_user_stream_config(self.__config.sample_width)
            params[STREAM_NAME] = self.__config.filename
            params[STREAM_RATE] = self.__config.sample_rate
            params[STREAM_DATA_WIDTH] = self.__config.sample_width
        return params

    def _data_received(self, timer_id):
        """Callback to be invoked when a timer has fired to get data from a sink stream

        Args:
            timer_id (int): Timer id
        """
        _ = timer_id
        self.__data.sink_timer_id = None

        if self.get_state() == STATE_STARTED:
            data = self.extract(self.__config.frame_size)
            if self.__config.backing == BACKING_FILE:
                self.__data.sink_audio_buffer += data
            else:
                if self.__config.callback_consume:
                    self.__config.callback_consume[0](data=data)

            # set next periodic timer
            period = (1.0 * self.__config.frame_size) / self.__config.sample_rate
            period, self.__data.sink_timer_remain = compute_period(
                period, self.__data.sink_timer_remain)
            self.__data.sink_timer_id = self.__helper.uut.timer_add_relative(
                period, self._data_received)

    @log_input(logging.INFO)
    def _start(self, timer_id):
        """Callback to be invoked when the source stream start delay has elapsed to actually start
        the stream.

        Args:
            timer_id (int): Timer id
        """
        _ = timer_id
        self.__data.loop_timer_id = None
        super().start()

        if self.get_type() == STREAM_TYPE_SOURCE:
            if self.__config.backing == BACKING_FILE:
                # if we have kalsim support then the start call above will handle streaming
                if self.__data.source_packetiser:  # without kalsim support
                    self.__data.source_packetiser.start()
            else:  # backing=data
                # we receive data with calls to consume
                pass
        else:  # sink stream
            if self.__config.backing == BACKING_FILE:
                # if we have kalsim support then the start call above will handle streaming
                if self.__data.sink_audio_buffer is not None:  # without kalsim support
                    if os.path.isfile(self.__config.filename):
                        os.remove(self.__config.filename)
                    self.__data.sink_audio_buffer = []

                    # set next periodic timer
                    period = (1.0 * self.__config.frame_size) / self.__config.sample_rate
                    period, self.__data.sink_timer_remain = compute_period(period, 0)
                    self.__data.sink_timer_id = self.__helper.uut.timer_add_relative(
                        period, self._data_received)
            else:  # backing=data
                # set next periodic timer
                period = (1.0 * self.__config.frame_size) / self.__config.sample_rate
                period, self.__data.sink_timer_remain = compute_period(period, 0)
                self.__data.sink_timer_id = self.__helper.uut.timer_add_relative(
                    period, self._data_received)

    def __eof(self):
        """Internal End of file callback.

        This is used to accommodate the loop paramater which allows a source stream to
        automatically restart without delay multiple times
        """
        self.__data.loop = 0 if self.__data.loop <= 1 else self.__data.loop - 1
        if self.__data.loop > 0:
            self.stop()
            self._start(0)
        else:
            self.__config.user_callback_eof()

    @log_input(logging.INFO)
    def config(self, **kwargs):
        """Configure stream"""
        if CALLBACK_CONSUME in kwargs:
            self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME)
            if not isinstance(self.__config.callback_consume, list):
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))
            if len(self.__config.callback_consume) != 1:
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))

        for key in self.__parameters:
            self._config_param(key, self.__parameters[key])

        super().config(**kwargs)

        for key in self.__parameters:
            _ = self.query(key)

    @log_input(logging.INFO)
    def start(self):
        """Start stream"""
        self.__data.loop = self.__config.loop
        if self.__config.delay:
            self._log.info('delaying start for %s seconds', self.__config.delay)
            self.__data.loop_timer_id = self.__helper.uut.timer_add_relative(self.__config.delay,
                                                                             callback=self._start)
        else:
            self._start(0)

    @log_input(logging.INFO)
    def stop(self):
        """Stop stream"""

        if self.__data.loop_timer_id is not None:
            self.__helper.uut.timer_cancel(self.__data.loop_timer_id)
            self.__data.loop_timer_id = None
        if self.__data.source_packetiser:
            self.__data.source_packetiser.stop()
        if self.__data.sink_timer_id is not None:
            self.__helper.uut.timer_cancel(self.__data.sink_timer_id)
            self.__data.sink_timer_id = None

        super().stop()

    @log_input(logging.INFO)
    def destroy(self):
        """Destroy stream"""

        if self.__config.backing == BACKING_FILE and self.__data.sink_audio_buffer is not None:
            self._log.info('creating file %s', self.__config.filename)
            audio_instance = audio_get_instance(self.__config.filename, 'w', allow_encoded=False)
            audio_instance.add_audio_stream(self.__config.sample_rate,
                                            self.__config.sample_width,
                                            self.__data.sink_audio_buffer)
            audio_instance.write()
            del audio_instance
            self.__data.sink_audio_buffer = []

        super().destroy()

    def consume(self, input_num, data):
        if (input_num == 0 and
                self.get_type() == STREAM_TYPE_SOURCE and
                self.__config.backing == BACKING_DATA):
            self.insert(data)

    def eof_detected(self, input_num):
        if (input_num == 0 and
                self.get_type() == STREAM_TYPE_SOURCE and
                self.__config.backing == BACKING_DATA):
            self.eof()
