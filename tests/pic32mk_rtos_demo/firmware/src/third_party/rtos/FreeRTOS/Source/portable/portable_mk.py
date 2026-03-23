"""
FreeRTOS portable module for PIC32MK_Hello project.
Contains the port-specific files for PIC32MK and memory management.
"""

from pymakelib import module


@module.ModuleClass
class FreeRTOSPortModule(module.AbstractModule):
    """
    FreeRTOS portable layer module for PIC32MK.
    """

    def getSrcs(self) -> list:
        """Return FreeRTOS portable source files."""
        return [
            "firmware/src/third_party/rtos/FreeRTOS/Source/portable/MemMang/heap_1.c",
            "firmware/src/third_party/rtos/FreeRTOS/Source/portable/MPLAB/PIC32MK/port.c",
        ]

    def getIncs(self) -> list:
        """Return include paths for FreeRTOS portable layer."""
        return [
            "firmware/src/third_party/rtos/FreeRTOS/Source/portable/MPLAB/PIC32MK",
        ]
