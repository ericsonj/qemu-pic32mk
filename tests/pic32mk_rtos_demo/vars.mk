PROJECT         = pic32mk_rtos_demo
PROJECT_OUT     = Release/Objects

CC         := mipsel-linux-gnu-gcc
CXX        := mipsel-linux-gnu-g++
LD         := mipsel-linux-gnu-ld
AR         := mipsel-linux-gnu-ar
AS         := mipsel-linux-gnu-as
OBJCOPY    := mipsel-linux-gnu-objcopy
SIZE       := mipsel-linux-gnu-size
OBJDUMP    := mipsel-linux-gnu-objdump
NM         := mipsel-linux-gnu-nm

# MACROS
COMPILER_FLAGS += 
# MACHINE-OPTS
COMPILER_FLAGS += -march=mips32r2 -mdspr2 -msoft-float -mno-micromips -mabi=32 -EL -G 0 -mno-abicalls
# OPTIMIZE-OPTS
COMPILER_FLAGS += -O1
# OPTIONS
COMPILER_FLAGS += -ffreestanding -fno-builtin -nostdlib -nostdinc -ffunction-sections -fdata-sections
# DEBUGGING-OPTS
COMPILER_FLAGS += -g
# PREPROCESSOR-OPTS
COMPILER_FLAGS += 
# WARNINGS-OPTS
COMPILER_FLAGS += -Wno-unused-parameter -Wno-sign-compare
# CONTROL-C-OPTS
COMPILER_FLAGS += -c
# GENERAL-OPTS
COMPILER_FLAGS += -isystem /usr/lib/gcc-cross/mipsel-linux-gnu/14/include -isystem /usr/mipsel-linux-gnu/include

# LINKER-SCRIPT
LDFLAGS += 
# MACHINE-OPTS
LDFLAGS += 
# GENERAL-OPTS
LDFLAGS += 
# LINKER-OPTS
LDFLAGS += 
