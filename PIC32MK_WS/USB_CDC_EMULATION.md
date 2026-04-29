# PIC32MK USB CDC Emulation — Phase 4B

**Status:** Complete and verified
**Deliverable:** `[CDC] Hello #N\r\n` visible on host PTY every second.

---

## Overview

Phase 4B adds full-speed USB CDC-ACM device emulation to the PIC32MK QEMU
board model.  The QEMU emulator simulates a USB host that enumerates the
firmware's USB device and then drains CDC TX packets to a host character
device (PTY or UNIX socket).

The firmware side runs the unmodified Microchip Harmony 3 USB stack
(`DRV_USBFS` + `USB_DEVICE` + `USB_DEVICE_CDC`), compiled into the
`tests/freertos/` FreeRTOS image.

---

## Architecture

```
┌────────────────────────────────────────────────────────────────┐
│  QEMU host process                                             │
│                                                                │
│  ┌──────────────────┐     ┌──────────────────────────────────┐ │
│  │  MIPS CPU model  │────▶│  pic32mk_usb.c  (USB emulator)   │ │
│  │  (FreeRTOS +     │     │                                  │ │
│  │   Harmony stack) │◀────│  • SFR MMIO (4 KB, read/write)   │ │
│  └──────────────────┘     │  • QEMUTimer  (1 ms poll)        │ │
│         ▲    │            │  • EP0 state machine             │ │
│         │    │            │  • BDT read/write via            │ │
│         │    ▼            │    cpu_physical_memory_*()       │ │
│  ┌──────────────────┐     │  • chardev write (CDC TX)        │ │
│  │  EVIC model      │     └──────────────┬───────────────────┘ │
│  │  (IRQ dispatch)  │                    │                     │
│  └──────────────────┘                    │                     │
│                                          ▼                     │
│                                 ┌─────────────────┐            │
│                                 │  CharFrontend   │            │
│                                 │  (pty, socket,  │            │
│                                 │   or stdio)     │            │
│                                 └────────┬────────┘            │
└──────────────────────────────────────────┼─────────────────────┘
                                           │
                               /dev/pts/N  │  (or named socket)
                                           │
                               ┌───────────▼──────────┐
                               │  Host terminal       │
                               │  screen /dev/pts/N   │
                               └──────────────────────┘
```

---

## QEMU Source Files

| File | Role |
|---|---|
| [hw/mips/pic32mk_usb.c](../hw/mips/pic32mk_usb.c) | USB OTG device model — SFR stub, enumeration timer, BDT helpers, chardev |
| [include/hw/mips/pic32mk_usb.h](../include/hw/mips/pic32mk_usb.h) | Device state struct, register offsets, EP0 state enum |
| [hw/mips/pic32mk.c](../hw/mips/pic32mk.c) | Board init — instantiates USB1 with `"usbcdc"` chardev property |

### Register map

USB1 base: `0xBF889000` (SFR offset `0x089000`).
USB2 base: `0xBF88A000` (SFR offset `0x08A000`).
Each instance maps `0x1000` bytes of MMIO.

All registers follow the standard PIC32 SET/CLR/INV convention
(`base+0` = write, `base+4` = CLR, `base+8` = SET, `base+C` = INV)
except W1C registers (UxIR, UxEIR, UxOTGIR) which only support CLR (`base+4`).

Key registers implemented:

| Register | Offset | Description |
|---|---|---|
| UxOTGIR | 0x040 | OTG interrupt flags (W1C) |
| UxOTGIE | 0x050 | OTG interrupt enable |
| UxOTGSTAT | 0x060 | OTG status — SESVD (bit 3) read-only |
| UxOTGCON | 0x070 | OTG control |
| UxPWRC | 0x080 | Power control — USBPWR (bit 0) triggers enumeration |
| UxIR | 0x200 | USB interrupt flags (W1C) |
| UxIE | 0x210 | USB interrupt enable |
| UxEIR | 0x220 | Error interrupt flags (W1C) |
| UxEIE | 0x230 | Error interrupt enable |
| UxSTAT | 0x240 | Last transaction status (read-only, set by emulator) |
| UxCON | 0x250 | USB control (USBEN, PPBRST, RESUME…) |
| UxADDR | 0x260 | Device address |
| UxBDTP1/2/3 | 0x270/2C0/2D0 | BDT base address pages |
| UxFRML/H | 0x280/290 | Frame counter (read-only) |
| UxEP0–EP15 | 0x300–0x3F0 | Endpoint control registers |

