# PIC32MK Input Capture (IC) Emulation

## Overview

This document describes the full-stack emulation of the PIC32MK Input Capture (IC) peripheral,
from the QEMU device model through to the FreeRTOS test firmware. All 16 IC instances
(IC1–IC9 in the low SFR bank, IC10–IC16 in the high bank) are emulated with a complete
register model including a 4-entry FIFO, two IRQ outputs per instance (capture + error), and
a host-side capture injection mechanism via Unix socket.

Reference: DS60001519E §18 (Input Capture).

---

## Address Map

| Block  | Instances | Base Address  | SFR Offset | Stride |
|--------|-----------|---------------|------------|--------|
| Bank 1 | IC1–IC9   | 0xBF822000    | 0x022000   | 0x200  |
| Bank 2 | IC10–IC16 | 0xBF843200    | 0x043200   | 0x200  |

Register offsets within each 0x200-byte block:

| Offset | Register | Access |
|--------|----------|--------|
| +0x00  | ICxCON   | R/W with SET(+4)/CLR(+8)/INV(+C) |
| +0x10  | ICxBUF   | Read-only (FIFO pop; write ignored) |

ICxCON bit fields:

| Bits  | Name    | Description |
|-------|---------|-------------|
| [15]  | ON      | Module enable |
| [13]  | SIDL    | Stop in Idle mode |
| [9]   | FEDGE   | First edge select |
| [8]   | C32     | 32-bit timer capture mode |
| [7]   | ICTMR   | Timer select (TMR2 vs TMR3) |
| [6:5] | ICI     | Interrupt on every Nth capture (00=1st, 01=2nd, 10=3rd, 11=4th) |
| [4]   | ICOV    | Overflow flag (set on FIFO full) |
| [3]   | ICBNE   | Buffer not empty (set when FIFO has data) |
| [2:0] | ICM     | Capture mode (0=off, 1=every edge, 2–7=every Nth rising edge) |

---

## IRQ Table (Table 8-3, DS60001519E)

| Instance | Error IRQ | Capture IRQ | IFS Register / Bit |
|----------|-----------|-------------|---------------------|
| IC1      | 5         | 6           | IFS0[5], IFS0[6]   |
| IC2      | 10        | 11          | IFS0[10], IFS0[11] |
| IC3      | 15        | 16          | IFS0[15], IFS0[16] |
| IC4      | 20        | 21          | IFS0[20], IFS0[21] |
| IC5      | 25        | 26          | IFS0[25], IFS0[26] |
| IC6      | 77        | 78          | IFS2[13], IFS2[14] |
| IC7      | 81        | 82          | IFS2[17], IFS2[18] |
| IC8      | 85        | 86          | IFS2[21], IFS2[22] |
| IC9      | 89        | 90          | IFS2[25], IFS2[26] |
| IC10     | 197       | 198         | IFS6[5],  IFS6[6]  |
| IC11     | 200       | 201         | IFS6[8],  IFS6[9]  |
| IC12     | 203       | 204         | IFS6[11], IFS6[12] |
| IC13     | 206       | 207         | IFS6[14], IFS6[15] |
| IC14     | 209       | 210         | IFS6[17], IFS6[18] |
| IC15     | 212       | 213         | IFS6[20], IFS6[21] |
| IC16     | 215       | 216         | IFS6[23], IFS6[24] |

---

## QEMU Device Model

### Source Files

| File | Role |
|------|------|
| `hw/mips/pic32mk_ic.c`       | IC device model (SysBusDevice, FIFO, chardev inject) |
| `include/hw/mips/pic32mk.h`  | IC offset macros, register bit definitions, IRQ numbers |
| `hw/mips/pic32mk.c`          | Board instantiation of all 16 IC devices |
| `hw/mips/meson.build`        | Build system entry |

### Device Model Summary (`pic32mk_ic.c`)

Each IC instance is a `SysBusDevice` with:
- One 0x200-byte MMIO region mapped into the SFR window.
- Two IRQ outputs (index 0 = capture IRQ → EVIC, index 1 = error IRQ → EVIC).
- A 4-entry × 16-bit circular FIFO in device state.
- One optional `CharFrontend` for host-side capture injection.

