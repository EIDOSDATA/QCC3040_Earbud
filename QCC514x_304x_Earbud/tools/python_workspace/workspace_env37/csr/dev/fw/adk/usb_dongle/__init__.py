from .usb_dongle_app import UsbDongleApp

PYDBG_PLUGIN_CONTAINER_NAME = "app"
PYDBG_PLUGIN_CONTAINER_CLASS = UsbDongleApp

__all__ = [PYDBG_PLUGIN_CONTAINER_NAME, PYDBG_PLUGIN_CONTAINER_CLASS]