### Interrupt wiring

```
USB1 → EVIC source 34 → IFS1 bit 2 → IEC1 bit 2
USB2 → EVIC source 244 → IFS7 bit 20
```

IRQ numbers are defined in `include/hw/mips/pic32mk.h`:

```c
#define PIC32MK_IRQ_USB1   34
#define PIC32MK_IRQ_USB2   244
```

---

## BDT (Buffer Descriptor Table) Format

The PIC32MK USB hardware uses a software-managed BDT in RAM.
The firmware points the controller to it via UxBDTP1/2/3.
Each endpoint has **4 entries** (RX-even, RX-odd, TX-even, TX-odd),
each 8 bytes.

```
BDT base (512-byte aligned physical address)
 +0x000  EP0-OUT-even   ← SETUP / OUT transactions on EP0
 +0x008  EP0-OUT-odd
 +0x010  EP0-IN-even    ← IN descriptor data / ZLP from device
 +0x018  EP0-IN-odd
 +0x020  EP1-OUT-even
 +0x028  EP1-OUT-odd
 +0x030  EP1-IN-even    ← EP1 = CDC ACM interrupt IN (notification)
 +0x038  EP1-IN-odd
 +0x040  EP2-OUT-even
 +0x048  EP2-OUT-odd
 +0x050  EP2-IN-even    ← EP2 = CDC bulk IN (TX data from device)
 +0x058  EP2-IN-odd
```

### Entry layout (Harmony `DRV_USBFS_BDT_ENTRY` union)

```
 31          16  15        8   7   6   5    4   3   2   1   0
 ┌────────────┬─────────────┬───┬───┬────┬───┬───┬───┬───┬───┐
 │ byte count │  byte[1]   │UOW│D01│TOKPID[5:2]  │BST│   │   │
 │ shortWord  │  (padding) │ N │   │            │ALL│   │   │
 └────────────┴─────────────┴───┴───┴────────────┴───┴───┴───┘
   bits[31:16]   bits[15:8]   7    6   5  4  3  2   1   0

 word[1] = 32-bit physical buffer address
```

Key fields in `byte[0]`:

| Bit | Name | Meaning |
|---|---|---|
| 7 | UOWN | 1 = controller owns (firmware armed it); 0 = firmware owns |
| 6 | DATA01 | Data toggle (DATA0 / DATA1) |
| 5:2 | TOK_PID | Token PID returned by hardware on completion |

TOK_PID values used in Harmony's TRNIF switch (`byte[0] & 0x3C`):

| Value | PID | Transaction type |
|---|---|---|
| 0x34 | 0xD | SETUP |
| 0x04 | 0x1 | OUT |
| 0x24 | 0x9 | IN |

**Byte count** is in `shortWord[1]` = bits [31:16] of the 32-bit control
word (NOT in the lower 10 bits as one might expect from a naive reading).

---

## EP0 Enumeration State Machine

The QEMU timer (`usb_timer_cb`) drives a software host that simulates the
USB enumeration sequence entirely without a real USB host controller.

