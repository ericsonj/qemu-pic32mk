"""
Configuration module — auto-discovers all C and ASM sources
under firmware/src/config/default/ (crt0.S, port_asm_patched.S,
libc_stubs.c, osal_freertos.c, etc.)
"""

from pymakelib import module


@module.ModuleClass
class ConfigModule(module.BasicCModule):

    def getSrcs(self):
        return self.getAllSrcsC() + self.findSrcs(module.SrcType.ASM)