FIFO semantics:
- **Push** (capture): if full → set ICOV, pulse error IRQ, return. Otherwise write to FIFO,
  set ICBNE, pulse capture IRQ.
- **Pop** (ICxBUF read): return head entry, advance pointer. If FIFO empties, clear ICBNE.
- **Reset** (ON 0→1): clear head/tail/count, clear ICBNE and ICOV.

### Chardev Capture Injection

Host code can inject synthetic capture events into any IC instance by writing 8-byte packets
to a Unix socket chardev.

**Packet format** (8 bytes, little-endian):

| Bytes | Field    | Description |
|-------|----------|-------------|
| [0]   | index    | IC instance number (1–16) |
| [1]   | flags    | bit0=error inject; bit1=32-bit pair (hi word valid) |
| [2–3] | val\_lo  | 16-bit capture value (low word), LE |
| [4–5] | val\_hi  | 16-bit capture value (high word, C32 mode), LE |
| [6–7] | reserved | Must be 0 |

On receipt:
- `flags.bit0 = 1` → set ICOV, pulse error IRQ (no FIFO push).
- Otherwise → push `val_lo` to FIFO; if `flags.bit1 && C32` also push `val_hi`.
- Pulse capture IRQ when FIFO is non-empty.

---

## IMPORTANT NOTE: Chardev Single-Owner Architecture

> **Background**: QEMU's `qemu_chr_fe_init()` calls `error_abort` if a chardev backend is
> already attached to another frontend. An early version of `pic32mk_ic_create()` in
> `pic32mk.c` tried to attach the `ic-events` chardev to every IC instance in a loop.
> This caused a hard crash at QEMU startup:
> ```
> qemu-system-mipsel: chardev 'ic-events' is already in use
> Aborted (core dumped)
> ```

**Current workaround — global routing table:**

`pic32mk_ic.c` maintains a module-level array:
```c
static struct PIC32MKICState *g_ic_instances[17]; /* index 1–16 */
```
Each instance registers itself in `pic32mk_ic_realize()`. The chardev (`ic-events`) is
attached **only to IC1**. IC1's `ic_chr_receive()` reads the `index` field from every
incoming packet and dispatches the call to the correct IC instance via this table.

This means: regardless of which instance the packet targets, the chardev path always goes
through IC1.

**Future improvement needed:**

This is a pragmatic workaround, not a clean architecture. It has several downsides:
- IC1 is special-cased in board code; the property is invisible to other instances.
- The global table is a module-level singleton — it will break if multiple PIC32MK
  machines are instantiated in the same process.
- There is no way to attach separate chardevs to individual IC instances.

Better approaches to consider:
1. **Mux chardev**: Use a single `mux` chardev hub that QEMU already supports for serial
   ports, adapted for fixed-width binary packets.
2. **IC hub device**: Introduce a separate `pic32mk-ic-hub` SysBusDevice that owns the
   chardev and holds references to all 16 IC instances, dispatching in its own receive handler.
3. **Per-instance chardevs**: Each IC instance accepts its own optional `CharFrontend`
   property (e.g., `ic2-events`, `ic3-events`, ...) — cleanest but verbose at the CLI.

The current implementation is sufficient for single-machine IC capture testing. The above
improvements should be applied before upstreaming or when multi-instance support is needed.

---

## Bugs Fixed During Implementation

### 1. I2C Offset Collision (pre-existing)

**Symptom**: IC2CON wrote silently to I2C1 address space; `con` register stayed 0 even after
`ICAP2_Initialize()` + `ICAP2_Enable()`.

**Root cause**: `PIC32MK_I2C1_OFFSET` and `PIC32MK_I2C2_OFFSET` were defined as `0x022000`
and `0x022200` respectively — identical to `PIC32MK_IC1_OFFSET` and `PIC32MK_IC2_OFFSET`.
The I2C device, added first during `pic32mk_machine_init()` with priority 1, won all MMIO
dispatch, silently dropping IC SFR writes.