```
IDLE
 │  (UxPWRC.USBPWR written → schedule 50 ms)
 ▼
RESET ─────────────────────────────────────────────────────────────
 │  Set UxOTGSTAT.SESVD=1, fire UxOTGIR.SESSION_VALID             │
 │  Fire UxIR.URSTIF                                               │
 │  Purpose: Harmony ISR gate `!vbusIsValid || !isAttached`        │
 │  must pass before any transaction is processed.                 │
 │  (→ 10 ms)                                                      │
 ▼
GET_DEV_DESC ─────────────────────────────────────────────────────
 │  Inject SETUP: GET_DESCRIPTOR(Device, wLength=18)               │
 │  Write 8-byte SETUP into EP0-OUT buffer, set byte[0]=0x34,      │
 │  shortWord[1]=8, fire TRNIF with UxSTAT=EP0/OUT/PPBIn           │
 │  (→ 5 ms on success, retry every 2 ms)                          │
 ▼
WAIT_DEV_DESC ────────────────────────────────────────────────────
 │  Poll EP0-IN (both ping-pong): accept 18-byte descriptor,        │
 │  set byte[0]=0x24, preserve bc, fire TRNIF with UxSTAT=IN       │
 │  When no more IN pending: fire status-OUT ZLP                    │
 │  (byte[0]=0x04, bc=0 in shortWord[1], fire TRNIF)               │
 ▼
SET_ADDRESS ──────────────────────────────────────────────────────
 │  Inject SETUP: SET_ADDRESS(1)                                    │
 │  Also write UxADDR=1 immediately                                 │
 ▼
WAIT_ADDRESS ─────────────────────────────────────────────────────
 │  Accept EP0-IN ZLP (status phase, bc=0, ppbi=1 after ping-pong  │
 │  toggle)                                                         │
 ▼
GET_CFG_DESC ─────────────────────────────────────────────────────
 │  Inject SETUP: GET_DESCRIPTOR(Configuration, wLength=67)         │
 ▼
WAIT_CFG_DESC ────────────────────────────────────────────────────
 │  Accept multi-packet IN: 64 bytes (ppbi=0) + 3 bytes (ppbi=1)   │
 │  Then fire status-OUT ZLP                                        │
 ▼
SET_CONFIG ───────────────────────────────────────────────────────
 │  Inject SETUP: SET_CONFIGURATION(1)                              │
 ▼
WAIT_CONFIG ──────────────────────────────────────────────────────
 │  Accept EP0-IN ZLP status (ppbi=0)                               │
 │  Set s->configured = true                                        │
 ▼
DONE ─────────────────────────────────────────────────────────────
    Poll usb_check_cdc_bdt() every 1 ms
    Advance frame counter (UxFRML/H) by 1 per tick
    (Also: opportunistic TX drain on every TRNIF W1C clear)
```

### Ping-pong handling

The Harmony driver toggles `endpointObj->nextPingPong` after every
`F_DRV_USBFS_DEVICE_EndpointBDTEntryArm()` call.  This means consecutive
transactions on the same endpoint alternate between the even and odd BDT
entries.

All three BDT access helpers (`usb_inject_setup`, `usb_accept_ep0_in`,
`usb_send_status_out`) scan **both** even and odd entries, write to
whichever has UOWN=1, and set `UxSTAT.PPBI` (bit 2) accordingly so the
TRNIF handler uses the correct BDT entry.

Example — observed sequence for a full enumeration:

```
GET_DEV_DESC SETUP    EP0-OUT ppbi=0  (UxSTAT=0x00)
GET_DEV_DESC IN       EP0-IN  ppbi=0  (UxSTAT=0x08)  bc=18
GET_DEV_DESC status   EP0-OUT ppbi=1  (UxSTAT=0x04)  bc=0
SET_ADDRESS  SETUP    EP0-OUT ppbi=0  (UxSTAT=0x00)
SET_ADDRESS  status   EP0-IN  ppbi=1  (UxSTAT=0x0C)  bc=0
GET_CFG_DESC SETUP    EP0-OUT ppbi=1  (UxSTAT=0x04)
GET_CFG_DESC IN[0]    EP0-IN  ppbi=0  (UxSTAT=0x08)  bc=64
GET_CFG_DESC IN[1]    EP0-IN  ppbi=1  (UxSTAT=0x0C)  bc=3
GET_CFG_DESC status   EP0-OUT ppbi=0  (UxSTAT=0x00)  bc=0
SET_CONFIG   SETUP    EP0-OUT ppbi=1  (UxSTAT=0x04)
SET_CONFIG   status   EP0-IN  ppbi=0  (UxSTAT=0x08)  bc=0
```

