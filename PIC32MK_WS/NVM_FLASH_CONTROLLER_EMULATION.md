# PIC32MK NVM / Flash Controller Emulation

> **Datasheet:** DS60001519E, §10 — Flash Program Memory
> **Device:** PIC32MK1024MCM100
> **Phase:** 2A (functional)

---

## Overview

The PIC32MK includes a Flash Controller (NVM) that mediates program/erase
operations on the 1 MB Program Flash (0x1D000000–0x1D0FFFFF). This module
emulates the NVM controller through its SFR control registers: **NVMCON**,
**NVMKEY**, **NVMADDR**, **NVMDATA0–3**, **NVMSRCADDR**, **NVMPWP**,
**NVMBWP**, and **NVMCON2**.

The program flash is RAM-backed (not ROM) so the NVM controller can write
into it at runtime. A QOM link property connects the NVM device to the
MemoryRegion backing the program flash.

Operations complete synchronously in QEMU (the WR bit clears immediately),
which means `NVM_IsBusy()` returns false right after each call.

---

## Source File Layout

| File | Purpose |
|------|---------|
| `hw/mips/pic32mk_nvm.c` | QEMU device model — register I/O, command FSM, flash writes |
| `include/hw/mips/pic32mk.h` | Constants: offsets, bit masks, geometry, IRQ vector |
| `hw/mips/pic32mk.c` | Board integration: device instantiation, pflash RAM region, EVIC wiring |
| `hw/mips/meson.build` | Build system entry (`'pic32mk_nvm.c'`) |
| `tests/.../peripheral/nvm/plib_nvm.c` | Harmony NVM plib (firmware driver, unmodified) |
| `tests/.../peripheral/nvm/plib_nvm.h` | Harmony NVM plib header |
| `tests/.../config/default/xc.h` | GCC xc.h shim — NVM SFR definitions |
| `tests/.../config/default/crt0.S` | ISR dispatch — NVM interrupt (IFS0 bit 31) |
| `tests/.../config/default/port_asm_patched.S` | FreeRTOS-safe NVM ISR wrapper |
| `tests/.../config/default/interrupts.h` | `NVM_InterruptHandler` prototype |
| `tests/.../firmware/src/main.c` | FreeRTOS `vNvmTestTask` test task |

---

## Memory Map

### Program Flash

| Region | Physical Address | Virtual (KSEG0) | Virtual (KSEG1) | Size |
|--------|-----------------|-----------------|-----------------|------|
| Program Flash | 0x1D000000 | 0x9D000000 | 0xBD000000 | 1 MB |

The program flash is created as `memory_region_init_ram()` (not ROM) in
`pic32mk.c` so the NVM controller can write into it.

### NVM SFR Registers

| Register    | Physical Address | Virtual (KSEG1) | Offset | Type |
|-------------|-----------------|-----------------|--------|------|
| NVMCON      | 0x1F800A00 | 0xBF800A00 | 0x00 | R/W |
| NVMCONCLR   | 0x1F800A04 | 0xBF800A04 | 0x04 | W (clear bits) |
| NVMCONSET   | 0x1F800A08 | 0xBF800A08 | 0x08 | W (set bits) |
| NVMCONINV   | 0x1F800A0C | 0xBF800A0C | 0x0C | W (invert bits) |
| NVMKEY      | 0x1F800A10 | 0xBF800A10 | 0x10 | W-only |
| NVMADDR     | 0x1F800A20 | 0xBF800A20 | 0x20 | R/W |
| NVMDATA0    | 0x1F800A30 | 0xBF800A30 | 0x30 | R/W |
| NVMDATA1    | 0x1F800A40 | 0xBF800A40 | 0x40 | R/W |
| NVMDATA2    | 0x1F800A50 | 0xBF800A50 | 0x50 | R/W |
| NVMDATA3    | 0x1F800A60 | 0xBF800A60 | 0x60 | R/W |
| NVMSRCADDR  | 0x1F800A70 | 0xBF800A70 | 0x70 | R/W |
| NVMPWP      | 0x1F800A80 | 0xBF800A80 | 0x80 | R/W |
| NVMBWP      | 0x1F800A90 | 0xBF800A90 | 0x90 | R/W |
| NVMCON2     | 0x1F800AA0 | 0xBF800AA0 | 0xA0 | R/W |

**MMIO Region Size:** 0xB0 bytes (176 bytes)

