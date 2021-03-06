#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""UUT handler API"""

import logging

from kats.core.uut_base import UutBase
from kats.framework.library.factory import find_subclass
from kats.library.module import get_parent_module


def uut_get_instance(platform, *args, **kwargs):
    """UUT class loader

    This function loads the relevant uut class depending on the platform.
    It supports a factory interface, defined by the platform, where it will try to autodetect
    a class subclassing UUTBase abstract class

    Args:
        platform (list[str]): Platforms available
    """

    log = logging.getLogger(__name__)
    log.debug('init platform:%s args:%s kwargs:%s', platform, args, kwargs)

    for plat in platform:
        mod_name = get_parent_module(__name__, __file__)  # get parent module
        try:
            class_list = find_subclass(mod_name, UutBase)  # get all available interfaces
            for entry in class_list:
                if list(set(entry.platform).intersection(platform)):
                    log.info('registering uut platform:%s interface:%s class:%s',
                             platform, entry.interface, entry.__name__)
                    uut = entry(*args, **kwargs)
                    return uut  # only one interface is used
        except ImportError:
            log.info('platform:%s no uut interface found', plat)

    raise RuntimeError('no uut platform:%s found' % (platform))