### Critical emulation details

**SESSION_VALID must precede USB Reset.**
`F_DRV_USBFS_DEVICE_Tasks_ISR()` contains an early-exit gate:

```c
if (!hDriver->vbusIsValid || !hDriver->isAttached) {
    /* clear all flags and return — URSTIF never processed */
}
```

`vbusIsValid` is set only when `USB_OTG_INT_SESSION_VALID` fires AND
`UxOTGSTAT.SESVD` is 1 (PLIB_USB_OTG_SessionValid() reads this bit).
The emulator sets both before asserting URSTIF.

**`USB_DEVICE_Open()` requires `USB_DEVICE_Tasks()` to run first.**
`USB_DEVICE_Open()` checks `usbDeviceInstanceState == SYS_STATUS_READY`,
which is only set inside `USB_DEVICE_Tasks()` during the
`USB_DEVICE_TASK_STATE_OPENING_USBCD → USB_DEVICE_TASK_STATE_RUNNING`
transition.  Calling `Open()` from `USB1_Initialize()` (before the
scheduler starts) always returns `USB_DEVICE_HANDLE_INVALID`.

---

## CDC TX Data Path

Once enumeration reaches `EP0_SIM_DONE`, `usb_check_cdc_bdt()` is called
every 1 ms (timer) and also opportunistically on every TRNIF W1C clear:

```
FreeRTOS vUsbCdcTask
  │
  └─▶ USB_DEVICE_CDC_Write()
        │
        └─▶ DRV_USBFS_DEVICE_IRPSubmit(EP2-IN)
              │
              └─▶ F_DRV_USBFS_DEVICE_EndpointBDTEntryArm()
                    │  sets EP2-IN-even/odd UOWN=1, bc=len
                    │
                    ▼
              [BDT at RAM offset 0x50 or 0x58 from BDT base]

QEMU 1 ms timer tick  (+ opportunistic call on TRNIF clear)
  │
  └─▶ usb_check_cdc_bdt()
        │  reads EP2-IN-even/odd: find UOWN=1
        │  cpu_physical_memory_read(addr, buf, bc)
        │  qemu_chr_fe_write_all(&s->chr, buf, bc)
        │  returns BDT: byte[0]=0x24 (IN PID), UOWN=0, preserve bc
        │  fires TRNIF with UxSTAT=0x28 (EP=2, DIR=IN, PPBI=0/1)
        │
        └─▶ Harmony ISR: case 0x24 → nPendingBytes=0 → IRP complete
                │
                └─▶ USB_DEVICE_CDC_EVENT_WRITE_COMPLETE callback
                      → s_cdc_tx_busy = false
                      → vUsbCdcTask can send next frame
```

---

## Firmware Source Files

| File | Role |
|---|---|
| [tests/freertos/usb_init.c](../tests/freertos/usb_init.c) | USB1_Initialize, vUsbDeviceTask, USB_SendFrame |
| [tests/freertos/usb_init.h](../tests/freertos/usb_init.h) | Public API |
| [tests/freertos/usb_device_init_data.c](../tests/freertos/usb_device_init_data.c) | USB descriptors, endpoint config |
| [tests/freertos/main.c](../tests/freertos/main.c) | vUsbCdcTask, USB1_Initialize() call |
| `tests/freertos/drv_usbfs.c` | Harmony USBFS driver (copied from firmware project) |
| `tests/freertos/drv_usbfs_device.c` | USBFS device mode driver |
| `tests/freertos/usb_device.c` | USB device layer |
| `tests/freertos/usb_device_cdc.c` | CDC function driver |
| `tests/freertos/usb_device_cdc_acm.c` | CDC ACM subclass |

### USB descriptor summary

```
Configuration 1
  Interface 0 — CDC Communication (bInterfaceClass=0x02)
    EP0  — Control
    EP1 IN  — Interrupt, 8 bytes   (CDC ACM notification)
  Interface 1 — CDC Data (bInterfaceClass=0x0A)
    EP2 OUT — Bulk, 64 bytes       (host→device data, not used by test)
    EP2 IN  — Bulk, 64 bytes       (device→host data = CDC TX)
```