### SET/CLR/INV Convention

Every register bank (except NVMKEY) uses the standard PIC32MK atomic modify
pattern:

| Sub-offset | Operation | Example |
|-----------|-----------|---------|
| +0x00 | Direct write (`reg = val`) | `NVMCON = 0x4001` |
| +0x04 | Clear bits (`reg &= ~val`) | `NVMCONCLR = 0x4000` clears WREN |
| +0x08 | Set bits (`reg |= val`) | `NVMCONSET = 0x8000` sets WR |
| +0x0C | Invert bits (`reg ^= val`) | `NVMCONINV = 0x4000` toggles WREN |

**Exception:** NVMKEY is write-only with no SET/CLR/INV aliases. All writes
go through the unlock FSM regardless of sub-offset.

---

## NVMCON Register (Offset 0x00)

```
Bit 15   14    13   12    [11:8]  7    6    [5:4]  [3:0]
    WR   WREN  WRERR LVDERR  —   PFSWAP BFSWAP  —    NVMOP
```

| Bits | Name | R/W | Reset | Description |
|------|------|-----|-------|-------------|
| 3:0 | NVMOP | R/W | 0 | Operation code (see Operations table) |
| 6 | BFSWAP | R/W | 0 | Boot Flash Bank Swap |
| 7 | PFSWAP | R/W | 0 | Program Flash Bank Swap |
| 12 | LVDERR | R | 0 | Low-Voltage Detect Error |
| 13 | WRERR | R | 0 | Write Error |
| 14 | WREN | R/W | 0 | Write Enable — must be set before write/erase commands |
| 15 | WR | R/W | 0 | Start Operation — 0→1 transition triggers command |

### Bit Masks (from `pic32mk.h`)

```c
#define PIC32MK_NVMCON_NVMOP_MASK  0x000Fu  /* bits [3:0] */
#define PIC32MK_NVMCON_BFSWAP     (1u << 6)
#define PIC32MK_NVMCON_PFSWAP     (1u << 7)
#define PIC32MK_NVMCON_LVDERR     (1u << 12)
#define PIC32MK_NVMCON_WRERR      (1u << 13)
#define PIC32MK_NVMCON_WREN       (1u << 14)  /* bit 14: Write Enable */
#define PIC32MK_NVMCON_WR         (1u << 15)  /* bit 15: Start Operation */
```

---

## Operations (NVMOP Field)

| NVMOP | Value | Name | WREN | Unlock | Description |
|-------|-------|------|------|--------|-------------|
| 0 | `0x0` | NOP | No | No | No operation |
| 1 | `0x1` | WORD_PROG | Yes | Yes | Write NVMDATA0 → 32-bit word at NVMADDR |
| 2 | `0x2` | QUAD_WORD_PROG | Yes | Yes | Write NVMDATA0–3 → 128 bits at NVMADDR (16-byte aligned) |
| 3 | `0x3` | ROW_PROG | Yes | Yes | Write 512 bytes from NVMSRCADDR → row at NVMADDR |
| 4 | `0x4` | PAGE_ERASE | Yes | Yes | Erase 4 KB page containing NVMADDR → all 0xFF |
| 5 | `0x5` | LOWER_PFM_ERASE | — | — | *Unimplemented* (LOG_UNIMP) |
| 6 | `0x6` | UPPER_PFM_ERASE | — | — | *Unimplemented* (LOG_UNIMP) |
| 7 | `0x7` | PFM_ERASE | Yes | Yes | Erase entire 1 MB flash → all 0xFF |

### Command Execution Flow

```
1. Firmware sets NVMDATA0[-3], NVMADDR, NVMSRCADDR as needed
2. Firmware clears WREN, sets NVMOP, re-sets WREN
3. Firmware writes NVMKEY sequence: 0xAA996655 then 0x556699AA
4. Firmware sets WR bit (NVMCONSET = 0x8000)
5. QEMU detects WR 0→1 transition
6. nvm_execute_cmd() runs:
   a. Clear previous WRERR/LVDERR
   b. Verify WREN=1 and NVMKEY unlocked
   c. Execute command based on NVMOP field
   d. Clear WR bit
   e. Reset NVMKEY FSM to LOCKED
   f. Pulse IRQ 31 (sets IFS0[31])
7. Firmware polls WR == 0 (or handles NVM interrupt)
```

