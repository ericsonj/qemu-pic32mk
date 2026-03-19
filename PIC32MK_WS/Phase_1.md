
### Step 5 — Project file layout

Create your board files inside the QEMU tree on your branch:

```
qemu/
├── hw/mips/
│   ├── pic32mk.c              ← board init, memory map (Phase 1)
│   ├── pic32mk_evic.c         ← EVIC interrupt controller (Phase 2)
│   ├── pic32mk_uart.c         ← UART×6 (Phase 2)
│   ├── pic32mk_spi.c          ← SPI×4  (Phase 2)
│   ├── pic32mk_gpio.c         ← GPIO + PPS (Phase 2)
│   ├── pic32mk_timer.c        ← Timers 1–9 (Phase 2)
│   ├── pic32mk_dma.c          ← DMA×8 (Phase 2)
│   └── meson.build            ← register all new .c files here
├── include/hw/mips/
│   └── pic32mk.h              ← shared constants (SFR bases, register offsets)
└── tests/functional/
    └── test_pic32mk.py        ← Phase 4 validation tests
```

Register your files in `hw/mips/meson.build` — add one line per source file:
```meson
system_ss.add(when: 'CONFIG_PIC32MK', if_true: files(
  'pic32mk.c',
  'pic32mk_evic.c',
  'pic32mk_uart.c',
))
```

Enable the config symbol in `configs/targets/mipsel-softmmu.mak`:
```makefile
CONFIG_PIC32MK=y
```

---

### Step 6 — Minimal smoke-test firmware

Before implementing anything beyond the memory map, write a tiny MIPS assembly binary to confirm the CPU boots and reaches your reset vector (`0xBFC00000`):

```asm
/* test_boot.S — minimal PIC32MK boot test */
/* Assemble: mipsel-linux-gnu-gcc -march=mips32r2 -EL -nostdlib  */
/*           -Ttext=0x9FC40000 test_boot.S -o test_boot.elf       */
/* Strip:    mipsel-linux-gnu-objcopy -O binary test_boot.elf boot.bin */

    .section .text
    .set    noreorder
    .set    mips32r2
    .org    0x0

_reset:
    /* Disable interrupts, set kernel mode */
    mtc0    $zero, $12          /* CP0 Status = 0 */
    nop

    /* Load stack pointer to top of RAM
     * KSEG1 (uncached) RAM: 0xA0000000 + 256KB = 0xA0040000 */
    lui     $sp, 0xA004
    addiu   $sp, $sp, -4

    /* Spin forever — gives us a clean halt to verify boot */
_loop:
    nop
    b       _loop
    nop
```

Boot it in QEMU:
```bash
./qemu-system-mipsel \
    -M pic32mk \
    -bios path/to/boot.bin \
    -d unimp,guest_errors \
    -nographic \
    -serial stdio
```

The `-d unimp` flag prints every unimplemented SFR register access to stderr — that output becomes your Phase 2 implementation checklist.

---

### Step 7 — Set up remote GDB debugging

This is how you step through firmware running inside QEMU — essential throughout all four phases:

```bash
# Terminal 1 — launch QEMU with GDB stub on port 1234
./qemu-system-mipsel \
    -M pic32mk \
    -bios path/to/firmware.bin \
    -d unimp \
    -nographic \
    -s -S          # -s = GDB port 1234, -S = pause at reset vector

# Terminal 2 — connect GDB
gdb-multiarch path/to/firmware.elf
(gdb) set architecture mips
(gdb) set endian little
(gdb) target remote :1234
(gdb) break _reset
(gdb) continue
```

---

### Quick reference — CP0 config values for microAptiv

These come directly from datasheet pages 55–61 and must match when you register the CPU type in QEMU:

| Register | Field | Value | Meaning |
|---|---|---|---|
| `CONFIG` (sel 0) | `BE` | 0 | Little-endian |
| `CONFIG1` (sel 1) | `FP` | 1 | FPU present |
| `CONFIG1` (sel 1) | `EP` | 1 | EJTAG present |
| `CONFIG1` (sel 1) | `PC` | 1 | Performance counters present |
| `CONFIG1` (sel 1) | `MMUSIZE` | 0 | No TLB (fixed-map MPU) |
| `CONFIG3` (sel 3) | `DSPP` | 1 | DSP ASE rev2 present |
| `CONFIG3` (sel 3) | `MCU` | 1 | MCU ASE present |
| `CONFIG3` (sel 3) | `VEIC` | 1 | EIC vectored interrupts |
| `CONFIG3` (sel 3) | `VINT` | 1 | Vectored interrupt mode |
| `CONFIG3` (sel 3) | `ISA` | 10b | Both MIPS32 + microMIPS |
| `CONFIG3` (sel 3) | `IPLW` | 01b | 8-bit IPL/RIPL width |
| `CONFIG5` (sel 5) | `NF` | 1 | Nested fault implemented |

When you're ready to implement the CPU type registration or any specific peripheral, ask and I'll provide the exact QEMU device model C code for it.
