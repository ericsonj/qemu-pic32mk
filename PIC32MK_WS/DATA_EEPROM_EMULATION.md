# PIC32MK Data EEPROM Emulation

> **Datasheet:** DS60001519E, §11 — Data EEPROM  
> **Device:** PIC32MK1024MCM100  
> **Phase:** 2A (functional)

---

## Overview

The PIC32MK includes 4 KB of on-chip Data EEPROM (1024 × 32-bit words) organized
in 32 pages of 128 bytes each. This module emulates the EEPROM controller through
its SFR control registers: **EECON**, **EEKEY**, **EEADDR**, and **EEDATA**.

An optional host-file backing mode persists EEPROM contents across QEMU sessions.

---

## Source File Layout

| File | Purpose |
|------|---------|
| `hw/mips/pic32mk_dataee.c` | QEMU device model — register I/O, command FSM, backing file |
| `include/hw/mips/pic32mk.h` | Constants: offsets, bit masks, geometry, IRQ vector |
| `hw/mips/pic32mk.c` | Board integration: device instantiation + EVIC wiring |
| `hw/mips/meson.build` | Build system entry (`'pic32mk_dataee.c'`) |
| `tests/.../peripheral/eeprom/plib_eeprom.c` | Harmony EEPROM plib (firmware driver) |
| `tests/.../peripheral/eeprom/plib_eeprom.h` | Harmony EEPROM plib header |
| `tests/.../config/default/xc.h` | GCC xc.h shim — EEPROM SFR definitions |
| `tests/.../firmware/src/main.c` | FreeRTOS `vEepromTestTask` test task |

---

## Memory Map

| Register | Physical Address | Virtual (KSEG1) | Offset | Type |
|----------|-----------------|-----------------|--------|------|
| EECON    | 0x1F829000 | 0xBF829000 | 0x00 | R/W |
| EECONCLR | 0x1F829004 | 0xBF829004 | 0x04 | W (clear bits) |
| EECONSET | 0x1F829008 | 0xBF829008 | 0x08 | W (set bits) |
| EECONINV | 0x1F82900C | 0xBF82900C | 0x0C | W (invert bits) |
| EEKEY    | 0x1F829010 | 0xBF829010 | 0x10 | W-only |
| EEADDR   | 0x1F829020 | 0xBF829020 | 0x20 | R/W |
| EEADDRCLR| 0x1F829024 | 0xBF829024 | 0x24 | W (clear bits) |
| EEADDRSET| 0x1F829028 | 0xBF829028 | 0x28 | W (set bits) |
| EEDATA   | 0x1F829030 | 0xBF829030 | 0x30 | R/W |
| EEDATACLR| 0x1F829034 | 0xBF829034 | 0x34 | W (clear bits) |
| EEDATASET| 0x1F829038 | 0xBF829038 | 0x38 | W (set bits) |

**MMIO Region Size:** 0x40 bytes (64 bytes)

### SET/CLR/INV Convention

Every register bank uses the standard PIC32MK atomic modify pattern:

| Sub-offset | Operation | Example |
|-----------|-----------|---------|
| +0x00 | Direct write (`reg = val`) | `EECON = 0x8100` |
| +0x04 | Clear bits (`reg &= ~val`) | `EECONCLR = 0x40` clears WREN |
| +0x08 | Set bits (`reg |= val`) | `EECONSET = 0x80` sets RW |
| +0x0C | Invert bits (`reg ^= val`) | `EECONINV = 0x80` toggles RW |

**Exception:** EEKEY is write-only with no SET/CLR/INV aliases. All writes go
through a direct-write path regardless of sub-offset.

---

## EECON Register (Offset 0x00)

```
Bit 15   14   13   12   [11:8]  7    6    [5:4]  3    [2:0]
    ON   RDY  SIDL ABORT  —     RW   WREN  ERR   ILW   CMD
```

| Bits | Name | R/W | Reset | Description |
|------|------|-----|-------|-------------|
| 2:0 | CMD | R/W | 0 | Command selector (see Commands table) |
| 3 | ILW | R | 0 | Internal Long Write (hardware status) |
| 5:4 | ERR | R | 0 | Error status code |
| 6 | WREN | R/W | 0 | Write Enable — must be set before write/erase commands |
| 7 | RW | R/W | 0 | Read/Write start — 0→1 transition triggers command |
| 11:8 | — | — | — | Reserved |
| 12 | ABORT | R/W | 0 | Abort in-progress operation |
| 13 | SIDL | R/W | 0 | Stop in Idle mode |
| 14 | RDY | R | 0 | Module ready (set automatically when ON=1) |
| 15 | ON | R/W | 0 | Module enable |