**Fix** in `include/hw/mips/pic32mk.h`:
```c
/* Correct offsets per DS60001519E Table 4-2 (SFR base 0xBF820000) */
#define PIC32MK_I2C1_OFFSET     0x026000u   /* 0xBF826000 */
#define PIC32MK_I2C2_OFFSET     0x026200u
#define PIC32MK_I2C3_OFFSET     0x046400u
#define PIC32MK_I2C4_OFFSET     0x046600u
```

### 2. ICI=0 Suppressed IRQ

**Symptom**: After all MMIO/routing fixes, IC2 FIFO accumulated values but ISR never fired.
`[ICAP] IC2 armed` appeared but no `IC2 captured` output even after injection.

**Root cause**: `ic_inject_capture()` in `pic32mk_ic.c` had:
```c
uint32_t ici = (s->con & PIC32MK_ICCON_ICI_MASK) >> PIC32MK_ICCON_ICI_SHIFT;
if (ici != 0 && s->fifo_count > 0) { qemu_irq_pulse(s->irq_capture); }
```
The Harmony `ICAP2_Initialize()` sets `IC2CON = 0x1` (ICM=1, all other bits=0), so ICI=0
(= interrupt on every first capture). The condition `ici != 0` incorrectly suppressed
the IRQ for the most common configuration.

**Fix**: IRQ now fires whenever the FIFO is non-empty after a push:
```c
if (s->fifo_count > 0) {
    qemu_irq_pulse(s->irq_capture);
}
```

### 3. `.vectors` Section Overflow

**Symptom**: Firmware build error:
```
crt0.S:339: Error: attempt to move .org backwards
```

**Root cause**: Adding IC2 dispatch code (~64 bytes) to the interrupt handler at EBASE+0x200
caused the total code from 0x200 to overflow past 0x400, where `_crt_init` was placed with
`.org 0x400`.

**Fix**: Moved `_crt_init` from `.org 0x400` to `.org 0x600`, giving 512 bytes for
interrupt dispatch (0x200–0x400) and 512 bytes of padding/future vectors (0x400–0x600).

### 4. FreeRTOS Heap Exhaustion

**Symptom**: QEMU console printed `FATAL: malloc failed` twice at startup; tasks did not run.

**Root cause**: `configTOTAL_HEAP_SIZE = 24000` was insufficient for the expanded task set
(FreeRTOS timer task + UART tasks + ADC task + new ICAP task + their semaphores and stacks).

**Fix**: Increased `configTOTAL_HEAP_SIZE` to `40000` in
`tests/pic32mk_rtos_demo/firmware/src/config/default/FreeRTOSConfig.h`.

---

## Firmware Integration

### Harmony PLIB Files

Located in `tests/pic32mk_rtos_demo/firmware/src/config/default/peripheral/icap/`:

| File | Description |
|------|-------------|
| `plib_icap2.c` | ICAP2 implementation (verbatim from Harmony) |
| `plib_icap2.h` | ICAP2 API header (verbatim from Harmony) |
| `plib_icap_common.h` | Shared typedefs (`ICAP_CALLBACK`, `ICAP_OBJECT`, `ICAP_STATUS_SOURCE`) |

Key PLIB functions:
- `ICAP2_Initialize()`: sets `IC2CON = 0x1` (ICM=1, all else 0) and enables IEC0 IC2IE +
  IC2EIE bits.
- `ICAP2_Enable()`: sets `IC2CONSET = _IC2CON_ON_MASK` (turns on the module).
- `ICAP2_CaptureBufferRead()`: reads `IC2BUF` (FIFO pop, returns 16-bit value).
- `ICAP2_CallbackRegister()`: registers a user callback for capture events.
- `INPUT_CAPTURE_2_InterruptHandler()`: clears `IFS0_IC2IF`, calls registered callback.
- `INPUT_CAPTURE_2_ERROR_InterruptHandler()`: clears `IFS0_IC2EIF`, calls error callback.

### Interrupt Wiring (`crt0.S`)

