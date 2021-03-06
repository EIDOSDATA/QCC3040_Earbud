#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Audio base class"""

import logging
from abc import ABC, abstractmethod, abstractproperty

MODE_READ = 'r'
MODE_WRITE = 'w'
MODE_LIST = [MODE_READ, MODE_WRITE]
MODE_DEFAULT = MODE_READ


class AudioBase(ABC):
    """Base audio file class handler

    .. code-block:: python

        input_file = AudioBase('audiofile.ext', 'r')
        channels = input_file.get_audio_stream_num()
        print('channels %s' % (channels))
        for channel in range(channels):
            sample_rate = input_file.get_audio_stream_sample_rate(channel)
            sample_width = input_file.get_audio_stream_sample_width(channel)
            audio_data = input_file.get_audio_stream_data(channel)
            print('channel %s sample_rate:%s sample_width:%s samples:%s' %
                  (channel, sample_rate, sample_width, len(audio_data)))

    Args:
        filename (str): Filename
        mode (str): File open mode 'r' for read, 'w' for write
    """

    def __init__(self, filename, mode=MODE_DEFAULT, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self._filename = filename
        self._mode = mode

        if self._mode not in MODE_LIST:
            raise RuntimeError('mode %s invalid' % (self._mode))

        kwargs.pop('channels', None)
        kwargs.pop('sample_rate', None)
        kwargs.pop('sample_width', None)

        if kwargs:
            self._log.warning('unknown kwargs:%s', str(kwargs))

    @abstractproperty
    def platform(self):
        """str: Platform name"""

    @abstractproperty
    def interface(self):
        """str: Interface name"""

    @abstractproperty
    def default_interface(self):
        """bool: Is this the default interface?"""

    @abstractproperty
    def encoded(self):
        """bool: Is this an encoded format"""

    @abstractmethod
    def get_filename(self):
        """Get current filename

        Returns:
            str: Raw filename
        """

    @abstractmethod
    def get_audio_stream_num(self):
        """Get number of audio streams contained in the file

        Returns:
            int: Number of streams
        """

    @abstractmethod
    def get_audio_stream_sample_rate(self, index=0):
        """Get sample rate of an audio stream

        Args:
            index (int): Zero-based index stream

        Returns:
            int: Sample rate in hertzs

        Raises:
            RuntimeError: If the stream does not exist
        """

    @abstractmethod
    def get_audio_stream_sample_width(self, index=0):
        """Get sample width of an audio stream

        Args:
            index (int): Zero-based index stream

        Returns:
            int: Sample width in bits

        Raises:
            RuntimeError: If the stream does not exist
        """

    @abstractmethod
    def get_audio_stream_data(self, index=0, start=0, limit=0):
        """Get audio stream frames

        Args:
            index (int): Zero-based index stream
            start (int): Zero-based index of the first packet to retrieve
            limit (int): Maximum number of packets to retrieve or 0 for all

        Returns:
            list[int]: Frame values

        Raises:
            RuntimeError: If the stream does not exist
        """

    @abstractmethod
    def add_audio_stream(self, sample_rate, sample_width, data):
        """Add audio stream

        Args:
            sample_rate (int): Frame rate in hertzs
            sample_width (int): Sample width in bits
            data (list[int]): Frame values

        Returns:
            list[int]: Frame values

        Raises:
            RuntimeError: If the file is not in write mode
        """

    @abstractmethod
    def write(self):
        """Write contents to file

        Raises:
            RuntimeError: If the file is not in write mode
        """
