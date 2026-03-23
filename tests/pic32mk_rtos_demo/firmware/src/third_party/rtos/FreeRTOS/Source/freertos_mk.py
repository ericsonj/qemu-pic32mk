"""
FreeRTOS kernel module — explicit source list for the FreeRTOS kernel,
heap allocator, and PIC32MK port layer.
"""

from pymakelib import module


@module.ModuleClass
class FreeRTOSModule(module.AbstractModule):

    def getSrcs(self) -> list:
        """Return FreeRTOS kernel source files."""
        return [
            'firmware/src/third_party/rtos/FreeRTOS/Source/croutine.c',
            'firmware/src/third_party/rtos/FreeRTOS/Source/list.c',
            'firmware/src/third_party/rtos/FreeRTOS/Source/queue.c',
            'firmware/src/third_party/rtos/FreeRTOS/Source/FreeRTOS_tasks.c',
            'firmware/src/third_party/rtos/FreeRTOS/Source/timers.c',
            'firmware/src/third_party/rtos/FreeRTOS/Source/event_groups.c',
            'firmware/src/third_party/rtos/FreeRTOS/Source/stream_buffer.c',
        ]
    
    def getIncs(self) -> list:
        """Return include paths for FreeRTOS."""
        return [
            'firmware/src/third_party/rtos/FreeRTOS/Source/include',
        ]
