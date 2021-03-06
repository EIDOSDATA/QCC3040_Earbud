#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Generic I2S endpoint class"""

import logging

from kats.core.endpoint_base import EndpointBase
from .mapping import map_endpoint


class EndpointI2sHydra(EndpointBase):
    """Generic I2S Endpoint

    Args:
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Instance of class Kymera
        endpoint_type (str): Type of endpoint "source" or "sink"
        instance (int): I2S instance
        channel (int): Time slot 0 to 1
        i2s_sync_rate (int): Sample clock rate in hertzs
        i2s_master_clock_rate (int): Master clock rate in hertzs
        i2s_master_mode (int): 1 for Master mode, 0 for Slave mode
        i2s_justify_mode (int): 0 left justified, 1 right justified
        i2s_left_justify_delay (int): If using Left Justified format
            - 0 (MSB of SD data occurs in the first SCLK period following WS transition)
            - 1 (MSB of SD data occurs in the second SCLK period)
        i2s_channel_polarity (int): 0 data is left channel when WS is high,
            1 data is right channel with WS is high
        i2s_audio_attenuation_enable (int): Audio attenuation enable, 1 for enable, 0 for disable
        i2s_audio_attenuation (int): Audio attenuation, 0 to 15 in 6dB steps
        i2s_justify_resolution (int): Justify resolution:

            - 0 16 bit
            - 1 20 bit
            - 2 24 bit

        i2s_crop_enable (int): Crop enable, 1 for enable, 0 for disable
        i2s_bits_per_sample (int): Bits per sample:

            - 0 16 bit
            - 1 20 bit
            - 2 24 bit

        i2s_tx_start_sample (int): 0 during low wclk phase, 1 during high wclk phase
        i2s_rx_start_sample (int): 0 during low wclk phase, 1 during high wclk phase
        audio_mute_enable (int): Mute endpoint, 1 for enable, 0 for disable
        audio_sample_size (int): Selects the size (width or resolution) of the audio sample on an
            audio interface.

            This setting controls the width of the internal data path, not just the number of bits
            per slot (on digital interfaces).
            All interfaces support the following settings:

                - 16: 16-bit sample size
                - 24: 24-bit sample size

            For the PCM interface, the following extra settings are supported for backward
            compatibility:

                - 0: 13 bits in a 16 bit slot
                - 1: 16 bits in a 16 bit slot (same as 16)
                - 2: 8 bits in a 16 bit slot
                - 3: 8 bits in an 8 bit slot
    """

    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'i2s'

    def __init__(self, kymera, endpoint_type, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        kwargs = map_endpoint(self.interface, endpoint_type, kwargs)
        self._instance = kwargs.pop('instance', 0)
        self._channel = kwargs.pop('channel', 0)

        # initialise default values
        self.__args = []
        for entry in args:
            if not isinstance(entry, list):
                raise RuntimeError('arg %s invalid should be a list' % (entry))
            if len(entry) != 2:
                raise RuntimeError('arg %s invalid should be list of 2 elements' % (entry))
            self.__args.append(entry)

        self.__args += list(kwargs.items())

        super().__init__(kymera, endpoint_type)

    def create(self, *_, **__):
        self._create('i2s', [self._instance, self._channel])

    def config(self):

        for entry in self.__args:
            self.config_param(entry[0], entry[1])

        super().config()