### Operation Details

#### WORD_PROG (NVMOP=1)
- **Input:** `NVMADDR` = physical address (word-aligned), `NVMDATA0` = value
- **Address range:** 0x1D000000–0x1D0FFFFF
- **Pre-conditions:** WREN=1, NVMKEY unlocked
- **Effect:** 4 bytes from NVMDATA0 written to flash at NVMADDR
- **Failure:** WRERR if WREN=0, locked, or out-of-range

#### QUAD_WORD_PROG (NVMOP=2)
- **Input:** `NVMADDR` = physical address (16-byte aligned), `NVMDATA0–3` = 128 bits
- **Pre-conditions:** WREN=1, NVMKEY unlocked
- **Effect:** 16 bytes from NVMDATA0–3 written to flash at NVMADDR
- **Alignment enforced:** Address is masked to 16-byte boundary

#### ROW_PROG (NVMOP=3)
- **Input:** `NVMADDR` = destination (row-aligned), `NVMSRCADDR` = source physical address
- **Pre-conditions:** WREN=1, NVMKEY unlocked
- **Effect:** 512 bytes copied from guest RAM at NVMSRCADDR to flash at NVMADDR
- **Uses `dma_memory_read()`** to read source data from guest address space

#### PAGE_ERASE (NVMOP=4)
- **Input:** `NVMADDR` = any address within the target page
- **Pre-conditions:** WREN=1, NVMKEY unlocked
- **Effect:** 4096 bytes at page boundary → `0xFF`
- **Page alignment:** `offset &= ~(4096 - 1)`

#### PFM_ERASE (NVMOP=7)
- **Pre-conditions:** WREN=1, NVMKEY unlocked
- **Effect:** Entire 1 MB flash → `0xFF`
- **NVMADDR ignored**

---

## NVMKEY Unlock FSM

The NVMKEY register implements a two-step unlock sequence to prevent
accidental flash writes. This is a write-only register that always reads
as 0.

### FSM State Diagram

```
                ┌─────────────┐
                │   LOCKED    │◄──── Reset / After command
                │  (state 0)  │◄──── Wrong key at any state
                └──────┬──────┘
                       │ Write 0xAA996655
                       ▼
                ┌─────────────┐
                │  KEY1_OK    │
                │  (state 1)  │
                └──────┬──────┘
                       │ Write 0x556699AA
                       ▼
                ┌─────────────┐
                │  UNLOCKED   │───── Valid for ONE command
                │  (state 2)  │      then auto-locks
                └─────────────┘
```

### Unlock Keys

| Step | Value | Constant |
|------|-------|----------|
| 1st  | `0xAA996655` | `PIC32MK_NVMKEY1` |
| 2nd  | `0x556699AA` | `PIC32MK_NVMKEY2` |

### Rules
- Writing any value other than the expected key resets to LOCKED
- After command execution (WR cleared), FSM resets to LOCKED
- Unlock is consumed by a single command — must re-unlock for each operation
- The unlock + WR set must be done with interrupts disabled (atomicity)

---

## Flash Geometry

| Parameter | Value |
|-----------|-------|
| Total size | 1 MB (1,048,576 bytes) |
| Word width | 32 bits |
| Page size | 4096 bytes (erase granularity) |
| Row size | 512 bytes (row-program granularity) |
| Quad-word size | 16 bytes (128 bits) |
| Physical base | 0x1D000000 |
| KSEG0 base | 0x9D000000 (cached) |
| KSEG1 base | 0xBD000000 (uncached) |
| Erased value | `0xFF` per byte / `0xFFFFFFFF` per word |

---

## Interrupt Handling

### EVIC Integration

The NVM controller asserts EVIC source 31 (`_FLASH_CONTROL_VECTOR`) upon
completion of every operation (including NOP). The IRQ is pulsed — raised
then immediately lowered — so IFS0[31] is latched but `irq_level` clears.

| Property | Value |
|----------|-------|
| EVIC source | 31 |
| IFS register | IFS0, bit 31 (mask `0x80000000`) |
| IEC register | IEC0, bit 31 (mask `0x80000000`) |
| IPC register | IPC7, bits [28:26] (priority), bits [25:24] (subpriority) |
| Priority (firmware default) | 1 (set by `plib_evic.c`: `IPC7SET = 0x4000000`) |

### Interrupt Sequence

