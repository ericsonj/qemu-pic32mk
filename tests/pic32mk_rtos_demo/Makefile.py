"""
Makefile.py for PIC32MK RTOS Demo Project
==========================================
Pymaketool build configuration for PIC32MK1024MCM100 FreeRTOS demo
running on QEMU emulator. Uses mipsel-linux-gnu-gcc cross-compiler.

Usage:
    pymaketool            - Build the project
    pymaketool clean      - Clean build artifacts
    pymaketool rebuild    - Clean and rebuild
"""

import subprocess
from pymakelib import MKVARS, AbstractMake, Makeclass, addon
from pymakelib.vscode_addon import VSCodeAddon

addon.add(VSCodeAddon)

# =============================================================================
# Project Configuration
# =============================================================================
PROJECT_NAME = "pic32mk_rtos_demo"

# Resolve GCC built-in include path (for stdint.h, stddef.h with -nostdinc)
GCC_INTERNAL_INCLUDE = subprocess.check_output(
    ["mipsel-linux-gnu-gcc", "-print-file-name=include"],
    text=True
).strip()

# Output directories
FOLDER_OUT = "Release/Objects/"
DIST_DIR = "Release/"

# Target files
TARGET_ELF = DIST_DIR + PROJECT_NAME + ".elf"
TARGET_BIN = DIST_DIR + PROJECT_NAME + ".bin"
TARGET_MAP = DIST_DIR + PROJECT_NAME + ".map"

# Linker script
LINKER_SCRIPT = "firmware/src/config/default/link.ld"

# Resolve GCC built-in include path (for stdint.h, stddef.h with -nostdinc)
GCC_INTERNAL_INCLUDE = subprocess.check_output(
    ["mipsel-linux-gnu-gcc", "-print-file-name=include"],
    text=True
).strip()


@Makeclass
class Project(AbstractMake):
    """PIC32MK RTOS Demo — QEMU emulator test firmware"""

    def getProjectSettings(self, **kwargs) -> dict:
        return {
            "PROJECT_NAME": PROJECT_NAME,
            "FOLDER_OUT": FOLDER_OUT,
        }

    def getTargetsScript(self, **kwargs) -> dict:
        return {
            "TARGET": {
                "LOGKEY": "LD",
                "FILE": TARGET_ELF,
                "SCRIPT": [MKVARS.LD, "-m", "elf32ltsmip",
                           "-T", LINKER_SCRIPT,
                           "--gc-sections",
                           "-Map=" + TARGET_MAP,
                           "-o", "$@", MKVARS.OBJECTS],
            },
            "TARGET_BIN": {
                "LOGKEY": "BIN",
                "FILE": TARGET_BIN,
                "SCRIPT": [MKVARS.OBJCOPY, "-O", "binary",
                           "--gap-fill", "0xFF",
                           MKVARS.TARGET, TARGET_BIN],
            },
        }

    def getCompilerSet(self, **kwargs) -> dict:
        prefix = "mipsel-linux-gnu-"
        return {
            "CC":       prefix + "gcc",
            "CXX":      prefix + "g++",
            "LD":       prefix + "ld",
            "AR":       prefix + "ar",
            "AS":       prefix + "as",
            "OBJCOPY":  prefix + "objcopy",
            "SIZE":     prefix + "size",
            "OBJDUMP":  prefix + "objdump",
            "NM":       prefix + "nm",
            "STRIP":    prefix + "strip",
            "READELF":  prefix + "readelf",
            "INCLUDES": [
                GCC_INTERNAL_INCLUDE,
                "/usr/mipsel-linux-gnu/include",
            ],
        }

    def getCompilerOpts(self, **kwargs) -> dict:
        return {
            "MACROS": {},
            "MACHINE-OPTS": [
                "-march=mips32r2",
                "-mdspr2",
                "-msoft-float",
                "-mno-micromips",
                "-mabi=32",
                "-EL",
                "-G", "0",
                "-mno-abicalls",
            ],
            "OPTIMIZE-OPTS": [
                "-O1",
            ],
            "OPTIONS": [
                "-ffreestanding",
                "-fno-builtin",
                "-nostdlib",
                "-nostdinc",
                "-ffunction-sections",
                "-fdata-sections",
            ],
            "DEBUGGING-OPTS": [
                "-g",
            ],
            "PREPROCESSOR-OPTS": [],
            "WARNINGS-OPTS": [
                "-Wno-unused-parameter",
                "-Wno-sign-compare",
            ],
            "CONTROL-C-OPTS": [
                "-c",
            ],
            "GENERAL-OPTS": [
                "-isystem", GCC_INTERNAL_INCLUDE,
                "-isystem", "/usr/mipsel-linux-gnu/include",
            ],
        }

    def getLinkerOpts(self, **kwargs) -> dict:
        return {
            "LINKER-SCRIPT": [],
            "MACHINE-OPTS": [],
            "GENERAL-OPTS": [],
            "LINKER-OPTS": [],
        }