### Bit Masks (from `pic32mk.h`)

```c
#define PIC32MK_EECON_CMD_MASK  0x00000007u   /* bits [2:0] */
#define PIC32MK_EECON_ERR_MASK  0x00000030u   /* bits [5:4] */
#define PIC32MK_EECON_WREN      (1u << 6)     /* bit 6      */
#define PIC32MK_EECON_RW        (1u << 7)     /* bit 7      */
#define PIC32MK_EECON_ABORT     (1u << 12)    /* bit 12     */
#define PIC32MK_EECON_SIDL      (1u << 13)    /* bit 13     */
#define PIC32MK_EECON_RDY       (1u << 14)    /* bit 14     */
#define PIC32MK_EECON_ON        (1u << 15)    /* bit 15     */
```

### Error Codes (ERR field, bits 5:4)

| Code | Bits | Name | Condition |
|------|------|------|-----------|
| 0 | 00 | NONE | No error |
| 1 | 01 | VERIFY | Erase/program verify failure |
| 2 | 10 | INVALID | Bad address, missing WREN, or unlock not done |
| 3 | 11 | BOR | Brownout reset occurred |

Errors are cleared automatically at the start of each new command.

---

## Commands (CMD Field)

| CMD | Value | Name | WREN | Unlock | Description |
|-----|-------|------|------|--------|-------------|
| 0 | `000` | WORD_READ | No | No | Read 32-bit word at EEADDR → EEDATA |
| 1 | `001` | WORD_WRITE | Yes | Yes | Write EEDATA → word at EEADDR |
| 2 | `010` | PAGE_ERASE | Yes | Yes | Erase 128-byte page containing EEADDR |
| 3 | `011` | BULK_ERASE | Yes | Yes | Erase entire 4 KB EEPROM |
| 4 | `100` | CONFIG_WRITE | Yes | Yes | Write EEDATA → DEVEE shadow config |
| 5–7 | — | Reserved | — | — | Sets ERR = INVALID |

### Command Execution Flow

```
1. Firmware sets EEADDR, EEDATA, CMD, WREN as needed
2. Firmware writes EEKEY sequence: 0xEDB7 then 0x1248
3. Firmware sets RW bit (EECONSET = 0x80)
4. QEMU detects RW 0→1 transition
5. dataee_execute_cmd() runs:
   a. Clear previous error
   b. Execute command based on CMD field
   c. Flush to backing file (if write/erase)
   d. Clear RW bit
   e. Reset EEKEY FSM to LOCKED
   f. Pulse IRQ 186
6. Firmware polls RW == 0 (or waits for IRQ)
```

### Command Details

#### WORD_READ (CMD=0)
- **Input:** `EEADDR[13:2]` = word index (0..1023)
- **Output:** `EEDATA` = word value
- **Out-of-range:** EEDATA = `0xFFFFFFFF`, ERR = INVALID
- **No WREN or unlock required**

#### WORD_WRITE (CMD=1)
- **Input:** `EEADDR[13:2]` = word index, `EEDATA` = value to write
- **Pre-conditions:** WREN=1, EEKEY unlocked
- **Effect:** `data[word_idx] = EEDATA`, marks dirty, flushes file
- **Failure:** ERR = INVALID if WREN=0, locked, or out-of-range

#### PAGE_ERASE (CMD=2)
- **Input:** `EEADDR` (any address within the target page)
- **Pre-conditions:** WREN=1, EEKEY unlocked
- **Effect:** 32 words starting at page boundary → `0xFFFFFFFF`
- **Page size:** 32 words = 128 bytes
- **Alignment:** `page_start = (word_idx / 32) * 32`

#### BULK_ERASE (CMD=3)
- **Pre-conditions:** WREN=1, EEKEY unlocked
- **Effect:** All 1024 words → `0xFFFFFFFF`
- **EEADDR ignored**

#### CONFIG_WRITE (CMD=4)
- **Input:** `EEADDR` = 0x00/0x04/0x08/0x0C (DEVEE0–3 shadow)
- **Pre-conditions:** WREN=1, EEKEY unlocked
- **Effect:** Writes EEDATA to internal config shadow register
- **Does NOT persist to backing file**

---

## EEKEY Unlock FSM

The EEKEY register implements a two-step unlock sequence to prevent
accidental writes. This is a write-only register that always reads as 0.

### FSM State Diagram