```
NVM operation complete
    │
    ▼
qemu_irq_pulse(s->irq)
    │
    ├── level=1: irq_level[0] |= (1<<31), IFS0[31] = 1
    ├── evic_update() — if IEC0[31]=0, no CPU interrupt yet
    ├── level=0: irq_level[0] &= ~(1<<31)
    └── evic_update()

... firmware enables IEC0[31] via IEC0SET ...
    │
    ▼
evic_update() — IFS0[31]=1 AND IEC0[31]=1 at priority 1
    │
    ▼
cpu_irq[2] asserted → CPU takes interrupt
    │
    ▼
_int_handler (crt0.S) checks IFS0 bit 31
    │
    ▼
vNVMInterruptWrapper (port_asm_patched.S)
    ├── portSAVE_CONTEXT
    ├── jal NVM_InterruptHandler
    │       ├── IFS0CLR = 0x80000000  (clears IFS0[31])
    │       └── invoke registered callback (if any)
    └── portRESTORE_CONTEXT
```

### Key Detail: Why Pulse Works

After `qemu_irq_pulse`, `irq_level[0]` bit 31 is 0. When the ISR clears
IFS0[31] via `IFS0CLR`, the EVIC's re-assertion logic
(`ifsreg[0] |= irq_level[0]`) does NOT re-set IFS0[31] because
`irq_level` bit 31 is already 0. The interrupt fires exactly once per
NVM operation.

---

## Board Integration

The NVM device is instantiated in `pic32mk.c` during board init:

```c
/* NVM / Flash Controller at 0xBF800A00 */
{
    DeviceState *nvm = qdev_new(TYPE_PIC32MK_NVM);  /* "pic32mk-nvm" */
    object_property_set_link(OBJECT(nvm), "pflash",
        OBJECT(&s->pflash), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(nvm), &error_fatal);
    MemoryRegion *mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(nvm), 0);
    memory_region_add_subregion_overlap(&s->sfr, PIC32MK_NVM_OFFSET, mr, 1);
    sysbus_connect_irq(SYS_BUS_DEVICE(nvm), 0,
                       qdev_get_gpio_in(s->evic, PIC32MK_IRQ_FCE));
}
```

### Program Flash Region

```c
/* 1 MB Program Flash — RAM-backed so NVM controller can write */
memory_region_init_ram(&s->pflash, NULL, "pic32mk.pflash",
                       PIC32MK_PFLASH_SIZE, &error_fatal);
memory_region_add_subregion(sys_mem, PIC32MK_PFLASH_BASE, &s->pflash);
```

**Critical:** The program flash must be `memory_region_init_ram()`, not
`memory_region_init_rom()`. ROM regions reject writes, which would cause
all NVM operations to silently fail (the memcpy in `nvm_execute_cmd` would
have no effect).

| Property | Value |
|----------|-------|
| QEMU type name | `pic32mk-nvm` |
| Parent type | `TYPE_SYS_BUS_DEVICE` |
| SFR offset | `0x000A00` (maps to 0xBF800A00) |
| MMIO size | 0xB0 bytes |
| IRQ output | EVIC vector 31 (`_FLASH_CONTROL_VECTOR`) |
| QOM link | `pflash` → program flash MemoryRegion |

---

## Firmware-Side Integration

### xc.h SFR Definitions

The GCC compatibility shim (`tests/.../config/default/xc.h`) provides all
register definitions the Harmony plib needs:

