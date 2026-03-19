# PIC32MK QEMU — Functional Tests

## Overview

The PIC32MK board emulation (`-M pic32mk`) has functional tests that verify
the full interrupt-driven boot chain: CPU reset → FreeRTOS scheduler →
Timer1 tick → UART TX interrupt → serial output.

Tests are implemented using QEMU's standard `QemuSystemTest` framework
(Python `unittest`-based, TAP output).

## Test file

```
tests/functional/mipsel/test_pic32mk.py
```

## Test cases

| Test                          | What it proves                                                       |
|-------------------------------|----------------------------------------------------------------------|
| `test_boot_message`           | CPU boots, memory map works, UART TX outputs the banner              |
| `test_freertos_hello_task`    | FreeRTOS scheduler starts, Timer1 tick fires, UART TX ISR works      |
| `test_freertos_recurring_ticks` | Timer1 delivers multiple ticks, CS0 yield + context switches work  |
| `test_freertos_ping_task`     | Two FreeRTOS tasks run concurrently (proves multitasking)            |

## Prerequisites

1. **Build QEMU** (mipsel target):

   ```bash
   cd build
   make -j8 qemu-system-mipsel
   ```

2. **Build the FreeRTOS test firmware**:

   ```bash
   make -C tests/freertos
   ```

   This produces `tests/freertos/hello_freertos.bin`. If the binary is
   missing when tests run, they skip with a helpful message.

## Running tests

### Option 1: Direct (fastest, TAP output)

```bash
cd build
QEMU_TEST_QEMU_BINARY=./qemu-system-mipsel \
  PYTHONPATH=../python:../tests/functional \
  pyvenv/bin/python3 ../tests/functional/mipsel/test_pic32mk.py
```

Expected output:

```
TAP version 13
ok 1 test_pic32mk.PIC32MKMachine.test_boot_message
ok 2 test_pic32mk.PIC32MKMachine.test_freertos_hello_task
ok 3 test_pic32mk.PIC32MKMachine.test_freertos_ping_task
ok 4 test_pic32mk.PIC32MKMachine.test_freertos_recurring_ticks
1..4
```

### Option 2: Via meson (all mipsel quick tests)

```bash
cd build
pyvenv/bin/meson test --suite func-mipsel -v
```

This runs the PIC32MK tests alongside the generic mipsel quick tests
(empty_cpu_model, info_usernet, linters, version, vnc).

### Option 3: Only PIC32MK via meson

```bash
cd build
pyvenv/bin/meson test func-mipsel-pic32mk -v
```

### Option 4: Run a single test method

```bash
cd build
QEMU_TEST_QEMU_BINARY=./qemu-system-mipsel \
  PYTHONPATH=../python:../tests/functional \
  pyvenv/bin/python3 -m pytest \
  ../tests/functional/mipsel/test_pic32mk.py::PIC32MKMachine::test_boot_message
```

Or using unittest directly:

```bash
cd build
QEMU_TEST_QEMU_BINARY=./qemu-system-mipsel \
  PYTHONPATH=../python:../tests/functional \
  pyvenv/bin/python3 -m unittest \
  test_pic32mk.PIC32MKMachine.test_boot_message
```

## How the tests work

Each test follows the same pattern:

1. **`set_machine('pic32mk')`** — selects the PIC32MK board
   (test skips automatically if the machine is not compiled in)
2. **`vm.set_console()`** — attaches a socket to the UART serial port
3. **`vm.add_args('-bios', path)`** — loads the FreeRTOS firmware binary
4. **`vm.launch()`** — starts the QEMU process
5. **`wait_for_console_pattern(self, 'string')`** — blocks reading
   the serial socket until the expected string appears, or the 30-second
   timeout expires (which fails the test)

The firmware is located via a relative path from the test file up to
the source root: `tests/freertos/hello_freertos.bin`.

## meson.build registration

File: `tests/functional/mipsel/meson.build`

```meson
test_mipsel_timeouts = {
  'pic32mk' : 30,
}

tests_mipsel_system_quick = [
  'pic32mk',
]
```

The test is in `tests_mipsel_system_quick` so it runs during `make check`
— no network assets needed since the firmware is local.

## Debugging test failures

- **Console logs** are saved to:
  `build/tests/functional/mipsel/<test_id>/console.log`

- **QEMU logs** are saved to:
  `build/tests/functional/mipsel/<test_id>/base.log`

- **Keep scratch files** after a run:
  ```bash
  QEMU_TEST_KEEP_SCRATCH=1 pyvenv/bin/meson test func-mipsel-pic32mk -v
  ```

- **Run QEMU manually** to reproduce what the test does:
  ```bash
  cd build
  timeout 6 ./qemu-system-mipsel \
    -M pic32mk \
    -bios ../tests/freertos/hello_freertos.bin \
    -nographic \
    -serial stdio \
    -monitor none
  ```

  Expected output:
  ```
  PIC32MK QEMU booting FreeRTOS...
  [00:00:00] Hello from FreeRTOS!
  [00:00:01] Hello from FreeRTOS!
  [00:00:02] Hello from FreeRTOS!
  ...
  ```

## Adding new tests

To test a new peripheral or feature:

1. Add a new test method to `PIC32MKMachine` in `test_pic32mk.py`
2. Use `wait_for_console_pattern()` to assert expected serial output
3. If the test needs different firmware, build it separately and
   reference it via a new path helper
4. For interactive tests (e.g. UART RX), use
   `exec_command_and_wait_for_pattern(self, 'input', 'expected_response')`