### Interrupt wiring in firmware

```
EVIC source 34 = USB1
  → IFS1 bit 2  (_IFS1_USB1IF_MASK)
  → IEC1 bit 2  (_IEC1_USB1IE_MASK)
  → IPC8 bits [20:18] for priority (_IPC8_USB1IP_POSITION = 18)

crt0.S ISR dispatch:
  IFS1 loaded → bit 2 set → branch _usb1_int
  → la $26, vUSB1InterruptWrapper → jr $26

port_asm_patched.S:
  vUSB1InterruptWrapper:
    portSAVE_CONTEXT
    jal USB1_InterruptHandler     ← calls DRV_USBFS_Tasks_ISR()
    portRESTORE_CONTEXT
```

### vUsbDeviceTask — three-phase startup

```c
void vUsbDeviceTask(void *pvParam)
{
    /* Phase 1: spin until USB_DEVICE_Tasks() advances the internal
     * state machine to SYS_STATUS_READY, then Open() succeeds. */
    for (;;) {
        USB_DEVICE_Tasks(s_dev_obj);
        s_dev_hdl = USB_DEVICE_Open(...);
        if (s_dev_hdl != USB_DEVICE_HANDLE_INVALID) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Phase 2: register application event handler; attach to bus.
     * Attach writes UxIE=0xDF enabling all USB interrupt sources. */
    USB_DEVICE_EventHandlerSet(s_dev_hdl, usb_device_event_handler, 0);
    USB_DEVICE_Attach(s_dev_hdl);

    /* Phase 3: normal 10 ms service loop. */
    for (;;) {
        USB_DEVICE_Tasks(s_dev_obj);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

## Running

### Build

```bash
# QEMU
mkdir -p build && cd build
../configure --target-list=mipsel-softmmu \
    --enable-debug --disable-werror --enable-trace-backends=log
make -j$(nproc)
cd ..

# Firmware
cd tests/freertos && make
```

### Run with virtual CDC serial port

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/freertos/hello_freertos.bin \
    -serial stdio -nographic -monitor none \
    -chardev pty,id=usbcdc
```

QEMU prints:
```
char device redirected to /dev/pts/6 (label usbcdc)
PIC32MK QEMU booting FreeRTOS...
[00:00:00] Hello from FreeRTOS!
...
```

Connect to the CDC port:
```bash
screen /dev/pts/6
```

Expected CDC output (after ~20 s for enumeration + initial delay):
```
[CDC] Hello #1
[CDC] Hello #2
[CDC] Hello #3
```

### Alternative: pipe CDC to a file or netcat

```bash
# Pipe to file
-chardev file,id=usbcdc,path=/tmp/cdc.log

# Expose as TCP socket (connect with nc localhost 4444)
-chardev socket,id=usbcdc,host=127.0.0.1,port=4444,server=on,wait=off
```

### UART and CDC output simultaneously

```
┌── stdio ─────────────────────────────────────────────┐
│ PIC32MK QEMU booting FreeRTOS...                     │
│ [00:00:00] Hello from FreeRTOS!    (UART1, 1 Hz)     │
│ Ping                               (UART2 echo)       │
│ [CAN] TX #1 id=0x100 data='hello' (CAN1 task)        │
└──────────────────────────────────────────────────────┘

┌── /dev/pts/N (USB CDC) ──────────────────────────────┐
│ [CDC] Hello #1                     (USB CDC, 1 Hz)   │
│ [CDC] Hello #2                                        │
│ [CDC] Hello #3                                        │
└──────────────────────────────────────────────────────┘
```

---

## Key Bugs Fixed During Development

These are non-obvious issues that future maintainers should be aware of.

### 1 — `USB_DEVICE_Open()` must follow `USB_DEVICE_Tasks()`

`USB_DEVICE_Open()` returns `USB_DEVICE_HANDLE_INVALID` until the internal
state machine reaches `USB_DEVICE_TASK_STATE_RUNNING`, which only happens
inside `USB_DEVICE_Tasks()`.  Calling `Open()` before the scheduler starts
(e.g. in `USB1_Initialize()`) always fails.