```
                ┌─────────────┐
                │   LOCKED    │◄──── Reset / After command
                │  (state 0)  │◄──── Wrong key at any state
                └──────┬──────┘
                       │ Write 0xEDB7
                       ▼
                ┌─────────────┐
                │  KEY1_OK    │
                │  (state 1)  │
                └──────┬──────┘
                       │ Write 0x1248
                       ▼
                ┌─────────────┐
                │  UNLOCKED   │───── Valid for ONE command
                │  (state 2)  │      then auto-locks
                └─────────────┘
```

### Unlock Keys

| Step | Value | Constant |
|------|-------|----------|
| 1st  | `0xEDB7` | `PIC32MK_EEKEY1` |
| 2nd  | `0x1248` | `PIC32MK_EEKEY2` |

### Rules
- Writing any value other than the expected key resets to LOCKED
- After command execution (RW cleared), FSM resets to LOCKED
- Unlock is consumed by a single command — must re-unlock for each operation
- The unlock + RW set must be done with interrupts disabled (atomicity)

---

## Storage Geometry

| Parameter | Value |
|-----------|-------|
| Total size | 4 KB (4096 bytes) |
| Word width | 32 bits |
| Total words | 1024 |
| Page size | 128 bytes (32 words) |
| Total pages | 32 |
| Address range | 0x000 – 0xFFC (byte addresses, word-aligned) |
| Address mask | `0x3FFC` (14-bit, word-aligned) |
| Erased value | `0xFFFFFFFF` |

---

## Host-File Backing

### Purpose

By default, EEPROM data lives in RAM and is lost when QEMU exits. The
optional host-file backing mode persists EEPROM contents to a binary file
on the host filesystem, surviving across QEMU sessions.

### Enabling

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -global pic32mk-dataee.filename=eeprom.bin
```

The `-global pic32mk-dataee.filename=<path>` qdev property sets the backing
file path. Without this flag, the device operates in RAM-only mode.

### File Format

- **Size:** Exactly 4096 bytes
- **Byte order:** Little-endian (native MIPS LE)
- **Content:** Raw 32-bit word array: `data[0]` at offset 0, `data[1]` at offset 4, etc.
- **Erased state:** All bytes `0xFF`

### Auto-Initialization

When the backing file doesn't exist or is smaller than 4096 bytes, the device
automatically creates and initializes it with `0xFF` (erased EEPROM state).
**You do not need to pre-create the file.**

### Lifecycle

```
┌──────────────────────────────────────────────────────────────┐
│                    Device Realize                             │
│  1. Open/create file (O_RDWR | O_CREAT, 0644)               │
│  2. If file < 4096 bytes → write 4096 × 0xFF + fdatasync    │
└──────────────────────────────┬───────────────────────────────┘
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                    Device Reset                               │
│  1. Initialize RAM to all 0xFF                                │
│  2. Load backing file into RAM (overwrite erased state)       │
└──────────────────────────────┬───────────────────────────────┘
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                  Runtime (per write/erase)                     │
│  1. Modify s->data[] in RAM                                   │
│  2. Mark dirty                                                │
│  3. Seek to file start                                        │
│  4. Write all 4096 bytes                                      │
│  5. fdatasync() — ensure durability on disk                   │
└──────────────────────────────┬───────────────────────────────┘
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                   Device Unrealize (QEMU exit)                │
│  1. Final flush if dirty                                      │
│  2. Close file descriptor                                     │
└──────────────────────────────────────────────────────────────┘
```

### Inspecting the Backing File

```bash
# Dump as 32-bit little-endian words
xxd -e -g4 eeprom.bin | head -20

# Show only non-erased words
xxd -e -g4 eeprom.bin | grep -v 'ffffffff ffffffff ffffffff ffffffff'

# Check file size (should be 4096)
wc -c eeprom.bin
```

---

## Board Integration

The DATAEE device is instantiated in `pic32mk.c` during board init:

```c
/* Data EEPROM at 0xBF829000 — 4 KB, optionally backed by host file */
{
    DeviceState *ee = qdev_new(TYPE_PIC32MK_DATAEE);  /* "pic32mk-dataee" */
    sysbus_realize_and_unref(SYS_BUS_DEVICE(ee), &error_fatal);
    MemoryRegion *mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(ee), 0);
    memory_region_add_subregion_overlap(&s->sfr, PIC32MK_DATAEE_OFFSET, mr, 1);
    sysbus_connect_irq(SYS_BUS_DEVICE(ee), 0,
                       qdev_get_gpio_in(s->evic, PIC32MK_IRQ_DATAEE));
}
```

| Property | Value |
|----------|-------|
| QEMU type name | `pic32mk-dataee` |
| Parent type | `TYPE_SYS_BUS_DEVICE` |
| SFR offset | `0x029000` (maps to 0xBF829000) |
| MMIO size | 0x40 bytes |
| IRQ output | EVIC vector 186 (`_DATA_EE_VECTOR`) |
| qdev property | `filename` (string, optional) |

---

## Firmware-Side Integration

### xc.h SFR Definitions

The GCC compatibility shim (`tests/.../config/default/xc.h`) provides all
register definitions the Harmony plib needs:

```c
/* Data EEPROM base */
#define _DATAEE_BASE    0xBF829000u