```c
/* NVM base */
#define _NVM_BASE  0xBF800A00u

/* NVMCON register + SET/CLR/INV aliases */
#define NVMCON      (*(volatile uint32_t *)(_NVM_BASE + 0x00u))
#define NVMCONCLR   (*(volatile uint32_t *)(_NVM_BASE + 0x04u))
#define NVMCONSET   (*(volatile uint32_t *)(_NVM_BASE + 0x08u))
#define NVMCONINV   (*(volatile uint32_t *)(_NVM_BASE + 0x0Cu))

/* NVMCONbits bitfield union */
typedef union {
    struct {
        uint32_t NVMOP  : 4;  uint32_t        : 2;
        uint32_t BFSWAP : 1;  uint32_t PFSWAP : 1;
        uint32_t        : 4;
        uint32_t LVDERR : 1;  uint32_t WRERR  : 1;
        uint32_t WREN   : 1;  uint32_t WR     : 1;
        uint32_t        : 16;
    };
    uint32_t w;
} __NVMCON_t;
#define NVMCONbits  (*(volatile __NVMCON_t *)(_NVM_BASE + 0x00u))

/* Bit masks used by plib */
#define _NVMCON_NVMOP_MASK     0x0000000Fu
#define _NVMCON_NVMOP_POSITION 0
#define _NVMCON_WREN_MASK      0x00004000u
#define _NVMCON_WR_MASK        0x00008000u
#define _NVMCON_WRERR_MASK     0x00002000u
#define _NVMCON_LVDERR_MASK    0x00001000u
#define _NVMCON_PFSWAP_MASK    0x00000080u

/* NVMKEY — write-only unlock register */
#define NVMKEY      (*(volatile uint32_t *)(_NVM_BASE + 0x10u))

/* NVMADDR, NVMDATA0–3, NVMSRCADDR, NVMPWP, NVMBWP, NVMCON2 + CLR/SET/INV */
#define NVMADDR     (*(volatile uint32_t *)(_NVM_BASE + 0x20u))
#define NVMDATA0    (*(volatile uint32_t *)(_NVM_BASE + 0x30u))
#define NVMDATA1    (*(volatile uint32_t *)(_NVM_BASE + 0x40u))
#define NVMDATA2    (*(volatile uint32_t *)(_NVM_BASE + 0x50u))
#define NVMDATA3    (*(volatile uint32_t *)(_NVM_BASE + 0x60u))
#define NVMSRCADDR  (*(volatile uint32_t *)(_NVM_BASE + 0x70u))
#define NVMPWP      (*(volatile uint32_t *)(_NVM_BASE + 0x80u))
#define NVMBWP      (*(volatile uint32_t *)(_NVM_BASE + 0x90u))
#define NVMCON2     (*(volatile uint32_t *)(_NVM_BASE + 0xA0u))
/* ... CLR/SET/INV variants for all registers also defined ... */
```

### kmem.h Macros

The `sys/kmem.h` shim provides MIPS address-space conversion macros:

```c
#define KVA_TO_PA(v)    ((uint32_t)(uintptr_t)(v) & 0x1FFFFFFFu)
#define PA_TO_KVA0(pa)  ((void *)((uintptr_t)(pa) | 0x80000000u))
#define PA_TO_KVA1(pa)  ((void *)((uintptr_t)(pa) | 0xA0000000u))
#define KVA0_TO_KVA1(v) ((void *)(((uintptr_t)(v) & 0x1FFFFFFFu) | 0xA0000000u))
```

These are used by the plib's `NVM_Read()` (via `KVA0_TO_KVA1`) to read back
flash contents through the uncached KSEG1 alias.

### Harmony plib API

The Harmony-generated NVM plib is located at:
`tests/pic32mk_rtos_demo/firmware/src/config/default/peripheral/nvm/`

These files are **unmodified** copies from the Harmony 3 project.

| Function | Description |
|----------|-------------|
| `NVM_Initialize()` | Perform NOP operation to clear state |
| `NVM_WordWrite(data, addr)` | Write 32-bit word (unlock + WR) |
| `NVM_QuadWordWrite(data, addr)` | Write 128 bits / 4 words (unlock + WR) |
| `NVM_RowWrite(data, addr)` | Write 512-byte row from RAM (unlock + WR) |
| `NVM_PageErase(addr)` | Erase 4 KB page (unlock + WR) |
| `NVM_Read(buf, len, addr)` | memcpy from KSEG1 alias of flash |
| `NVM_IsBusy()` | Returns `true` if WR bit is set |
| `NVM_ErrorGet()` | Returns `NVMCON & (WRERR | LVDERR)` |
| `NVM_CallbackRegister(cb, ctx)` | Register completion callback |

### plib Interrupt Behavior

Every plib write/erase operation (via `NVM_StartOperationAtAddress`) ends
with:

```c
IEC0SET = 0x80000000;   /* Enable IEC0[31] — NVM interrupt */
```

This means the NVM interrupt **must** be properly dispatched in the firmware
ISR chain. On operation completion, the QEMU NVM device pulses IRQ 31, which
sets IFS0[31]. With IEC0[31] enabled, the EVIC routes the interrupt to the
CPU at priority 1.

---

## ISR Dispatch Chain

### The Problem (Before Fix)

