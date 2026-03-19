# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## What this project is

A full QEMU board emulation for the **Microchip PIC32MK GPK/MCM with CAN FD** family
(datasheet DS60001519E). The goal is firmware development and testing without hardware.
The emulator is built as a new board target inside mainline QEMU.

Reference documents:
- Datasheet: DS60001519E (783 pages) — at `DS60001519E.pdf` / `PIC32MK_WS/DS60001519E.pdf`
- Effort estimate: `PIC32MK_WS/Effort.md`
- FreeRTOS Hello World plan: `PIC32MK_WS/PLAN_FreeRTOS_HelloWould_on_PIC32MK_QEMU.md`

## Focus microcontroller: PIC32MK1024MCM100

Here are the key parametric details for the PIC32MK1024MCM100 device:

| Parameter                  | Value/Description       |
|----------------------------|-------------------------|
| Part Number                | PIC32MK1024MCM100T-E/PT |
| Core                       | MCU32 (MIPS32r2)        |
| Max Clock Speed (MHz)      | 120                     |
| Program Memory (KB)        | 1024 (ECC Flash)        |
| SRAM (KB)                  | 256                     |
| Pin Count                  | 100                     |
| Package Type               | TQFP                    |
| Supply Voltage Min (V)     | 2.8                     |
| Supply Voltage Max (V)     | 3.3                     |
| Temp. Range Min (°C)       | -40                     |
| Temp. Range Max (°C)       | 125                     |
| UART                       | 6                       |
| SPI                        | 6                       |
| I2C                        | 4                       |
| USB                        | Full-Speed USB          |
| CAN-FD                     | 4                       |
| ADC                        | 12-bit                  |
| Motor Control              | Yes                     |
| Crypto Engine              | No                      |
| Secure Boot                | No                      |


For further details, refer to the datasheet linked above.

---

## Build commands

```bash
# Configure (run once from repo root, builds only mipsel-softmmu)
mkdir -p build && cd build
../configure --target-list=mipsel-softmmu \
    --enable-debug --disable-werror --enable-trace-backends=log
make -j$(nproc)

# Quick rebuild after source changes
make -C build -j$(nproc)
```

Cross-compiler: `mipsel-linux-gnu-gcc`

---

## Running the emulator

```bash
# Run FreeRTOS Hello World (from repo root after build)
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/freertos/hello_freertos.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors

# Run minimal boot firmware
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/firmware/boot.bin \
    -nographic -d unimp,guest_errors
```

---

## Building test firmware

```bash
# Minimal boot firmware
cd tests/firmware
make     # produces boot.bin

# FreeRTOS Hello World
cd tests/freertos
make     # produces hello_freertos.bin
make run # builds + launches QEMU immediately
```

FreeRTOS sources are expected at:
`/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/third_party/rtos/FreeRTOS/Source`

---

## CPU facts (from datasheet §3, pp. 54–61)

| Property | Value |
|---|---|
| Core | MIPS32® microAptiv™ MCU |
| ISA | MIPS32 Release 2 + microMIPS |
| Endianness | Little-endian (CONFIG.BE = 0) |
| Clock | 120 MHz max (198 DMIPS) |
| FPU | Present (CONFIG1.FP = 1) |
| DSP ASE | Revision 2 (CONFIG3.DSP2P=1, DSPP=1) |
| MMU/TLB | None — fixed MPU (CONFIG1.MMUSIZE = 0) |
| Interrupts | EIC vectored mode (CONFIG3.VEIC=1, VINT=1) |
| QEMU CPU type | **74Kf** (supports DSP ASE r2 — M14K lacks it and traps on lwx/mfhi $ac1) |

---

## Memory map (from datasheet §4, pp. 70–73)

### Physical addresses

| Region | Physical Base | Size | KSEG1 Virtual |
|---|---|---|---|
| RAM | 0x00000000 | 256 KB | 0xA0000000 |
| Program Flash | 0x1D000000 | 1 MB | 0xBD000000 |
| Boot Flash 1 | 0x1FC40000 | **256 KB** | 0xBFC40000 |
| Boot Flash 2 | 0x1FC60000 | ~20 KB | 0xBFC60000 |
| SFR window | 0x1F800000 | 1 MB | 0xBF800000 |
| Reset vector | — | — | 0xBFC00000 |