**Fix:** `vUsbDeviceTask()` polls `USB_DEVICE_Tasks()` in a loop before
calling `Open()`.

### 2 — Harmony ISR early exit requires VBUS session valid

`F_DRV_USBFS_DEVICE_Tasks_ISR()` contains:

```c
if (!hDriver->vbusIsValid || !hDriver->isAttached) {
    /* clear all interrupt flags and return */
}
```

`vbusIsValid` is only set when `USB_OTG_INT_SESSION_VALID` fires AND
`UxOTGSTAT.SESVD` (bit 3) is 1.  Without this, every TRNIF (including
USB Reset) is silently discarded.

**Fix:** `EP0_SIM_RESET` sets `s->otgstat |= 0x08` and
`s->otgir |= 0x08` before asserting `URSTIF`.

### 3 — BDT byte count is in `shortWord[1]` (bits 31:16), not bits 9:0

The `DRV_USBFS_BDT_ENTRY` union uses `shortWord[1]` (the upper 16 bits of
the 32-bit control word) for the byte count, not the lower 10 bits.
Using `ctrl & 0x3FF` always reads zero; the correct access is `ctrl >> 16`.

**Fix:** All BDT helpers updated to use `(uint32_t)bc << 16` for setting
and `ctrl >> 16` for reading the byte count.

### 4 — Returned BDT entries must carry the correct TOK_PID

Harmony's TRNIF handler dispatches on `byte[0] & 0x3C`:
- `0x34` → SETUP (PID=0xD)
- `0x04` → OUT   (PID=0x1)
- `0x24` → IN    (PID=0x9)

Without the correct TOK_PID, every returned entry hits the `default` case
("Unknown TOKEN") and the IRP queue is never advanced.

**Fix:** `usb_inject_setup` writes `byte[0] = 0x34`, `usb_send_status_out`
writes `byte[0] = 0x04`, and `usb_accept_ep0_in` / `usb_check_cdc_bdt`
write `byte[0] = 0x24`.

### 5 — Ping-pong BDT alternates after every arm operation

`F_DRV_USBFS_DEVICE_EndpointBDTEntryArm()` toggles `nextPingPong` after
every call.  Consecutive transactions on the same endpoint alternate
between the even and odd BDT entries.  Ignoring this causes the emulator
to read/write the wrong 8-byte slot, making the TRNIF handler see
`UOWN=0` and stall indefinitely.

**Fix:** All BDT helpers scan both even and odd entries and report the
correct PPBI in `UxSTAT` (bit 2).

### 6 — CDC TX is on EP2-IN, not EP1-IN

The USB descriptor assigns:
- EP1 IN = CDC ACM interrupt (notification, 8 bytes)
- EP2 IN = CDC data bulk (TX, 64 bytes)

The BDT offset for EP2-IN-even is `BDT + 0x50`, not `BDT + 0x30`.

**Fix:** `usb_check_cdc_bdt()` uses offset `0x50` and sets
`UxSTAT = 0x28` (EP=2, DIR=IN, PPBI=0).

### 7 — Missing USTAT FIFO caused write stalls (Phase 4B+)

Real PIC32MK hardware has a **4-deep USTAT FIFO** (DS60001519E §25.3.3).
Each completed USB transaction pushes a USTAT value onto the FIFO and
asserts TRNIF.  When firmware W1C-clears TRNIF, the FIFO pops; if more
entries remain, TRNIF is immediately re-asserted with the next USTAT value.

The original Phase 4B implementation used a single `s->ustat` register.
When two transactions completed in quick succession (e.g. `usb_chr_receive`
firing for EP2-OUT RX, then the timer draining EP2-IN TX), the second
overwrite destroyed the first USTAT value.  The firmware ISR processed
only the second transaction, permanently missing the first.  If the missed
transaction was a TX completion, the firmware stayed stuck in
`WAIT_FOR_WRITE_COMPLETE` — stalling all further protocol communication.

