# PIC32MK QEMU — QTest Device-Level Tests

## Overview

The PIC32MK board emulation (`-M pic32mk`) has **QTest** device-level tests
that directly verify MMIO register behaviour, SET/CLR/INV atomic operations,
IRQ routing, and timer tick generation for the peripheral device models
(`pic32mk_uart.c`, `pic32mk_evic.c`, `pic32mk_timer.c`).

Unlike the Python functional tests (which boot firmware end-to-end and check
serial output), QTest runs **without firmware** — it talks directly to the
QEMU device model via a QTest protocol socket, reading and writing physical
MMIO addresses through `readl()` / `writel()`.

## Test file

```
tests/qtest/pic32mk-test.c
```

Registered in `tests/qtest/meson.build` under the `qtests_mipsel` list,
guarded by `CONFIG_PIC32MK`.

## Test cases

### UART1 tests (`/pic32mk/uart/...`)

| Test              | What it verifies                                                     |
|-------------------|----------------------------------------------------------------------|
| `reset-values`    | MODE=0, STA has TRMT set (shift register empty), BRG=0 after reset  |
| `set-clr-inv`     | Atomic SET (+0x08), CLR (+0x04), INV (+0x0C) register operations     |
| `enable`          | Setting ON, UTXEN, URXEN bits; TRMT stays 1, UTXBF stays 0          |
| `tx-write`        | Writing to TXREG doesn't crash; TRMT remains 1 (instantaneous TX)   |
| `rx-not-ready`    | URXDA=0 when no byte received; RXREG reads as 0 when empty          |

### EVIC tests (`/pic32mk/evic/...`)

| Test              | What it verifies                                                     |
|-------------------|----------------------------------------------------------------------|
| `reset-values`    | IFS0=0, IEC0=0, INTCON=0 after reset                                |
| `ifs-set-clr`     | SET/CLR of interrupt flag bits in IFS0 (Timer1 bit 4)                |
| `iec-set-clr`     | SET/CLR of interrupt enable bits in IEC0 (Timer1 bit 4)              |
| `ipc-priority`    | Writing and reading interrupt priority in IPC1                       |
| `uart-ifs1`       | IFS1 SET/CLR for UART1 sources: U1E (bit 6), U1RX (bit 7), U1TX (bit 8) |
| `intstat-readonly`| Writing to INTSTAT is silently ignored (read-only register)          |

### Timer1 tests (`/pic32mk/timer/...`)

| Test              | What it verifies                                                     |
|-------------------|----------------------------------------------------------------------|
| `reset-values`    | T1CON=0, TMR1=0, PR1=0xFFFF (16-bit max) after reset                |
| `on-off`          | Starting/stopping timer via T1CONSET/T1CONCLR ON bit                 |
| `period-register` | Writing PR1 and reading it back                                      |
| `prescaler-config`| Setting TCKPS bits [5:4] for 1:8 and 1:256 prescaler modes           |
| `tick-fires-irq`  | Configuring Timer1 with PR=9, stepping the clock, verifying IFS0.T1IF is set |

### Integration tests (`/pic32mk/integration/...`)

| Test              | What it verifies                                                     |
|-------------------|----------------------------------------------------------------------|
| `uart-tx-irq`     | UART1 ON+UTXEN → TX IRQ asserted → IFS1.U1TXIF set; CLR re-asserts while line high; disabling UTXEN deasserts |

**Total: 17 test cases**

## Prerequisites

Build QEMU with the mipsel target:

```bash
cd build
make -j8 qemu-system-mipsel
```

Build the QTest executable:

```bash
cd build
make -j8 tests/qtest/pic32mk-test
```

> **Note:** No firmware binary is needed — QTest drives the device model
> directly without booting the CPU.

## Running tests

### Option 1: Direct execution (fastest, verbose TAP output)

```bash
cd build
QTEST_QEMU_BINARY=./qemu-system-mipsel ./tests/qtest/pic32mk-test -v
```

Expected output:

```
TAP version 14
1..17
# Start of mipsel tests
# Start of pic32mk tests
# Start of uart tests
ok 1 /mipsel/pic32mk/uart/reset-values
ok 2 /mipsel/pic32mk/uart/set-clr-inv
ok 3 /mipsel/pic32mk/uart/enable
ok 4 /mipsel/pic32mk/uart/tx-write
ok 5 /mipsel/pic32mk/uart/rx-not-ready
# End of uart tests
# Start of evic tests
ok 6 /mipsel/pic32mk/evic/reset-values
ok 7 /mipsel/pic32mk/evic/ifs-set-clr
ok 8 /mipsel/pic32mk/evic/iec-set-clr
ok 9 /mipsel/pic32mk/evic/ipc-priority
ok 10 /mipsel/pic32mk/evic/uart-ifs1
ok 11 /mipsel/pic32mk/evic/intstat-readonly
# End of evic tests
# Start of timer tests
ok 12 /mipsel/pic32mk/timer/reset-values
ok 13 /mipsel/pic32mk/timer/on-off
ok 14 /mipsel/pic32mk/timer/period-register
ok 15 /mipsel/pic32mk/timer/prescaler-config
ok 16 /mipsel/pic32mk/timer/tick-fires-irq
# End of timer tests
# Start of integration tests
ok 17 /mipsel/pic32mk/integration/uart-tx-irq
# End of integration tests
# End of pic32mk tests
# End of mipsel tests
```