Boot: 0xBFC00000 trampoline jumps to 0xBFC40000 (BFLASH1). EBASE = 0xBFC40000.

### SFR peripheral offsets (Table 4-2, p. 73)

| Peripheral block | Base |
|---|---|
| CFG/PMD/CACHE/NVM/WDT/CRU/PPS | 0xBF800000 |
| EVIC | 0xBF810000 |
| DMA | 0xBF811000 |
| Timers 1–9, SPI1–2, UART1–2, I2C1–2 | 0xBF820000 |
| I2C3–4, SPI3–6, UART3–6 | 0xBF840000 |
| GPIO (PORTA–PORTG) | 0xBF860000 |
| CAN1–4, ADC | 0xBF880000 |

---

## Interrupt controller (EVIC, §8, pp. 121–166)

- 216 interrupt sources, up to 190 vectors; 7 priority × 4 subpriority levels
- **SET/CLR/INV convention**: for every register bank, +4 = SET, +8 = CLR, +C = INV
- Single-vector (INTCON.MVEC=0) and multi-vector (MVEC=1) modes
- Interrupt vectors at EBASE+0x180 (general exception) and EBASE+0x200 (vectored)

Key EVIC register offsets from 0xBF810000:

| Register | Offset |
|---|---|
| INTCON | 0x0000 |
| IFSx (x=0–7) | 0x0040+ |
| IECx (x=0–7) | 0x00C0+ |
| IPCx (x=0–63) | 0x0140+ |
| OFFx (x=0–190) | 0x0540+ |

---

## Phase plan and status

| Phase | Scope | Status |
|---|---|---|
| Phase 1 | CPU core, memory map, reset/boot | **COMPLETE** |
| Phase 2A | EVIC, UART1, Timer1, FreeRTOS Hello World running | **COMPLETE** |
| Phase 2B | Remaining UART×5, SPI×6, I2C×4, GPIO A–G, Timers 2–9, DMA×8 | NOT STARTED |
| Phase 3 | PWM×12, QEI×6, CAN FD×4, ADC (7 modules), DAC×3 | NOT STARTED |
| Phase 4 | USB OTG×2, integration testing, firmware bring-up | NOT STARTED |

FreeRTOS Hello World milestone: UART1 TX output, Timer1 tick at ~1 kHz,
FreeRTOS preemptive scheduler all working on QEMU.

---

## QEMU source file layout

```
hw/mips/
├── pic32mk.c           — board init, memory map, device instantiation
├── pic32mk_evic.c      — EVIC interrupt controller (Phase 2A, functional)
├── pic32mk_uart.c      — UART×6 (UART1 functional)
├── pic32mk_timer.c     — Timers 1–9 (Timer1 functional, drives FreeRTOS tick)
├── pic32mk_gpio.c      — GPIO PORTA–PORTG (stub)
├── pic32mk_spi.c       — SPI×6 (stub)
├── pic32mk_i2c.c       — I2C×4 (stub)
├── pic32mk_dma.c       — DMA×8 (stub)
└── meson.build

include/hw/mips/
├── pic32mk.h           — all base addresses, SFR offsets, size constants
└── pic32mk_evic.h      — EVIC device interface + IRQ line indices

tests/firmware/         — minimal MIPS assembly boot/hello test firmware
tests/freertos/         — FreeRTOS Hello World (main.c, crt0.S, xc.h, link.ld, Makefile)
```

---

## Rules for this project

1. Always reference DS60001519E register definitions before implementing any peripheral —
   offsets and reset values must match exactly.
2. Use QEMU's device model API: `MemoryRegionOps`, `SysBusDevice`, `qemu_log_mask`, qdev properties.
3. Use `LOG_UNIMP` stubs for all unimplemented SFR ranges — never silent ignores.
4. Keep Phase scope in mind — don't implement Phase 3 features during Phase 2.
5. The Vakulenko PIC32MZ fork is a reference only — use mainline QEMU as the base.
6. Contribution target is `gitlab.com/qemu-project/qemu` (GitHub mirror is read-only).