**Root cause:** Missing USTAT FIFO → lost transactions → write deadlock.

**Fix (`pic32mk_usb.h` + `pic32mk_usb.c`):**

Added a 4-entry circular FIFO to `PIC32MKUSBState`:

```c
/* pic32mk_usb.h */
uint32_t stat_fifo[4];
int      stat_fifo_head;
int      stat_fifo_tail;
int      stat_fifo_count;
```

Two helper functions manage the FIFO:

- `usb_stat_fifo_push(s, stat_val)` — pushes a USTAT value, exposes the
  front via `s->ustat`, and asserts `TRNIF`.  Logs overflow if FIFO full.
- `usb_stat_fifo_pop(s)` — called when firmware W1C-clears TRNIF.  Advances
  the tail.  If entries remain, re-exposes the next value and re-asserts
  TRNIF.

All direct `s->ustat = ...; s->uir |= TRNIF;` assignments were replaced
with `usb_stat_fifo_push()` calls across all 6 sites:
`usb_inject_setup`, `usb_accept_ep0_in`, `usb_send_status_out`,
`usb_chr_receive`, `usb_check_cdc_bdt` (EP1-IN + EP2-IN).

The UIR W1C handler now calls `usb_stat_fifo_pop(s)` when TRNIF is cleared:

```c
case PIC32MK_UxIR:
    if (v & USB_IR_TRNIF) {
        usb_stat_fifo_pop(s);
    }
    apply_w1c(&s->uir, v, sub);
    /* + opportunistic CDC TX check (see bug #8) */
    ...
```

The FIFO is reset to zero in both `pic32mk_usb_reset()` and
`pic32mk_usb_init()`.

### 8 — 5 ms TX poll too slow for bootloader throughput

`EP0_SIM_DONE` polled `usb_check_cdc_bdt()` every 5 ms and drained only
one EP2-IN packet per tick.  This limited throughput to ~200 commands/second
and added up to 5 ms latency per response — enough to cause host-side
timeouts during rapid bootloader WRITE sequences.

**Fix:**

1. **Timer reduced from 5 ms to 1 ms** — polling `usb_check_cdc_bdt()` 5×
   faster.  Frame counter now increments by 1 per tick (matching real USB
   full-speed SOF rate of 1 ms).

2. **Opportunistic TX drain on TRNIF clear** — when firmware W1C-clears
   TRNIF in the UIR write handler (meaning it just finished processing a
   transaction), we immediately call `usb_check_cdc_bdt()`.  If the firmware
   armed EP2-IN (response data) during that transaction's processing, the
   response is drained instantly rather than waiting up to 1 ms for the next
   timer tick.  This gives near-zero TX latency for the common case.

```c
case PIC32MK_UxIR:
    if (v & USB_IR_TRNIF) {
        usb_stat_fifo_pop(s);
    }
    apply_w1c(&s->uir, v, sub);
    if (s->configured) {
        usb_check_cdc_bdt(s);  /* opportunistic drain */
    }
    usb_update_irq(s);
    return;
```

---

## Limitations and Future Work

| Item | Notes |
|---|---|
| CDC RX (host→device) not implemented | `usb_check_cdc_bdt` only drains TX. RX would require reading host input from the chardev and injecting it into EP2-OUT BDT entries. |
| EP1 interrupt IN not drained | CDC ACM notifications (EP1-IN) are ignored. The firmware will stall if it queues them without a TRNIF ack, but in practice Harmony only sends them on line-state changes which don't happen in this test. |
| Frame counter increments by 1 per 1 ms | Matches real USB full-speed SOF rate. Harmony does not currently use the frame counter in device mode for any critical timing. |
| USB2 instance has no chardev | `pic32mk_usb_create(s, PIC32MK_USB2_OFFSET, PIC32MK_IRQ_USB2, NULL)` — USB2 is a pure SFR stub. |
| No suspend/resume emulation | `USB_PWRC_USUSPND` writes are accepted but ignored. |
| No host-mode support | `USB_CON_HOSTEN` is stored but has no effect. |
