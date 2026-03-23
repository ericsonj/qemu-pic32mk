# Plan: Refactor FreeRTOS project to test PIC32MK1024MCM100 Emulation in QEMU

## Context

The PIC32MK1024MCM100 is a microcontroller from Microchip's PIC32MK family, featuring a microAptiv core. QEMU has recently added support for this device, and I want to refactor my existing freertos (`tests/freertos`) project creating a new folder `tests/pic32mk_rtos_demo` to run on this emulated hardware. This will allow me to test the emulator's accuracy and performance with a real-world RTOS application.

## Goals

1. Use pymaketool to re-organize the FreeRTOS project structure. Strucure folder should be:
   - `tests/pic32mk_rtos_demo/firmware/` - for source code and FreeRTOS kernel
   - `tests/pic32mk_rtos_demo/firmware/src/` - for application source files
   - `tests/pic32mk_rtos_demo/firmware/src/main.c` - main application entry point
   - `tests/pic32mk_rtos_demo/firmware/src/app_mk.py` - for build automation with pymaketool
   - `tests/pic32mk_rtos_demo/firmware/thrid_party/rtos/FreeRTOS/Source/` - for FreeRTOS kernel source, use reference (`/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/third_party/rtos/FreeRTOS/Source`)
   - `tests/pic32mk_rtos_demo/firmware/src/config` - for Microchip Harmony configuration files drivers, etc.
   - `tests/pic32mk_rtos_demo/firmware/src/config/config_mk.py` - for build automation with pymaketool
   - `tests/pic32mk_rtos_demo/firmware/src/config/default/` - Microchip Harmony default configuration files, use reference (`/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/config/default`)
   - `tests/pic32mk_rtos_demo/firmware/APP_Firmware` - for application code and logic
   - `tests/pic32mk_rtos_demo/firmware/APP_Firmware/APP_Firmware_mk.py` - for build automation with pymaketool
   - `tests/pic32mk_rtos_demo/Release/` - for build sources and output binaries
   - `tests/pic32mk_rtos_demo/Makefile.py` - for build automation with pymaketool

## Pymaketool reference:

As reference of pymaketool files and structure use the following files from the existing FreeRTOS project:

- `Makefile.py` - [text](/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/Makefile.py)
- `firmware/src/app_mk.py` - [text](/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/app_mk.py)
- `firmware/src/config/config_mk.py` - [text](/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/config/config_mk.py)
- `firmware/APP_Firmware/APP_Firmware_mk.py`

## Important constraints and notes:
- No change freeRTOS kernel source files
- No change Microchip Harmony configuration files

