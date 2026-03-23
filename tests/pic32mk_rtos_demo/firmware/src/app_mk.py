"""
Application module — contains main.c entry point.
"""

from pymakelib import module


@module.ModuleClass
class AppModule(module.BasicCModule):

    def getSrcs(self) -> list:
        return [
            "firmware/src/main.c",
            "firmware/src/usb_device_init_data.c",
            "firmware/src/usb_init.c",
        ]

    def getIncs(self) -> list:
        return ["firmware/src"]
