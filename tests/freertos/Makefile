# Makefile — PIC32MK FreeRTOS Hello World
#
# Build target:  hello_freertos.bin  (flat binary loaded via -bios)
# Run command (example):
#   ./build/qemu-system-mipsel -M pic32mk -bios tests/freertos/hello_freertos.bin \
#       -serial stdio -nographic -monitor none -d unimp,guest_errors

CC      = mipsel-linux-gnu-gcc
LD      = mipsel-linux-gnu-ld
OBJCOPY = mipsel-linux-gnu-objcopy

FREERTOS = /home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/third_party/rtos/FreeRTOS/Source

# Architecture flags for MIPS32r2 + DSP ASE r2 + soft-float + 32-bit ABI
ARCH = -march=mips32r2 -mdspr2 -msoft-float -mno-micromips -mabi=32 -EL -G 0 -mno-abicalls

GCC_INCLUDES := $(shell mipsel-linux-gnu-gcc $(ARCH) -print-file-name=include)

CFLAGS = $(ARCH) \
	-O1 -g \
	-ffreestanding -fno-builtin -nostdlib -nostdinc \
	-ffunction-sections -fdata-sections \
	-I. \
	-I$(GCC_INCLUDES) \
	-I$(FREERTOS)/include \
	-I$(FREERTOS)/portable/MPLAB/PIC32MK \
	-I/usr/mipsel-linux-gnu/include

ASFLAGS = $(ARCH) \
	-ffreestanding -nostdlib -nostdinc \
	-I. \
	-I$(GCC_INCLUDES) \
	-I$(FREERTOS)/include \
	-I$(FREERTOS)/portable/MPLAB/PIC32MK \
	-I/usr/mipsel-linux-gnu/include

LDFLAGS = -m elf32ltsmip \
	-T link.ld \
	--gc-sections \
	-Map=hello_freertos.map

# Source files
C_SRCS = \
	main.c \
	plib_uart1.c \
	$(FREERTOS)/FreeRTOS_tasks.c \
	$(FREERTOS)/queue.c \
	$(FREERTOS)/list.c \
	$(FREERTOS)/portable/MemMang/heap_1.c \
	$(FREERTOS)/portable/MPLAB/PIC32MK/port.c

S_SRCS = \
	crt0.S \
	port_asm_patched.S

# Object files (all in build directory)
BUILDDIR = build
C_OBJS = $(addprefix $(BUILDDIR)/, $(notdir $(C_SRCS:.c=.o)))
S_OBJS = $(addprefix $(BUILDDIR)/, $(notdir $(S_SRCS:.S=.o)))
OBJS   = $(C_OBJS) $(S_OBJS)

# VPATH lets make find sources by name without full path
VPATH = . \
	$(FREERTOS) \
	$(FREERTOS)/portable/MemMang \
	$(FREERTOS)/portable/MPLAB/PIC32MK

.PHONY: all clean run

all: hello_freertos.bin

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.o: %.S | $(BUILDDIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

hello_freertos.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

hello_freertos.bin: hello_freertos.elf
	$(OBJCOPY) -O binary --gap-fill 0xFF $< $@
	@echo "Built $@ (size: $$(wc -c < $@) bytes)"

clean:
	rm -rf $(BUILDDIR) hello_freertos.elf hello_freertos.bin hello_freertos.map

# Quick QEMU run (requires QEMU already built at ../../build/)
QEMU = ../../build/qemu-system-mipsel
run: hello_freertos.bin
	$(QEMU) -M pic32mk -bios hello_freertos.bin \
		-serial stdio -nographic -monitor none \
		-d unimp,guest_errors 2>&1