/* EECON register + SET/CLR/INV aliases */
#define EECON           (*(volatile uint32_t *)(_DATAEE_BASE + 0x00u))
#define EECONCLR        (*(volatile uint32_t *)(_DATAEE_BASE + 0x04u))
#define EECONSET        (*(volatile uint32_t *)(_DATAEE_BASE + 0x08u))
#define EECONINV        (*(volatile uint32_t *)(_DATAEE_BASE + 0x0Cu))

/* EECONbits bitfield union */
typedef union {
    struct {
        uint32_t CMD  : 3;   uint32_t ILW  : 1;
        uint32_t ERR  : 2;   uint32_t WREN : 1;
        uint32_t RW   : 1;   uint32_t      : 4;
        uint32_t ABORT: 1;   uint32_t SIDL : 1;
        uint32_t RDY  : 1;   uint32_t ON   : 1;
        uint32_t      : 16;
    };
    uint32_t w;
} __EECON_t;
#define EECONbits  (*(volatile __EECON_t *)(_DATAEE_BASE + 0x00u))

/* Bit masks used by plib */
#define _EECON_CMD_MASK   0x00000007u
#define _EECON_ERR_MASK   0x00000030u
#define _EECON_WREN_MASK  0x00000040u
#define _EECON_RW_MASK    0x00000080u
#define _EECON_ON_MASK    0x00008000u
#define _EECON_RDY_MASK   0x00004000u

/* EEKEY — write-only unlock register */
#define EEKEY    (*(volatile uint32_t *)(_DATAEE_BASE + 0x10u))

/* EEADDR + aliases, EEDATA + aliases */
#define EEADDR   (*(volatile uint32_t *)(_DATAEE_BASE + 0x20u))
#define EEDATA   (*(volatile uint32_t *)(_DATAEE_BASE + 0x30u))
/* ... CLR/SET variants also defined ... */

/* DEVEE0–3 config words (Boot Flash addresses) */
#define DEVEE0   (*(volatile uint32_t *)0xBFC45030u)
#define DEVEE1   (*(volatile uint32_t *)0xBFC45034u)
#define DEVEE2   (*(volatile uint32_t *)0xBFC45038u)
#define DEVEE3   (*(volatile uint32_t *)0xBFC4503Cu)

/* CFGCON2 — needed for EEWS (EEPROM Wait States) */
#define CFGCON2     (*(volatile uint32_t *)(_CFG_BASE + 0x110u))
#define CFGCON2bits (*(volatile __CFGCON2_t *)(_CFG_BASE + 0x110u))
```

A `__builtin_mtc0()` shim was also added to xc.h (used by `EEPROM_WriteExecute`
to restore CP0.Status after disabling interrupts for the unlock sequence).

### Harmony plib API

The Harmony-generated EEPROM plib is located at:
`tests/pic32mk_rtos_demo/firmware/src/config/default/peripheral/eeprom/`

| Function | Description |
|----------|-------------|
| `EEPROM_Initialize()` | Enable module, wait for RDY, write DEVEE0–3 config |
| `EEPROM_WordRead(addr, *data)` | Read 32-bit word → `*data` |
| `EEPROM_WordWrite(addr, data)` | Write 32-bit word (unlock + RW) |
| `EEPROM_PageErase(addr)` | Erase 128-byte page (unlock + RW) |
| `EEPROM_BulkErase()` | Erase all 4 KB (unlock + RW) |
| `EEPROM_IsBusy()` | Returns `true` if RW bit is set |
| `EEPROM_ErrorGet()` | Returns `EECON & _EECON_ERR_MASK` |

### Initialization Sequence (plib_eeprom.c)

```
CFGCON2bits.EEWS = 0        ← Set EEPROM wait states
EECONbits.ON = 1            ← Power on the EEPROM module
wait(EECONbits.RDY == 1)    ← Wait for ready (~125 µs on real HW, instant in QEMU)
EECONSET = _EECON_WREN_MASK ← Enable writing
EECONbits.CMD = CONFIG_WRITE
for each DEVEEx:
    EEADDR = offset          ← 0x00, 0x04, 0x08, 0x0C
    EEDATA = DEVEEx           ← Read from Boot Flash
    EEKEY = 0xEDB7            ← Unlock step 1
    EEKEY = 0x1248            ← Unlock step 2
    EECONSET = _EECON_RW_MASK ← Execute
    wait(RW == 0)