### Option 2: Via meson (all mipsel QTests)

```bash
cd build
pyvenv/bin/meson test --suite qtest-mipsel -v
```

This runs all mipsel QTests including `pic32mk-test`, `endianness-test`,
`qmp-cmd-test`, and others.

### Option 3: Only PIC32MK QTest via meson

```bash
cd build
pyvenv/bin/meson test qtest-mipsel/pic32mk-test -v
```

### Option 4: Run a single test case

Using GLib's `-p` flag to select a specific test path:

```bash
cd build
QTEST_QEMU_BINARY=./qemu-system-mipsel \
  ./tests/qtest/pic32mk-test -v -p /pic32mk/timer/tick-fires-irq
```

Run all tests matching a prefix:

```bash
cd build
QTEST_QEMU_BINARY=./qemu-system-mipsel \
  ./tests/qtest/pic32mk-test -v -p /pic32mk/uart/
```

### Option 5: List all test paths without running them

```bash
cd build
QTEST_QEMU_BINARY=./qemu-system-mipsel \
  ./tests/qtest/pic32mk-test -l
```

## How QTest works

QTest is QEMU's C-level device testing framework. Unlike functional tests
that boot real firmware, QTest operates at the MMIO register level:

1. **`qtest_start("-machine pic32mk")`** — launches QEMU with the QTest
   accelerator (no CPU execution, no firmware needed)
2. **`readl(phys_addr)`** — reads a 32-bit value from a physical address
3. **`writel(phys_addr, value)`** — writes a 32-bit value to a physical address
4. **`clock_step(nanoseconds)`** — advances the QEMU virtual clock by the
   specified duration (critical for timer expiry tests)
5. **`g_assert_cmphex(actual, ==, expected)`** — GLib assertion macro that
   prints both values in hex on failure

Communication happens over a Unix socket using the QTest protocol.
The test binary is a standalone executable that links against `libqtest`.

### Physical addresses vs KSEG1 virtual

QTest accesses physical memory directly. The PIC32MK firmware uses KSEG1
virtual addresses (e.g. `0xBF828000` for UART1), but QTest uses the
corresponding **physical** address (`0x1F828000`):

| Peripheral | KSEG1 (firmware) | Physical (QTest)  |
|------------|-------------------|-------------------|
| SFR base   | `0xBF800000`      | `0x1F800000`      |
| EVIC       | `0xBF810000`      | `0x1F810000`      |
| Timer1     | `0xBF820000`      | `0x1F820000`      |
| UART1      | `0xBF828000`      | `0x1F828000`      |

### SET/CLR/INV register convention

PIC32MK peripherals use a +0x00/+0x04/+0x08/+0x0C pattern for each
register group:

| Offset | Operation                         |
|--------|-----------------------------------|
| +0x00  | Direct read/write (base register) |
| +0x04  | CLR — clears bits (write `1` to clear) |
| +0x08  | SET — sets bits (write `1` to set)     |
| +0x0C  | INV — inverts bits (write `1` to toggle) |

Tests verify all four access modes for critical registers.

## Build system registration

In `tests/qtest/meson.build`, the test is added to `qtests_mipsel` with
a `CONFIG_PIC32MK` Kconfig guard:

```meson
qtests_mipsel = qtests_mips + \
  (config_all_devices.has_key('CONFIG_PIC32MK') ? ['pic32mk-test'] : [])
```

This ensures the test is only built and run when the PIC32MK board is
compiled into the `mipsel-softmmu` target.

## Debugging failures

### See assertion details

QTest uses GLib's `g_assert_cmphex` which prints both the actual and
expected values on failure:

```
ERROR:../tests/qtest/pic32mk-test.c:257:test_timer_reset_values:
  assertion failed (readl(PR1) == 0x0000FFFF): (0x00000000 == 0x0000FFFF)
```

### Enable QEMU device logging

Set the `QTEST_LOG` environment variable to see QEMU's stderr output
(including `LOG_UNIMP` messages):

```bash
cd build
QTEST_QEMU_BINARY=./qemu-system-mipsel \
QTEST_LOG=1 \
  ./tests/qtest/pic32mk-test -v -p /pic32mk/timer/tick-fires-irq
```

### Run under GDB

```bash
cd build
QTEST_QEMU_BINARY=./qemu-system-mipsel \
  gdb --args ./tests/qtest/pic32mk-test -v -p /pic32mk/timer/tick-fires-irq
```

### Attach GDB to the QEMU process

Start the test paused (add a breakpoint at `qtest_start`), then attach
to the spawned QEMU process in a separate terminal:

```bash
gdb -p $(pgrep -f "qemu-system-mipsel.*qtest")
```

## Related files

| File | Purpose |
|------|---------|
| `tests/qtest/pic32mk-test.c` | QTest device-level tests (this file) |
| `tests/qtest/meson.build` | Build registration |
| `hw/mips/pic32mk_uart.c` | UART device model under test |
| `hw/mips/pic32mk_evic.c` | EVIC interrupt controller under test |
| `hw/mips/pic32mk_timer.c` | Timer device model under test |
| `include/hw/mips/pic32mk.h` | Register addresses, bit definitions |
| `tests/functional/mipsel/test_pic32mk.py` | Python functional tests (end-to-end) |
| `tests/functional/mipsel/TESTING_PIC32MK.md` | Functional test documentation |