The plib unconditionally enables IEC0[31] after each NVM operation. Without
a matching ISR entry in the firmware dispatch chain, the interrupt falls
through to the default Timer1 tick handler, which does NOT clear IFS0[31].
Result: infinite re-entry at priority 1, starving the FreeRTOS scheduler.

### The Fix

Three files were modified to add NVM interrupt support:

#### 1. crt0.S — Dispatch Logic

Added IFS0 bit 31 check in `_int_handler`, before the default Timer1
fallthrough:

```asm
    /* NVM / Flash Control — IFS0 bit 31 */
    lui     $26, 0xBF81             /* reload EVIC base */
    lw      $27, 0x0040($26)        /* $27 = IFS0 */
    lw      $26, 0x00C0($26)        /* $26 = IEC0 */
    and     $26, $27, $26           /* pending0 = IFS0 & IEC0 */
    lui     $27, 0x8000             /* bit 31 mask = 0x80000000 */
    and     $27, $26, $27
    bnez    $27, _nvm_int
    nop
```

Note: `andi` cannot test bit 31 (16-bit immediate limit), so `lui` is used
to construct the 0x80000000 mask.

#### 2. port_asm_patched.S — FreeRTOS-Safe Wrapper

```asm
    .extern NVM_InterruptHandler

    .global vNVMInterruptWrapper
    .ent    vNVMInterruptWrapper
vNVMInterruptWrapper:
    portSAVE_CONTEXT
    jal     NVM_InterruptHandler
    nop
    portRESTORE_CONTEXT
    .end    vNVMInterruptWrapper
```

This follows the exact same pattern as all other ISR wrappers (UART, CAN,
USB, ADC, GPIO).

#### 3. interrupts.h — Prototype

```c
void NVM_InterruptHandler(void);
```

The implementation lives in `plib_nvm.c` (Harmony, unmodified):

```c
void __attribute__((used)) NVM_InterruptHandler(void) {
    IFS0CLR = 0x80000000;   /* Clear IFS0[31] */
    if (nvmCallbackObj.CallbackFunc != NULL) {
        nvmCallbackObj.CallbackFunc(nvmCallbackObj.Context);
    }
}
```

### Full ISR Dispatch Order (crt0.S `_int_handler`)

```
1. CS0 (yield)          — IFS0 bit 1     → vPortYieldISR
2. UART1 TX/RX/Fault    — IFS1 bits 8/7/6 → vUART1*InterruptWrapper
3. USB1                  — IFS1 bit 2     → vUSB1InterruptWrapper
4. Change Notice A       — IFS1 bit 12    → vCNAInterruptWrapper
5. UART2 TX/RX/Fault    — IFS3 bits 21/20/19 → vUART2*InterruptWrapper
6. ADC End-of-Scan       — IFS3 bit 5     → vADCEOSInterruptWrapper
7. CAN1                  — IFS5 bit 7     → vCAN1InterruptWrapper
8. CAN2                  — IFS5 bit 8     → vCAN2InterruptWrapper
9. NVM / Flash Control   — IFS0 bit 31    → vNVMInterruptWrapper    ← NEW
10. Default              — (any other)     → vPortTickInterruptHandler
```

---

## Test Task: vNvmTestTask

Located in `tests/pic32mk_rtos_demo/firmware/src/main.c`.

### Registration

```c
/* In main(), after NVM_Initialize(): */
NVM_Initialize();
/* ... */
xTaskCreate(vNvmTestTask, "NVM", configMINIMAL_STACK_SIZE * 2,
            NULL, tskIDLE_PRIORITY + 1, NULL);
```

### Test Sequence

| Step | Operation | plib Call | Expected |
|------|-----------|-----------|----------|
| 1 | Write 4 words | `NVM_WordWrite()` ×4 | 0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xA5A5A5A5 |
| 2 | Read back & verify | `NVM_Read()` | Must match written values |
| 3 | Check error status | `NVM_ErrorGet()` | Must be `NVM_ERROR_NONE` |
| 4 | Quad-word write | `NVM_QuadWordWrite()` | 0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00 |
| 5 | Read back & verify | `NVM_Read()` | Must match written values |
| 6 | Page erase | `NVM_PageErase()` | 4 KB → all 0xFFFFFFFF |
| 7 | Read back & verify | `NVM_Read()` | All words = 0xFFFFFFFF |