The interrupt dispatch table at EBASE+0x200 checks IFS0 for IC2 bits:

```asm
/* IC2E = EVIC source 10 = IFS0 bit 10 = 0x400 */
/* IC2  = EVIC source 11 = IFS0 bit 11 = 0x800 */
lui     $26, 0xBF81
lw      $27, 0x0040($26)        /* IFS0 */
lw      $26, 0x00C0($26)        /* IEC0 */
and     $26, $27, $26
andi    $27, $26, 0x400         /* IC2EIF */
bnez    $27, _ic2_error_int
nop
andi    $27, $26, 0x800         /* IC2IF  */
bnez    $27, _ic2_capture_int
nop
```

### FreeRTOS Wrappers (`port_asm_patched.S`)

```asm
vIC2CaptureInterruptWrapper:
    portSAVE_CONTEXT
    jal     INPUT_CAPTURE_2_InterruptHandler
    nop
    portRESTORE_CONTEXT

vIC2ErrorInterruptWrapper:
    portSAVE_CONTEXT
    jal     INPUT_CAPTURE_2_ERROR_InterruptHandler
    nop
    portRESTORE_CONTEXT
```

### Demo Task (`main.c`)

```c
static void ic2_capture_callback(uintptr_t context)
{
    ic2_last_capture = ICAP2_CaptureBufferRead();
    xSemaphoreGiveFromISR(xIc2Semaphore, &xHigherPriorityTaskWoken);
}

static void vIcapDemoTask(void *pvParam)
{
    xIc2Semaphore = xSemaphoreCreateBinary();
    ICAP2_CallbackRegister(ic2_capture_callback, 0);
    ICAP2_Initialize();
    ICAP2_Enable();
    uart1_puts("[ICAP] IC2 armed (IRQ mode, ICM=1 every edge)\r\n");
    for (;;) {
        xSemaphoreTake(xIc2Semaphore, portMAX_DELAY);
        uart1_puts("[ICAP] IC2 captured 0x...\r\n");
    }
}
```

---

## Testing: Injecting Captures from the Host

### Start QEMU with Unix socket chardev

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -chardev socket,id=ic-events,path=/tmp/ic.sock,server=on,wait=off
```

The `-chardev socket,...,server=on,wait=off` option creates a Unix domain socket that
QEMU listens on. QEMU starts immediately without waiting for a client; connect at any time.

### Inject captures with Python

```python
import socket, struct, time

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/ic.sock')

# Inject three 16-bit captures into IC2
for val in [0x1234, 0x00AB, 0x7FFF]:
    # Packet: inst=2, flags=0 (normal capture), val_lo=val, val_hi=0, reserved=0
    pkt = struct.pack('<BBHHH', 2, 0, val, 0, 0)
    s.send(pkt)
    time.sleep(0.1)

s.close()
```

### Expected firmware output

```
[ICAP] IC2 armed (IRQ mode, ICM=1 every edge)
[ICAP] IC2 captured 0x1234
[ICAP] IC2 captured 0x00AB
[ICAP] IC2 captured 0x7FFF
```

### Inject an error (FIFO overflow)

```python
# flags=0x01 triggers error inject (sets ICOV, pulses error IRQ, no FIFO push)
pkt = struct.pack('<BBHHH', 2, 1, 0, 0, 0)
s.send(pkt)
```

### 32-bit capture (C32 mode)

When IC2CON.C32=1, two consecutive ICxBUF reads form one 32-bit timestamp:

```python
# flags=0x02 → push val_lo AND val_hi when C32=1
pkt = struct.pack('<BBHHH', 2, 0x02, 0xLO16, 0xHI16, 0)
s.send(pkt)
```

---

## Build

```bash
# Rebuild firmware
cd tests/pic32mk_rtos_demo
make clean && make

# Rebuild QEMU (after changing hw/mips/ sources)
make -C build -j$(nproc)
```

Firmware build adds to `srcs.mk`:
```makefile
CSRC += firmware/src/config/default/peripheral/icap/plib_icap2.c
INCS += -Ifirmware/src/config/default/peripheral/icap
```