EECONCLR = _EECON_WREN_MASK  ← Disable writes
```

---

## Test Task: vEepromTestTask

Located in `tests/pic32mk_rtos_demo/firmware/src/main.c`.

### Registration

```c
/* In main(), after EEPROM_Initialize(): */
xTaskCreate(vEepromTestTask, "EE", configMINIMAL_STACK_SIZE * 2,
            NULL, tskIDLE_PRIORITY + 1, NULL);
```

### Test Sequence

| Step | Operation | Addresses | Expected |
|------|-----------|-----------|----------|
| 1 | Write 4 words | 0x000, 0x004, 0x008, 0x00C | 0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xA5A5A5A5 |
| 2 | Read back & verify | Same 4 addresses | Must match written values |
| 3 | Check error status | EECON.ERR | Must be 0 (NONE) |
| 4* | Page erase (page 0) | 0x000–0x07C | All → 0xFFFFFFFF |
| 5* | Bulk erase | Entire 4 KB | 0x100 → 0xFFFFFFFF |

*Steps 4–5 are gated by `#if 0` — enable to test erase operations.

### Expected UART Output

```
[EE] EEPROM test starting...
[EE] Write/Read: 4/4 PASS
[EE] EEPROM test complete.
```

With erase tests enabled (`#if 1`):
```
[EE] EEPROM test starting...
[EE] Write/Read: 4/4 PASS
[EE] PageErase: 4/4 PASS
[EE] BulkErase: PASS
[EE] EEPROM test complete.
```

---

## Running the Test

### Build

```bash
# Build QEMU (from repo root)
make -C build -j$(nproc)

# Build firmware
cd tests/pic32mk_rtos_demo && make
```

### Run (RAM-only mode)

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors
```

### Run (with host-file persistence)

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors \
    -global pic32mk-dataee.filename=eeprom.bin
```

QEMU auto-creates `eeprom.bin` (4096 bytes, all `0xFF`) on first run.
Subsequent runs load previous EEPROM state from the file.

### Verify Persistence

```bash
# After QEMU exits, inspect the backing file:
xxd -e -g4 eeprom.bin | head -10

# Show non-erased words:
xxd -e -g4 eeprom.bin | grep -v 'ffffffff ffffffff ffffffff ffffffff'
```

---

## QEMU Device Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `filename` | string | `""` (empty = RAM-only) | Path to host backing file |

Set via: `-global pic32mk-dataee.filename=<path>`

---

## Implementation Notes

### Timing

All EEPROM operations complete instantaneously in the emulator. On real
hardware, WORD_WRITE takes ~20 ms and PAGE_ERASE takes ~20 ms. The RDY bit
after ON is set immediately (vs. ~125 µs on hardware). Firmware that polls
RW or RDY will work correctly — the bits are already in the expected state
when the firmware reads them.

### Interrupt

An IRQ pulse is sent on EVIC vector 186 after every command completes.
The firmware test uses polling mode (not interrupt-driven), but
interrupt-driven workflows are supported.

### Endianness

Data is stored in native little-endian format (matching the PIC32MK CPU
configuration: `CONFIG.BE = 0`). The backing file uses the same byte order.

### DEVEE Config Words

The Boot Flash addresses `0xBFC45030`–`0xBFC4503C` (DEVEE0–3) are mapped as
part of the boot flash memory region. In the current implementation they
read as `0xFFFFFFFF` (blank). The CONFIG_WRITE command stores to an internal
shadow array, not to the backing file.

### Error Handling

- Address validation: word index must be < 1024 for data operations
- WREN check: write/erase commands require WREN=1
- Unlock check: write/erase commands require EEKEY FSM = UNLOCKED
- All violations set ERR = INVALID (code 2)
- Errors are cleared at the start of each new command

### fsync Durability

Every write/erase that modifies EEPROM data calls `fdatasync()` after the
`write()` system call, ensuring data reaches stable storage even if QEMU
is killed (e.g., `timeout`, Ctrl+C, or crash).
