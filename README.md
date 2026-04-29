# PIC32MK QEMU Board Emulation

Full-system emulation of the **Microchip PIC32MK1024MCM100** microcontroller inside
[QEMU](https://www.qemu.org/). The goal is to enable firmware development, integration
testing, and CI pipelines without physical hardware.

> **Reference datasheet:** DS60001519E — *PIC32MK GPK/MCM with CAN FD Family*
> (783 pages, file: `DS60001519E.pdf`)

---

## Table of Contents

1. [Target Device](#1-target-device)
2. [Implementation Status](#2-implementation-status)
3. [Repository Layout](#3-repository-layout)
4. [Quick Start](#4-quick-start)
   - 4.1 [Prerequisites](#41-prerequisites)
   - 4.2 [Configure & Build QEMU](#42-configure--build-qemu)
   - 4.3 [Build Test Firmware](#43-build-test-firmware)
   - 4.4 [Run the Emulator](#44-run-the-emulator)
5. [Memory Map](#5-memory-map)
6. [Peripheral Reference Documents](#6-peripheral-reference-documents)
7. [Test Firmware](#7-test-firmware)
8. [QEMU Source File Index](#8-qemu-source-file-index)
9. [CPU & Interrupt Details](#9-cpu--interrupt-details)
10. [SFR Address Map](#10-sfr-address-map)
11. [Developer Notes](#11-developer-notes)
12. [References](#12-references)

---

## 1. Target Device

| Parameter | Value |
|---|---|
| Part Number | PIC32MK1024MCM100T-E/PT |
| Core | MIPS32® microAptiv™ MCU (Release 2 + DSP ASEr2) |
| Max Clock | 120 MHz (198 DMIPS) |
| Program Flash | 1024 KB (ECC) |
| SRAM | 256 KB |
| Pin Count | 100-pin TQFP |
| Supply Voltage | 2.8 V – 3.3 V |
| UART | 6 |
| SPI | 6 |
| I2C | 4 |
| CAN FD | 4 |
| USB | Full-Speed (2 instances) |
| ADC | 12-bit (7 modules) |
| Timers | 9 (Timer1–Timer9) |
| Output Compare | 16 (OC1–OC16) |
| Input Capture | 16 (IC1–IC16) |
| PWM | 12 |
| DMA | 8 channels |
| Data EEPROM | 4 KB |

QEMU CPU type used: **74Kf** (supports DSP ASEr2 — M14K lacks it and traps on
`lwx`/`mfhi $ac1`).

---

## 2. Implementation Status

| Phase | Scope | Status |
|---|---|---|
| **Phase 1** | CPU core, memory map, reset / boot sequence | ✅ Complete |
| **Phase 2A** | EVIC, UART1, Timer1, NVM Flash, Data EEPROM, FreeRTOS Hello World | ✅ Complete |
| **Phase 2B** | UART×6, SPI×6, I2C×4, GPIO A–G, Timers 2–9, DMA×8, OC×16, IC×16 | ✅ Complete |
| **Phase 3** | CAN FD×4, ADC (7 modules), DAC×3, PWM×12, QEI×6 | ✅ Complete |
| **Phase 4** | USB OTG×2 (CDC-ACM), bootloader + program-flash write, software reset | ✅ Complete |

**FreeRTOS Hello World milestone:** UART1 TX output, Timer1 tick at ~1 kHz,
FreeRTOS preemptive scheduler — all functional on QEMU.

---

## 3. Repository Layout

```
hw/mips/
├── pic32mk.c            — Board init, memory map, device instantiation
├── pic32mk_evic.c       — EVIC interrupt controller
├── pic32mk_uart.c       — UART×6
├── pic32mk_timer.c      — Timers 1–9
├── pic32mk_gpio.c       — GPIO PORTA–PORTG
├── pic32mk_spi.c        — SPI×6
├── pic32mk_i2c.c        — I2C×4
├── pic32mk_dma.c        — DMA×8
├── pic32mk_canfd.c      — CAN FD×4
├── pic32mk_adchs.c      — 12-bit ADC (7 modules)
├── pic32mk_nvm.c        — NVM / Program Flash controller
├── pic32mk_dataee.c     — Data EEPROM
├── pic32mk_oc.c         — Output Compare×16
├── pic32mk_ic.c         — Input Capture×16
├── pic32mk_usb.c        — USB Full-Speed×2 (CDC-ACM)
├── pic32mk_cru.c        — Clock & Reset Unit
├── pic32mk_cfg.c        — Configuration registers
├── pic32mk_wdt.c        — Watchdog Timer
└── meson.build

include/hw/mips/
├── pic32mk.h            — All base addresses, SFR offsets, size constants
└── pic32mk_evic.h       — EVIC device interface + IRQ line indices

PIC32MK_WS/              — This workspace: plans, notes, per-peripheral docs
tests/firmware/          — Minimal MIPS assembly boot / hello firmware
tests/freertos/          — FreeRTOS Hello World (UART, CAN FD, SPI, USB CDC)
tests/pic32mk_rtos_demo/ — Python host-side tooling (GPIO, eeprom.bin, etc.)
```

---

## 4. Quick Start

### 4.1 Prerequisites

| Tool | Purpose |
|---|---|
| `mipsel-linux-gnu-gcc` | Cross-compiler for MIPS little-endian |
| QEMU build deps | `ninja`, `python3`, `glib2`, `pixman` |
| `xc32-gcc` (optional) | Microchip compiler for Harmony firmware |

FreeRTOS sources are expected at:

```
/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/third_party/rtos/FreeRTOS/Source
```

### 4.2 Configure & Build QEMU

Run once from the repository root:

```bash
mkdir -p build && cd build
../configure \
    --target-list=mipsel-softmmu \
    --enable-debug \
    --disable-werror \
    --enable-trace-backends=log
make -j$(nproc)
```

Quick rebuild after source changes:

```bash
make -C build -j$(nproc)
# or, for the mipsel target only:
make -C build -j8 qemu-system-mipsel
```

### 4.3 Build Test Firmware

```bash
# Minimal boot firmware (MIPS assembly)
make -C tests/firmware

# FreeRTOS Hello World (UART, CAN FD, SPI, USB CDC demo)
make -C tests/freertos

# Build + launch QEMU immediately
make -C tests/freertos run
```

### 4.4 Run the Emulator

```bash
# FreeRTOS Hello World — UART output on stdout
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/freertos/hello_freertos.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors

# Minimal boot firmware
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/firmware/boot.bin \
    -nographic -d unimp,guest_errors

# With GDB remote stub (port 1234, paused at reset)
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/freertos/hello_freertos.bin \
    -serial stdio -nographic \
    -s -S
```

---

## 5. Memory Map

### Physical addresses

| Region | Physical Base | Size | KSEG1 Virtual |
|---|---|---|---|
| SRAM | `0x00000000` | 256 KB | `0xA0000000` |
| Program Flash | `0x1D000000` | 1 MB | `0xBD000000` |
| Boot Flash 1 (BFLASH1) | `0x1FC40000` | 256 KB | `0xBFC40000` |
| Boot Flash 2 | `0x1FC60000` | ~20 KB | `0xBFC60000` |
| SFR window | `0x1F800000` | 1 MB | `0xBF800000` |
| Reset vector | — | — | `0xBFC00000` |

Boot sequence: `0xBFC00000` trampoline → `0xBFC40000` (BFLASH1).
`EBASE = 0xBFC40000`.

### SFR blocks

| Peripheral block | KSEG1 base |
|---|---|
| CFG / PMD / Cache / NVM / WDT / DMT / CRU / PPS | `0xBF800000` |
| EVIC | `0xBF810000` |
| DMA | `0xBF811000` |
| Timers 1–9, IC1–9, OC1–9, I2C1–2, SPI1–2, UART1–2, Data EE, PWM, QEI, GPIO companion | `0xBF820000` |
| IC10–16, OC10–16, I2C3–4, SPI3–6, UART3–6, ADC (secondary) | `0xBF840000` |
| GPIO PORTA–PORTG | `0xBF860000` |
| CAN1–4, ADC, USB1–2 | `0xBF880000` |
| RTCC | `0xBF8C0000` |

> All PIC32MK register banks follow the **SET/CLR/INV convention**: base + 0x0 = direct
> write, +0x4 = CLR, +0x8 = SET, +0xC = INV. This enables lock-free atomic bit manipulation.

---

## 6. Peripheral Reference Documents

Each document below is a self-contained reference for implementing and testing the
corresponding peripheral emulation. They include: register maps, bit-field tables,
interrupt wiring, QEMU device-model internals, and firmware integration examples.

| Document | Peripheral | Status |
|---|---|---|
| [CANFD_IMPLEMENTATION_NOTES.md](PIC32MK_WS/CANFD_IMPLEMENTATION_NOTES.md) | CAN FD×4 (CAN1–CAN4) | Complete |
| [SPI_EMULATION.md](PIC32MK_WS/SPI_EMULATION.md) | SPI×6 with chardev socket | Complete |
| [INPUT_CAPTURE_EMULATION.md](PIC32MK_WS/INPUT_CAPTURE_EMULATION.md) | Input Capture×16 (IC1–IC16) | Complete |
| [OUTPUT_COMPARE_EMULATION.md](PIC32MK_WS/OUTPUT_COMPARE_EMULATION.md) | Output Compare×16 (OC1–OC16) | Complete |
| [NVM_FLASH_CONTROLLER_EMULATION.md](PIC32MK_WS/NVM_FLASH_CONTROLLER_EMULATION.md) | NVM / Program Flash controller | Complete |
| [DATA_EEPROM_EMULATION.md](PIC32MK_WS/DATA_EEPROM_EMULATION.md) | Data EEPROM (4 KB) | Complete |
| [USB_CDC_EMULATION.md](PIC32MK_WS/USB_CDC_EMULATION.md) | USB Full-Speed CDC-ACM | Complete |
| [PROGRAM_FLASH_AND_SOFTWARE_RESET.md](PIC32MK_WS/PROGRAM_FLASH_AND_SOFTWARE_RESET.md) | Bootloader + pflash write + RSWRST | Complete |
| [CAN2_RX_TASK_PLAN.md](PIC32MK_WS/CAN2_RX_TASK_PLAN.md) | CAN2 interrupt-driven RX with FreeRTOS queue | Reference plan |

IRQ vector assignments for all 216 interrupt sources:

| File | Contents |
|---|---|
| [IRQ_TABLE.csv](PIC32MK_WS/IRQ_TABLE.csv) | Complete EVIC IRQ table (source, IFS register, bit position) |

---

## 7. Test Firmware

### `tests/firmware/` — Bare-metal assembly

| File | Purpose |
|---|---|
| `boot.S` | Reset vector, minimal stack setup, infinite loop |
| `hello.c` | UART1 "Hello" loop (C, no OS) |
| `crt0.S` | C runtime startup |
| `boot.ld` / `hello.ld` | Linker scripts |

### `tests/freertos/` — FreeRTOS integration demo

| File | Purpose |
|---|---|
| `main.c` | FreeRTOS tasks: UART TX, CAN FD TX/RX, SPI loopback, NVM, EEPROM, USB CDC |
| `crt0.S` | MIPS startup + EVIC ISR dispatch table |
| `port_asm_patched.S` | FreeRTOS-safe ISR wrappers for MIPS |
| `link.ld` | Memory layout for Boot Flash 1 |
| `FreeRTOSConfig.h` | Kernel configuration (tick rate, heap, priorities) |
| `xc.h` | GCC shim replacing Microchip `xc.h` (SFR address macros) |
| `plib_uart1/2.c` | Harmony3 UART PLIBs |
| `plib_canfd1/2.c` | Harmony3 CAN FD PLIBs |
| `drv_usbfs*.c` | Harmony3 USB Full-Speed driver |
| `usb_device_cdc*.c` | Harmony3 USB CDC class driver |

### `tests/pic32mk_rtos_demo/` — Host-side Python tooling

Python scripts for GPIO injection, EEPROM file management, and build automation
for the full RTOS demo.

---

## 8. QEMU Source File Index

| File | Peripheral | Phase |
|---|---|---|
| `hw/mips/pic32mk.c` | Board init, memory map, device instantiation | 1 |
| `hw/mips/pic32mk_evic.c` | EVIC interrupt controller (216 sources, vectored) | 2A |
| `hw/mips/pic32mk_uart.c` | UART×6 (full register model + chardev) | 2A/2B |
| `hw/mips/pic32mk_timer.c` | Timer1–Timer9 (period match, PR, interrupt) | 2A/2B |
| `hw/mips/pic32mk_nvm.c` | NVM Flash controller (NVMCON command FSM) | 2A |
| `hw/mips/pic32mk_dataee.c` | Data EEPROM (EECON FSM, optional file backing) | 2A |
| `hw/mips/pic32mk_gpio.c` | GPIO PORTA–PORTG (direction, input/output) | 2B |
| `hw/mips/pic32mk_spi.c` | SPI×6 (FIFO, master/slave, chardev socket) | 2B |
| `hw/mips/pic32mk_i2c.c` | I2C×4 (register stub + LOG_UNIMP) | 2B |
| `hw/mips/pic32mk_dma.c` | DMA×8 (register stub) | 2B |
| `hw/mips/pic32mk_oc.c` | Output Compare×16 (register model + event stream) | 2B |
| `hw/mips/pic32mk_ic.c` | Input Capture×16 (FIFO, 2 IRQs/instance, socket injection) | 2B |
| `hw/mips/pic32mk_canfd.c` | CAN FD×4 (full FIFO model, filter/mask, virtual bus) | 3 |
| `hw/mips/pic32mk_adchs.c` | 12-bit ADC — 7 modules (register model + sample injection) | 3 |
| `hw/mips/pic32mk_usb.c` | USB Full-Speed×2 CDC-ACM (BDT, EP0 state machine, PTY) | 4 |
| `hw/mips/pic32mk_cru.c` | Clock & Reset Unit (PLL registers, RSWRST) | 2A |
| `hw/mips/pic32mk_cfg.c` | Configuration & PMD registers | 1 |
| `hw/mips/pic32mk_wdt.c` | Watchdog Timer (WDTCON, reset on timeout) | 2B |
| `include/hw/mips/pic32mk.h` | All base addresses, offsets, IRQ indices | — |
| `include/hw/mips/pic32mk_evic.h` | EVIC device interface + IRQ line indices | — |

---

## 9. CPU & Interrupt Details

### CPU

| Property | Value |
|---|---|
| Core | MIPS32® microAptiv™ MCU |
| ISA | MIPS32 Release 2 + microMIPS |
| Endianness | Little-endian (`CONFIG.BE = 0`) |
| FPU | Present (`CONFIG1.FP = 1`) |
| DSP ASE | Revision 2 (`CONFIG3.DSP2P=1, DSPP=1`) |
| MMU/TLB | None — fixed MPU (`CONFIG1.MMUSIZE = 0`) |
| Interrupts | EIC vectored mode (`CONFIG3.VEIC=1, VINT=1`) |
| QEMU CPU type | **74Kf** |

### EVIC (External Vectored Interrupt Controller)

- 216 interrupt sources, up to 190 vectors
- 7 priority levels × 4 subpriority levels
- Single-vector (`INTCON.MVEC=0`) and multi-vector (`MVEC=1`) modes
- General exception vector: `EBASE + 0x180`
- Vectored interrupt base: `EBASE + 0x200`

Key EVIC register offsets from `0xBF810000`:

| Register | Offset |
|---|---|
| `INTCON` | `0x0000` |
| `IFSx` (x = 0–7) | `0x0040` + 0x10×x |
| `IECx` (x = 0–7) | `0x00C0` + 0x10×x |
| `IPCx` (x = 0–63) | `0x0140` + 0x10×x |
| `OFFx` (x = 0–190) | `0x0540` + 0x04×x |

---

## 10. SFR Address Map

Complete peripheral base address table (physical addresses):

| Peripheral | Physical Base | KSEG1 Virtual |
|---|---|---|
| CFG / PMD | `0x1F800000` | `0xBF800000` |
| Cache control | `0x1F800800` | `0xBF800800` |
| NVM (Flash) | `0x1F800A00` | `0xBF800A00` |
| WDT | `0x1F800C00` | `0xBF800C00` |
| DMT | `0x1F800E00` | `0xBF800E00` |
| CRU | `0x1F801200` | `0xBF801200` |
| PPS | `0x1F801400` | `0xBF801400` |
| EVIC | `0x1F810000` | `0xBF810000` |
| DMA | `0x1F811000` | `0xBF811000` |
| Timer1 | `0x1F820000` | `0xBF820000` |
| IC1 | `0x1F822000` | `0xBF822000` |
| OC1 | `0x1F824000` | `0xBF824000` |
| I2C1 | `0x1F826000` | `0xBF826000` |
| SPI1 | `0x1F827000` | `0xBF827000` |
| UART1 | `0x1F828000` | `0xBF828000` |
| Data EEPROM | `0x1F829000` | `0xBF829000` |
| I2C3 | `0x1F846400` | `0xBF846400` |
| SPI3 | `0x1F847400` | `0xBF847400` |
| UART3 | `0x1F848400` | `0xBF848400` |
| GPIO (PORTA) | `0x1F860000` | `0xBF860000` |
| CAN1 | `0x1F880000` | `0xBF880000` |
| CAN2 | `0x1F881000` | `0xBF881000` |
| CAN3 | `0x1F882000` | `0xBF882000` |
| CAN4 | `0x1F883000` | `0xBF883000` |
| ADC | `0x1F887000` | `0xBF887000` |
| USB1 | `0x1F889000` | `0xBF889000` |
| USB2 | `0x1F88A000` | `0xBF88A000` |
| RTCC | `0x1F8C0000` | `0xBF8C0000` |

---

## 11. Developer Notes

### Adding a new peripheral

1. Read the relevant section of **DS60001519E** — register offsets and reset values
   must match exactly.
2. Create `hw/mips/pic32mk_<name>.c` using the `SysBusDevice` QOM pattern.
3. Implement `MemoryRegionOps` read/write handlers.
4. Use `LOG_UNIMP` stubs for every unimplemented SFR range — **never** silent ignores.
5. Instantiate and wire in `hw/mips/pic32mk.c`, then add to `hw/mips/meson.build`.
6. Add IRQ lines to `include/hw/mips/pic32mk_evic.h`.
7. Write a minimal FreeRTOS test task in `tests/freertos/main.c`.

### QEMU API patterns used

- `memory_region_init_io()` + `MemoryRegionOps` — MMIO register banks
- `sysbus_init_irq()` / `qdev_connect_gpio_out()` — IRQ wiring to EVIC
- `qemu_chr_fe_*()` — UART/SPI/IC chardev backends
- `timer_new_ns()` + `QEMU_CLOCK_VIRTUAL` — Timer1 period match
- `cpu_physical_memory_read/write()` — DMA and USB BDT access
- `qemu_log_mask(LOG_UNIMP, ...)` — unimplemented register accesses

### Datasheet convention reminder

Every PIC32MK SFR bank exposes four sub-registers per logical register:

```
base + 0x0   direct write  (reg = val)
base + 0x4   CLR           (reg &= ~val)
base + 0x8   SET           (reg |= val)
base + 0xC   INV           (reg ^= val)
```

All emulated peripherals implement this atomically.

### Contribution target

Upstream: `gitlab.com/qemu-project/qemu` (the GitHub mirror is read-only).

---

## 12. References

| Resource | Location |
|---|---|
| PIC32MK datasheet (DS60001519E) | `DS60001519E.pdf` / `PIC32MK_WS/DS60001519E.pdf` |
| Microchip XC32 header | `/opt/microchip/xc32/v4.60/pic32mx/include/proc/PIC32MK-MC/p32mk1024mcm100.h` |
| QEMU developer docs | https://www.qemu.org/docs/master/ |
| MIPS32 microAptiv datasheet | MD00901 (Imagination Technologies) |
| Microchip Harmony 3 | https://github.com/Microchip-MPLAB-Harmony |
| FreeRTOS MIPS port | https://www.freertos.org/RTOS-MIPS-microAptiv.html |
| Vakulenko PIC32MZ fork (reference only) | https://github.com/sergev/qemu |