Test address: `NVM_FLASH_START_ADDRESS + 0xFF000` (last page, 0x9D0FF000).

### Expected UART Output

```
[NVM] NVM test starting...
[NVM] Write/Read: 4/4 PASS
[NVM] QuadWord: PASS
[NVM] PageErase: 4/4 PASS
[NVM] NVM test complete.
```

### Verified Behavior

After the NVM test completes, the FreeRTOS scheduler continues normally:

```
[NVM] NVM test complete.
[00:00:01] Hello from FreeRTOS!
[00:00:02] Hello from FreeRTOS!
[CAN] TX #1 id=0x100 data='hello'
...
```

No watchdog resets, no scheduler hang — confirming that the NVM interrupt
(IEC0[31]) is properly handled by `NVM_InterruptHandler`.

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│  QEMU host process                                              │
│                                                                 │
│  ┌──────────────────┐     ┌───────────────────────────────────┐ │
│  │  MIPS CPU model  │────▶│  pic32mk_nvm.c  (NVM emulator)    │ │
│  │  (FreeRTOS +     │     │                                   │ │
│  │   Harmony plib)  │◀────│  • SFR MMIO (0xB0 bytes)          │ │
│  └──────────────────┘     │  • NVMKEY unlock FSM              │ │
│         ▲    │            │  • nvm_execute_cmd() on WR 0→1    │ │
│         │    │            │  • memcpy/memset into pflash RAM  │ │
│         │    ▼            │  • qemu_irq_pulse → EVIC #31      │ │
│  ┌──────────────────┐     └──────────────┬────────────────────┘ │
│  │  EVIC model      │                    │                      │
│  │  (IRQ dispatch)  │◀───── irq pulse ───┘                      │
│  │  IFS0[31]=1      │                                           │
│  └───────┬──────────┘                                           │
│          │ cpu_irq[2] (priority 1)                              │
│          ▼                                                      │
│  ┌──────────────────┐     ┌───────────────────────────────────┐ │
│  │  CPU exception   │────▶│  _int_handler (crt0.S)            │ │
│  │  EBASE + 0x200   │     │  → checks IFS0[31] & IEC0[31]     │ │
│  │                  │     │  → vNVMInterruptWrapper           │ │
│  │                  │     │    → NVM_InterruptHandler         │ │
│  │                  │     │      → IFS0CLR = 0x80000000       │ │
│  └──────────────────┘     └───────────────────────────────────┘ │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  Program Flash MemoryRegion (RAM-backed, 1 MB)           │   │
│  │  Physical: 0x1D000000 – 0x1D0FFFFF                       │   │
│  │  KSEG0:    0x9D000000 (cached, used by plib addresses)   │   │
│  │  KSEG1:    0xBD000000 (uncached, used by NVM_Read)       │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Running the Test

```bash
# Build firmware (from repo root)
make -C tests/pic32mk_rtos_demo

# Build QEMU (if needed)
make -C build -j$(nproc)

# Run with 15-second timeout
timeout 15 ./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors
```

### What to Look For

1. `[NVM] Write/Read: 4/4 PASS` — word program and read-back works
2. `[NVM] QuadWord: PASS` — quad-word program works
3. `[NVM] PageErase: 4/4 PASS` — page erase works
4. `Hello from FreeRTOS!` continues after NVM test — scheduler is healthy
5. No `watchdog timeout` — interrupt handling is correct

---

## Limitations and Future Work

| Item | Status | Notes |
|------|--------|-------|
| Word program | Implemented | NVMOP=1 |
| Quad-word program | Implemented | NVMOP=2 |
| Row program | Implemented | NVMOP=3, uses `dma_memory_read()` |
| Page erase | Implemented | NVMOP=4 |
| PFM erase | Implemented | NVMOP=7 |
| Lower/Upper PFM erase | Stub | LOG_UNIMP, NVMOP=5/6 |
| Write protection (NVMPWP) | Stub | Register exists, not enforced |
| Boot write protection (NVMBWP) | Stub | Register exists, not enforced |
| Dual-panel swap (PFSWAP/BFSWAP) | Stub | Bits exist, not functional |
| Asynchronous timing | Not implemented | All operations are synchronous |
| NVM callback support | Supported | Via `NVM_CallbackRegister()` (plib) |
| Host-file persistence | Not implemented | Flash contents lost on QEMU exit |
