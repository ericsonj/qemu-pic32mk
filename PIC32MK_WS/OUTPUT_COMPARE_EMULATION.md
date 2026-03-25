# PIC32MK Output Compare (OC) Peripheral Emulation

> **Datasheet:** DS60001519E, §19 — Output Compare
> **Device:** PIC32MK1024MCM100
> **Phase:** Diagnostic emulation (register storage + event streaming)

---

## Overview

The PIC32MK has 16 Output Compare modules (OC1–OC16). Each module compares
the current value of a timer against one or two compare registers and toggles
an output pin accordingly. Depending on the OCM mode bits, the module can
generate:

- A single pulse of configurable width
- A continuous train of pulses
- A PWM signal with configurable duty cycle
- Edge-triggered toggles on timer match

This QEMU emulation is **diagnostic only**: no actual pin toggling or waveform
timing occurs. Instead, when firmware enables or disables an OC instance, the
emulator records the register state and optionally streams a structured binary
event to a `chardev` for consumption by a GUI or analysis tool.

---

## Hardware Register Map

### Instance addresses

OC1–OC9 live in the peripheral-1 SFR block (base `0xBF820000`).
OC10–OC16 live in the peripheral-2 SFR block (base `0xBF840000`).

| Instance | KSEG1 Base Address | SFR Offset |
|----------|-------------------|------------|
| OC1  | `0xBF824000` | `0x024000` |
| OC2  | `0xBF824200` | `0x024200` |
| OC3  | `0xBF824400` | `0x024400` |
| OC4  | `0xBF824600` | `0x024600` |
| OC5  | `0xBF824800` | `0x024800` |
| OC6  | `0xBF824A00` | `0x024A00` |
| OC7  | `0xBF824C00` | `0x024C00` |
| OC8  | `0xBF824E00` | `0x024E00` |
| OC9  | `0xBF825000` | `0x025000` |
| OC10 | `0xBF845200` | `0x045200` |
| OC11 | `0xBF845400` | `0x045400` |
| OC12 | `0xBF845600` | `0x045600` |
| OC13 | `0xBF845800` | `0x045800` |
| OC14 | `0xBF845A00` | `0x045A00` |
| OC15 | `0xBF845C00` | `0x045C00` |
| OC16 | `0xBF845E00` | `0x045E00` |

Each block is `0x200` bytes. Stride between instances is `0x200`.

### Registers within each block

Each register follows the PIC32MK SET/CLR/INV convention:
`+0x0` = base, `+0x4` = CLR, `+0x8` = SET, `+0xC` = INV.

| Offset | Name   | Description |
|--------|--------|-------------|
| `0x00` | OCxCON | Control register |
| `0x10` | OCxR   | Primary compare value |
| `0x20` | OCxRS  | Secondary compare value |

### OCxCON bit fields (Register 19-1, p. 322)

| Bit  | Name    | Description |
|------|---------|-------------|
| 15   | ON      | Module enable (1 = enabled) |
| 13   | SIDL    | Stop in idle mode |
| 5    | OC32    | 32-bit compare mode (uses T2+T3 or T4+T5 pair) |
| 4    | OCFLT   | PWM fault condition (read-only) |
| 3    | OCTSEL  | Timer source: 0 = Timer2/4/6/8, 1 = Timer3/5/7/9 |
| 2:0  | OCM     | Operating mode (see table below) |

### OCM mode codes

| OCM | Mode |
|-----|------|
| 000 | Disabled |
| 001 | High on compare match, then low (force high) |
| 010 | Init low, force high on match |
| 011 | Toggle on match |
| 100 | Single pulse |
| 101 | Continuous pulses |
| 110 | PWM, fault pin disabled |
| 111 | PWM, fault pin enabled |

### Timer source selection

The timer that drives an OC instance is chosen by `OCTSEL`:

| Instance group | OCTSEL=0 | OCTSEL=1 |
|----------------|----------|----------|
| OC1–OC9        | Timer2   | Timer3   |
| OC10–OC16      | Timer4/6/8 | Timer5/7/9 (see datasheet §19) |

For 32-bit mode (`OC32=1`), the module uses a 32-bit timer pair:
- OCTSEL=0 → T2+T3
- OCTSEL=1 → T4+T5

---

## Compare Value Roles by Mode

The meaning of `OCxR` and `OCxRS` differs between modes:

| Mode | OCxR role | OCxRS role |
|------|-----------|------------|
| PWM (ocm=6,7) | Loaded from OCxRS at period boundary (don't-care at init) | High-time compare: output goes low when timer == OCxRS |
| Single pulse (ocm=4) | Timer count at which output **asserts** | Timer count at which output **de-asserts** |
| Continuous pulses (ocm=5) | Timer count at which output asserts each cycle | Timer count at which output de-asserts each cycle |
| Toggle (ocm=3) | Compare value; output toggles on each match | Not used |

---

## QEMU Implementation

### Source files

| File | Purpose |
|------|---------|
| `hw/mips/pic32mk_oc.c` | Device model: register I/O, event emission, chardev streaming |
| `include/hw/mips/pic32mk.h` | OC base offsets, register offsets, bit masks, IRQ numbers |
| `hw/mips/pic32mk.c` | Instantiates OC1–OC16, wires MMIO + EVIC, attaches chardev |
| `hw/mips/meson.build` | Adds `pic32mk_oc.c` to the build |

### Device model behaviour

- All three registers (`OCxCON`, `OCxR`, `OCxRS`) are stored in device state and
  respond to SET/CLR/INV sub-register writes via the `apply_sci()` helper.
- Reads of any register return the stored value.
- Unknown offsets log `LOG_UNIMP` via `qemu_log_mask`.
- On OCxCON.ON going **0→1**: `oc_emit_event(s, true)` is called.
- On OCxCON.ON going **1→0**: `oc_emit_event(s, false)` is called.
- No timer ticks, no pin state, no IRQ is generated — purely diagnostic.

### Interrupt vectors

| Instance | IRQ vector |
|----------|-----------|
| OC1  | 7   |
| OC2  | 12  |
| OC3  | 17  |
| OC4  | 22  |
| OC5  | 27  |
| OC6  | 79  |
| OC7  | 83  |
| OC8  | 87  |
| OC9  | 91  |
| OC10 | 199 |
| OC11 | 202 |
| OC12 | 205 |
| OC13 | 208 |
| OC14 | 211 |
| OC15 | 214 |
| OC16 | 217 |

Each OC device has one IRQ output wired to the EVIC, but it is never pulsed in
the current diagnostic implementation (no timing engine).

---

## Event Streaming (chardev)

When enabled, the device streams a 16-byte binary event to a QEMU chardev
named `"oc-events"` on every ON/OFF transition of OCxCON.ON.

### Activating

Pass `-chardev` on the QEMU command line before launching:

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios firmware.bin \
    -serial stdio -nographic -monitor none \
    -chardev file,id=oc-events,path=/tmp/oc_events.bin
```

Any QEMU chardev backend works: `file`, `socket`, `pipe`, `stdio`, etc.
If `"oc-events"` is not found at startup, the device silently falls back to
`qemu_log()` only — no change in behaviour.

### Binary message format

Each event is exactly **16 bytes**, little-endian:

| Offset | Size | Type    | Field     | Description |
|--------|------|---------|-----------|-------------|
| 0      | 1    | uint8   | `index`   | OC instance number (1–16) |
| 1      | 1    | uint8   | `enabled` | 1 = enabled, 0 = disabled |
| 2      | 1    | uint8   | `ocm`     | OCM mode bits [2:0] |
| 3      | 1    | uint8   | `flags`   | bit0 = OC32, bit1 = OCTSEL |
| 4–7    | 4    | uint32  | `ocr`     | OCxR primary compare value |
| 8–11   | 4    | uint32  | `ocrs`    | OCxRS secondary compare value |
| 12–15  | 4    | uint32  | `pr`      | Timer PRx period register (0 on disable event) |

The `pr` field is read directly from the associated timer's period register via
`address_space_ldl_le()` at the moment OCxCON.ON goes high.

### Pulse-width / duty-cycle formulas

The correct formula depends on the OCM mode:

```
PWM modes (ocm = 6 or 7):
    duty_pct = ocrs / pr × 100

Single pulse / Continuous pulses (ocm = 4 or 5):
    pulse_pct = (ocrs − ocr) / pr × 100

Toggle (ocm = 3):
    toggle_freq = timer_freq / (2 × ocr)   [not captured in event]
```

### Python consumer example

```python
import struct

OCM_NAMES = [
    'Disabled', 'High on match', 'Init low force high', 'Toggle on match',
    'Single pulse', 'Continuous pulses', 'PWM (no fault)', 'PWM (fault)',
]

with open('/tmp/oc_events.bin', 'rb') as f:
    while chunk := f.read(16):
        idx, en, ocm, flags, ocr, ocrs, pr = struct.unpack_from('<BBBBIIi', chunk)
        oc32   = flags & 1
        octsel = (flags >> 1) & 1
        state  = 'ON ' if en else 'OFF'

        if en and pr > 0:
            if ocm in (6, 7):
                pct = ocrs / pr * 100
                label = f'duty={pct:.1f}%'
            elif ocm in (4, 5):
                pct = (ocrs - ocr) / pr * 100
                label = f'pulse={pct:.1f}%'
            else:
                label = ''
        else:
            label = ''

        print(f'OC{idx:2d} {state}  [{OCM_NAMES[ocm]:<22}]  '
              f'OCxR=0x{ocr:08X}  OCxRS=0x{ocrs:08X}  PRx=0x{pr:08X}  {label}')
```

---

## ⚠️ WARNING — TODO: Chardev Event Output Calculations Are UNVERIFIED

> **This section documents known limitations that must be resolved before the
> chardev event output can be trusted for any real measurement or GUI display.**

### Issue 1 — PRx is only read at the moment OCxCON.ON is set

The `pr` field in the event message is sampled by reading the associated
timer's period register **at the instant the firmware writes OCxCON.ON=1**.
This means:

- If the firmware writes `PRx` *after* enabling the OC module, the event will
  carry the old (possibly default `0xFFFF`) period value.
- If the timer is reconfigured while the OC is running, no new event is emitted
  and the cached `pr` value becomes stale.
- The demo firmware does **not** explicitly configure Timer2/3 — the `pr` value
  seen in events (`0xFFFF`) is the timer reset default, not a real application
  period. The duty/pulse percentages computed from this are numerically valid but
  **not representative of a real hardware application**.

### Issue 2 — Timer source mapping is hardcoded for OC1–OC9 only

The `oc_timer_pr_addr[]` lookup table maps:
- `OCTSEL=0` → Timer2 PRx (physical `0x1F820220`)
- `OCTSEL=1` → Timer3 PRx (physical `0x1F820420`)

This is only correct for **OC1–OC9**. For OC10–OC16, the datasheet specifies
different timer pairings (Timer4–Timer9 in the peripheral-2 block). Using the
current table for OC10–OC16 will read from the wrong PRx register, producing
a silently incorrect `pr` value.

### Issue 3 — OC32=1 (32-bit mode) is not handled

When `OC32=1` the compare values are 32-bit and the timer source is a paired
32-bit timer (T2+T3 or T4+T5). The current implementation reads only a single
16-bit PRx register even in this mode. The resulting `pr` will be the low 16
bits of the period only.

### Issue 4 — Calculations have NOT been cross-checked against real hardware

The formulas and timer address mappings are derived from DS60001519E and the
MCU header file (`p32mk1024mcm100.h`) but have never been validated by running
equivalent firmware on physical PIC32MK hardware and comparing the captured
`pr` / `ocrs` / `ocr` values against an oscilloscope measurement.

### What needs to be done before this output can be trusted

- [ ] Validate PRx read timing: firmware should write `PRx` **before** enabling
  the OC module, or the emulator should defer event emission until the end of the
  current MMIO transaction.
- [ ] Extend `oc_timer_pr_addr` to a full table indexed by `(oc_index, octsel)`
  covering all 16 instances correctly, including OC10–OC16 peripheral-2 timers.
- [ ] Handle `OC32=1` by reading the 32-bit paired timer period (T3 PRx for the
  high half when OC32=1 and OCTSEL=0).
- [ ] Run the demo firmware on real PIC32MK hardware (or a known-correct
  simulation) and compare oscilloscope duty cycle against the event stream output.
- [ ] Until validated, treat duty/pulse percentages in the event stream as
  **approximate indicators only**, not accurate measurements.

---

## Test Firmware

The demo task `vOcDemoTask` in
`tests/pic32mk_rtos_demo/firmware/src/main.c` exercises three instances:

| Instance | Mode | OCxR | OCxRS | Expected pulse% (with default PR=0xFFFF) |
|----------|------|------|-------|------------------------------------------|
| OC1 | PWM (ocm=6)             | 0x0000 | 0x7FFF | duty = 50.0% |
| OC2 | Single pulse (ocm=4)    | 0x0100 | 0x0500 | pulse = 1.6% |
| OC3 | Continuous pulses (ocm=5)| 0x0200 | 0x0800 | pulse = 2.3% |

The task uses the Harmony OCMP plib API (`OCMP1_Initialize`, `OCMP1_Enable`,
`OCMP1_CompareSecondaryValueSet`, etc.) from
`tests/pic32mk_rtos_demo/firmware/src/config/default/peripheral/ocmp/`.

### Build and run

```bash
# Build QEMU (from repo root)
make -C build -j$(nproc)

# Build test firmware
make -C tests/pic32mk_rtos_demo

# Run — chardev to file
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -chardev file,id=oc-events,path=/tmp/oc_events.bin

# Run — qemu_log only (no chardev)
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors
```

### Expected UART output

```
[OC] Output Compare demo starting...
[OC] OC1: PWM mode (plib), duty=50%
[OC] OC2: Single pulse mode, R=0x100 RS=0x500
[OC] OC3: Continuous pulse mode
[OC] All OC modules disabled
[OC] Output Compare demo complete.
```

### Expected qemu_log output (stderr with `-d unimp`)

```
pic32mk_oc: OC1 enabled — mode=PWM (no fault), OC32=0, OCTSEL=0, OCxR=0x00000000, OCxRS=0x00007FFF
pic32mk_oc: OC2 enabled — mode=Single pulse, OC32=0, OCTSEL=0, OCxR=0x00000100, OCxRS=0x00000500
pic32mk_oc: OC3 enabled — mode=Continuous pulses, OC32=0, OCTSEL=0, OCxR=0x00000200, OCxRS=0x00000800
pic32mk_oc: OC1 disabled
pic32mk_oc: OC2 disabled
pic32mk_oc: OC3 disabled
```

### Expected chardev binary parse

```
OC 1 ON  [PWM (no fault)        ]  OCxR=0x00000000  OCxRS=0x00007FFF  PRx=0x0000FFFF  duty=50.0%
OC 2 ON  [Single pulse          ]  OCxR=0x00000100  OCxRS=0x00000500  PRx=0x0000FFFF  pulse=1.6%
OC 3 ON  [Continuous pulses     ]  OCxR=0x00000200  OCxRS=0x00000800  PRx=0x0000FFFF  pulse=2.3%
OC 1 OFF ...
OC 2 OFF ...
OC 3 OFF ...
```
