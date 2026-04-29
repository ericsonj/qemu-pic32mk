# PIC32MK1024MCM100 — CAN FD Peripheral Emulation Reference

> Based on PIC32MK GPK/MCM with CAN FD Family Datasheet (DS60001519E) and Microchip CAN FD Controller Reference Manual (DS60001507).

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [SFR Base Addresses](#2-sfr-base-addresses)
3. [Register Map](#3-register-map)
4. [Key Register Bit Fields](#4-key-register-bit-fields)
   - 4.1 [CiCON — Control Register](#41-cicon--control-register)
   - 4.2 [CiINT — Interrupt Aggregator](#42-ciint--interrupt-aggregator)
   - 4.3 [CiFIFOCONn — FIFO Control](#43-cififoconn--fifo-control)
5. [Message RAM Layout](#5-message-ram-layout)
   - 5.1 [Region Ordering](#51-region-ordering)
   - 5.2 [Message Object Format (TX)](#52-message-object-format-tx)
   - 5.3 [Message Object Format (RX)](#53-message-object-format-rx)
   - 5.4 [DLC → Byte Count Mapping](#54-dlc--byte-count-mapping)
   - 5.5 [PLSIZE → Object Size](#55-plsize--object-size)
6. [Operating Modes](#6-operating-modes)
7. [Interrupt System](#7-interrupt-system)
8. [Acceptance Filters](#8-acceptance-filters)
9. [QEMU Implementation](#9-qemu-implementation)
   - 9.1 [Device State Struct](#91-device-state-struct)
   - 9.2 [MemoryRegion Setup](#92-memoryregion-setup)
   - 9.3 [MMIO Read/Write Dispatcher](#93-mmio-readwrite-dispatcher)
   - 9.4 [Mode Transition Handler](#94-mode-transition-handler)
   - 9.5 [FIFO UINC Protocol](#95-fifo-uinc-protocol)
   - 9.6 [TX Path](#96-tx-path)
   - 9.7 [RX Deliver Path](#97-rx-deliver-path)
   - 9.8 [IRQ Update Logic](#98-irq-update-logic)
   - 9.9 [Virtual CAN Bus — Host Integration](#99-virtual-can-bus--host-integration)
10. [TX Sequence (Firmware Perspective)](#10-tx-sequence-firmware-perspective)
11. [RX Sequence (Firmware Perspective)](#11-rx-sequence-firmware-perspective)
12. [Key Emulation Pitfalls](#12-key-emulation-pitfalls)
13. [Useful References](#13-useful-references)

---

## 1. Architecture Overview

The PIC32MK1024MCM100 has **4 CAN FD controller instances** (CAN1–CAN4), all based on the Microchip MCP2517FD/CAN FD IP core. Key facts:

| Property | Value |
|---|---|
| Protocol | ISO 11898-1:2015 CAN FD (up to 8 Mbit/s data phase) |
| Instances | CAN1, CAN2, CAN3, CAN4 |
| Peripheral Bus | Bus 5 (shared with USB and ADC) |
| DMA access | CAN1–CAN4 are bus initiators — direct RAM access |
| Message RAM | Separate per-instance SRAM, CPU-mapped |
| TX Queue (TXQ) | Dedicated priority-based transmit queue |
| Configurable FIFOs | Up to 32 FIFOs (each configurable as TX or RX) |
| Acceptance filters | Up to 32 filter/mask pairs |
| Max payload | 64 bytes (CAN FD) / 8 bytes (Classic CAN 2.0) |
| Bit timing | Separate nominal (NBT) and data (DBT) phases |
| Clock source | CANCLK (from CRU, configurable) |

### Why CAN FD is harder to emulate than UART

Unlike UART — where firmware writes a byte to a register and the hardware sends it — CAN FD **inverts the data path**:

- Firmware writes an entire **message object** (2 header words + up to 64 bytes of payload) into a specific slot in **Message RAM**.
- The controller reads from RAM independently.
- Two `MemoryRegion` objects are needed per instance: one for the SFR control registers and one for the Message RAM.

---

## 2. SFR Base Addresses

CAN1–CAN4 SFR base: `0xBF880000` (from Table 4-2). Each instance is offset by `0x1000`.

| Instance | Virtual Base | Physical Base | IRQ Vector | IRQ # |
|---|---|---|---|---|
| CAN1 | `0xBF880000` | `0x1F880000` | `_CAN1_VECTOR` | 167 |
| CAN2 | `0xBF881000` | `0x1F881000` | `_CAN2_VECTOR` | 168 |
| CAN3 | `0xBF882000` | `0x1F882000` | `_CAN3_VECTOR` | 187 |
| CAN4 | `0xBF883000` | `0x1F883000` | `_CAN4_VECTOR` | 188 |

> **Note:** Each CAN FD instance has a **single global IRQ vector** (unlike UART which has 3). Firmware reads `CiINT` to determine what triggered, then `CiRXIF`/`CiTXIF` to find which FIFO.

---

## 3. Register Map

All offsets are relative to the instance base address.

| Offset | Register | Description | Reset Value |
|---|---|---|---|
| `0x000` | `CiCON` | Control — master enable, mode, config lock | `0x04980760` |
| `0x004` | `CiNBTCFG` | Nominal bit time config — BRP, TSEG1, TSEG2, SJW | `0x00000000` |
| `0x008` | `CiDBTCFG` | Data bit time config — BRP, TSEG1, TSEG2, SJW | `0x00000000` |
| `0x00C` | `CiTDC` | Transmitter delay compensation | `0x00000000` |
| `0x010` | `CiTBC` | Time base counter (free-running) | `0x00000000` |
| `0x014` | `CiTSCON` | Timestamp control | `0x00000000` |
| `0x018` | `CiVEC` | Interrupt flag code — which FIFO caused IRQ | `0x00000000` |
| `0x01C` | `CiINT` | Interrupt status & enable — global aggregator | `0x00000000` |
| `0x020` | `CiRXIF` | RX interrupt flag per FIFO (bitmask FIFO0–FIFO31) | `0x00000000` |
| `0x028` | `CiTXIF` | TX interrupt flag per FIFO | `0x00000000` |
| `0x030` | `CiRXOVIF` | RX overflow flag per FIFO | `0x00000000` |
| `0x038` | `CiTXREQ` | TX request bits per FIFO — set to trigger TX | `0x00000000` |
| `0x040` | `CiTREC` | TX/RX error counters (TEC, REC) | `0x00000000` |
| `0x044` | `CiBDIAG0` | Bus diagnostic 0 (error counts) | `0x00000000` |
| `0x048` | `CiBDIAG1` | Bus diagnostic 1 (error flags) | `0x00000000` |
| `0x04C` | `CiTEFCON` | TX Event FIFO control | `0x00000000` |
| `0x050` | `CiTEFSTA` | TX Event FIFO status | `0x00000000` |
| `0x054` | `CiTEFUA` | TX Event FIFO user address | `0x00000000` |
| `0x058` | `CiTXQCON` | TX Queue control — priority, payload size, depth | `0x00000000` |
| `0x05C` | `CiTXQSTA` | TX Queue status — TXQNIF, TXQEIF | `0x00000000` |
| `0x060` | `CiTXQUA` | TX Queue user address (where firmware writes next TX object) | `0x00000000` |
| `0x064` | `CiFIFOCON1` | FIFO 1 control | `0x00000000` |
| `0x068` | `CiFIFOSTA1` | FIFO 1 status | `0x00000000` |
| `0x06C` | `CiFIFOUA1` | FIFO 1 user address | `0x00000000` |
| `0x070…` | `CiFIFOCONn…` | FIFOn control/status/UA — repeats every `0xC` bytes for n=1..31 | — |
| `0x1D0` | `CiFLTCON0` | Filter 0 control — enable, FIFO pointer | `0x00000000` |
| `0x1D4…` | `CiFLTCONn…` | Filter n control — 32 filters, 4 packed per 32-bit register | — |
| `0x1F0` | `CiFLTOBJ0` | Filter 0 object (ID to match) | `0x00000000` |
| `0x1F4` | `CiMASK0` | Filter 0 mask | `0x00000000` |
| `0x1F8…` | `CiFLTOBJn / CiMASKn` | Filters 1–31 (8 bytes each: OBJ + MASK, interleaved) | — |

---

## 4. Key Register Bit Fields

### 4.1 CiCON — Control Register

| Bits | Name | Access | Description & Emulation Notes |
|---|---|---|---|
| 31–29 | `TXBWS` | R/W | TX bandwidth sharing. Store; not needed functionally. |
| 28 | `ABAT` | R/W | Abort all pending TX. Writing 1: cancel all queued TX, clear all TXREQ bits, set TXATIF in each FIFO status. HW clears this bit. |
| 27–25 | `REQOP` | R/W | **Requested operating mode.** Writing triggers mode change. Mirror to OPMOD immediately in emulation. |
| 24–22 | `OPMOD` | R only | **Current operating mode.** Firmware polls after writing REQOP. Must equal REQOP after transition. |
| 21 | `TXQEN` | R/W | Enable TX Queue. When 1, TXQ is present; firmware can use CiTXQCON/STA/UA. |
| 20 | `STEF` | R/W | Store TX Events. When 1, enable TX Event FIFO (TEF). |
| 19 | `SERR2LOM` | R/W | Stuff Error → Listen Only Mode. Store only. |
| 16 | `RTXAT` | R/W | Restrict retransmission attempts. Store. |
| 12 | `BRSDIS` | R/W | Disable bit rate switch. Store; no functional impact in emulation. |
| 7 | `BUSY` | R only | Module busy. **Always return 0** in emulation (instant operation). |
| 6 | `BRPSEL` | R/W | Bit rate prescaler (0=1x, 1=2x). Store for baud calculation. |

### 4.2 CiINT — Interrupt Aggregator

Upper 16 bits = enable flags, lower 16 bits = status flags.

| Bits | Name | Description & Emulation Trigger |
|---|---|---|
| 31 | `IVMIE` | Invalid message IRQ enable |
| 29 | `CERRIE` | CAN error IRQ enable |
| 27 | `RXOVIE` | RX overflow IRQ enable |
| 24 | `ECCIE` | ECC error IRQ enable |
| 23 | `TEFIE` | TX Event FIFO IRQ enable |
| 20 | `MODIE` | Mode change IRQ enable |
| 18 | `RXIE` | RX IRQ enable — gates `CiRXIF` → IRQ line |
| 17 | `TXIE` | TX IRQ enable — gates `CiTXIF` → IRQ line |
| 15 | `IVMIF` | Invalid message flag |
| 11 | `RXOVIF` | RX overflow — set when any `CiRXOVIF` bit is set |
| 8 | `ECCIF` | ECC error — always 0 in emulation |
| 7 | `TEFIF` | TX Event FIFO flag — set when TX event written to TEF |
| 4 | `MODIF` | **Must set when OPMOD transitions.** Firmware clears by writing 0. |
| 1 | `RXIF` | **Must set when `CiRXIF ≠ 0` and `RXIE=1`.** Assert IRQ line. |
| 0 | `TXIF` | **Must set when `CiTXIF ≠ 0` and `TXIE=1`.** Assert IRQ line. |

### 4.3 CiFIFOCONn — FIFO Control

First FIFO at `0x064`, stride `0xC` per FIFO (n = 1..31).

| Bits | Name | Description & Emulation Notes |
|---|---|---|
| 31–29 | `PLSIZE` | Payload size: `000`=8B, `001`=12B, `010`=16B, `011`=20B, `100`=24B, `101`=32B, `110`=48B, `111`=64B |
| 28–24 | `FSIZE` | FIFO depth: 0 = 1 entry, 31 = 32 entries. With PLSIZE determines Message RAM usage. |
| 20 | `TXEN` | 1 = TX FIFO, 0 = RX FIFO |
| 19 | `UINC` | Write 1 to advance head/tail pointer. **HW clears immediately.** Critical — see §9.5. |
| 18 | `TXREQ` | TX Request (TX FIFOs). Firmware sets to request TX. Emulator: process and clear. |
| 17 | `FRESET` | FIFO reset — clears head/tail, empties FIFO. HW clears this bit. |
| 7 | `TXNIE` | TX FIFO not full IRQ enable |
| 6 | `TXEIE` | TX empty IRQ enable |
| 5 | `RXOVIE` | RX overflow IRQ enable |
| 4 | `TFERFFIE` | TX/RX FIFO full IRQ enable |
| 3 | `TFHRFHIE` | TX/RX FIFO half full IRQ enable |
| 2 | `TFNRFNIE` | TX/RX FIFO not full / not empty IRQ enable |

---

## 5. Message RAM Layout

Message RAM is a dedicated SRAM region visible to the CPU, DMA, and CAN controller. Its size and layout are **determined at runtime** by how firmware configures the FIFOs/TXQ/TEF.

### 5.1 Region Ordering

Regions are allocated sequentially starting at Message RAM base:

| Region | Condition | Size formula |
|---|---|---|
| TX Queue (TXQ) | `CiCON.TXQEN = 1` | `(PLSIZE_bytes + 8) × (FSIZE + 1)` bytes |
| TX Event FIFO (TEF) | `CiCON.STEF = 1` | `8 × (FSIZE + 1)` bytes (TEF objects have no payload) |
| FIFO 1–31 | Always | `(PLSIZE_bytes + 8) × (FSIZE + 1)` bytes each, sequentially |

The **UA (user address) register** for each FIFO/TXQ/TEF contains the physical RAM address of the current head or tail slot.

### 5.2 Message Object Format (TX)

| Word Offset | Field | Bits | Description |
|---|---|---|---|
| Word 0 (T0) | SID/EID | 28:0 | Standard ID [11:0] = T0[10:0]; Extended ID [28:11] = T0[28:11], [10:0] = T0[10:0] |
| Word 0 (T0) | RTR | 29 | Remote Transmission Request |
| Word 0 (T0) | XTD | 30 | Extended ID (1 = 29-bit ID, 0 = 11-bit ID) |
| Word 0 (T0) | ESI | 31 | Error Status Indicator (set by sender in error passive state) |
| Word 1 (T1) | DLC | 3:0 | Data length code (0–15) |
| Word 1 (T1) | IDE | 4 | ID Extension flag |
| Word 1 (T1) | RTR | 5 | Remote frame flag |
| Word 1 (T1) | BRS | 6 | Bit Rate Switch — use DBT for data phase |
| Word 1 (T1) | FDF | 7 | FD Frame — 1 = CAN FD, 0 = Classic CAN |
| Word 1 (T1) | SEQ | 31:9 | Sequence number (firmware assigns; copied to TEF on TX complete) |
| Words 2–N | DATA | — | Payload bytes, little-endian, padded to next word boundary |

### 5.3 Message Object Format (RX)

T0 word (R0) is identical to TX. R1 differs:

| Word Offset | Field | Bits | Description |
|---|---|---|---|
| Word 1 (R1) | DLC | 3:0 | Received DLC |
| Word 1 (R1) | IDE | 4 | Extended ID flag |
| Word 1 (R1) | RTR | 5 | Remote frame |
| Word 1 (R1) | BRS | 6 | Bit rate switch was used |
| Word 1 (R1) | FDF | 7 | Was a CAN FD frame |
| Word 1 (R1) | ESI | 8 | Sender was in error passive state |
| Word 1 (R1) | FILHIT | 15:11 | **Filter index that matched** (0–31). Emulator must fill this. |
| Word 1 (R1) | RXTS | 31:16 | RX timestamp (from `CiTBC` if `CiTSCON` enabled) |
| Words 2–N | DATA | — | Received payload |

### 5.4 DLC → Byte Count Mapping

| DLC | Classic CAN | CAN FD |
|---|---|---|
| 0–8 | 0–8 bytes | 0–8 bytes |
| 9 | 8 bytes | 12 bytes |
| 10 | 8 bytes | 16 bytes |
| 11 | 8 bytes | 20 bytes |
| 12 | 8 bytes | 24 bytes |
| 13 | 8 bytes | 32 bytes |
| 14 | 8 bytes | 48 bytes |
| 15 | 8 bytes | 64 bytes |

### 5.5 PLSIZE → Object Size

| PLSIZE | Payload | Total Object Size (header + payload) |
|---|---|---|
| `000` | 8 bytes | 16 bytes |
| `001` | 12 bytes | 20 bytes |
| `010` | 16 bytes | 24 bytes |
| `011` | 20 bytes | 28 bytes |
| `100` | 24 bytes | 32 bytes |
| `101` | 32 bytes | 40 bytes |
| `110` | 48 bytes | 56 bytes |
| `111` | 64 bytes | 72 bytes |

---

## 6. Operating Modes

The mode is requested by writing `CiCON.REQOP` and confirmed by reading `CiCON.OPMOD`.

| REQOP / OPMOD | Mode | Emulation Notes |
|---|---|---|
| `100` | **Configuration** | Only mode where timing/filter registers can be written. Must reflect in OPMOD after write. |
| `000` | **Normal CAN FD** | Full operation. Firmware transitions here after configuration. |
| `010` | **Listen Only** | No TX, no ACK. RX still works. No IRQ assertion for TX. |
| `111` | **Internal Loopback** | TX loops to RX without bus. **Must implement** — firmware self-test uses this. |
| `101` | **External Loopback** | TX goes to bus and is also received. |
| `001` | **Restricted** | Can RX, can ACK, cannot TX data frames. |
| `110` | **Sleep** | Rarely used in practice. Store and return. |

**Mode transition rule:** Reflect `REQOP` into `OPMOD` **immediately** on the write. Set `CiINT.MODIF=1`. Timing/filter registers are only writable while `OPMOD = 100` (Configuration).

---

## 7. Interrupt System

Each CAN FD instance has **one global IRQ line** to the MIPS interrupt controller.

| Instance | IRQ # | IFS Flag | IEC Enable | IPC Priority | IPC Subpri |
|---|---|---|---|---|---|
| CAN1 | 167 | `IFS5<7>` | `IEC5<7>` | `IPC41<28:26>` | `IPC41<25:24>` |
| CAN2 | 168 | `IFS5<8>` | `IEC5<8>` | `IPC42<4:2>` | `IPC42<1:0>` |
| CAN3 | 187 | `IFS5<27>` | `IEC5<27>` | `IPC46<28:26>` | `IPC46<25:24>` |
| CAN4 | 188 | `IFS5<28>` | `IEC5<28>` | `IPC47<4:2>` | `IPC47<1:0>` |

### Firmware ISR dispatch chain

1. Read `CiINT` — check `RXIF`, `TXIF`, `CERRIF`, `RXOVIF`, `MODIF`, `TEFIF`
2. If `RXIF=1`: read `CiRXIF` bitmask → find which FIFO n has bit set → handle RX
3. If `TXIF=1`: read `CiTXIF` bitmask → find which TX FIFO/TXQ completed → refill
4. Clear the specific FIFO interrupt via `CiFIFOSTAn.TXATIF` or `CiFIFOSTAn.RXIF`
5. Emulator must recompute `CiRXIF`, `CiTXIF`, `CiINT.RXIF`, `CiINT.TXIF` after each FIFO state change
6. Lower IRQ line when `CiINT` has no active enabled flags

---

## 8. Acceptance Filters

Up to 32 filters. Each filter has:

- **`CiFLTCONn`** — 4 filters packed per 32-bit register. Per-filter byte: bit 7 = `FLTEN` (enable), bits 4:0 = `FIFO[4:0]` (destination FIFO index).
- **`CiFLTOBJn`** — The ID to match (bit 30 = `XTD` for extended ID, bits 28:0 = ID).
- **`CiMASKn`** — Mask bits (1 = bit must match, 0 = don't care).

### Filter matching algorithm (emulator)

```c
int canfd_find_fifo(PIC32CANFDState *s, uint32_t id, bool xtd) {
    for (int n = 0; n < 32; n++) {
        // Each CiFLTCON register holds 4 filter bytes
        uint8_t fltcon = (s->fltcon[n / 4] >> ((n % 4) * 8)) & 0xFF;
        if (!(fltcon & 0x80)) continue;  // FLTEN = 0, skip

        uint32_t obj  = s->fltobj[n];
        uint32_t mask = s->mask[n];

        // Check ID type match
        bool obj_xtd = (obj >> 30) & 1;
        if (obj_xtd != xtd) continue;

        // Apply mask: (frame_id ^ filter_id) & mask == 0
        if ((id ^ (obj & 0x1FFFFFFF)) & (mask & 0x1FFFFFFF))
            continue;

        // Match — return destination FIFO
        return fltcon & 0x1F;
    }
    return -1;  // No match — frame discarded
}
```

---

## 9. QEMU Implementation

### 9.1 Device State Struct

```c
typedef struct PIC32CANFDState {
    SysBusDevice parent;

    /* Two MemoryRegions per instance */
    MemoryRegion sfr_mmio;   /* SFR registers: 0x1000 bytes */
    MemoryRegion msg_ram;    /* Message RAM: firmware-configured size */

    /* Key SFRs */
    uint32_t con;        /* CiCON */
    uint32_t nbtcfg;     /* CiNBTCFG */
    uint32_t dbtcfg;     /* CiDBTCFG */
    uint32_t tdc;        /* CiTDC */
    uint32_t tscon;      /* CiTSCON */
    uint32_t vec;        /* CiVEC */
    uint32_t cint;       /* CiINT */
    uint32_t rxif;       /* CiRXIF */
    uint32_t txif;       /* CiTXIF */
    uint32_t rxovif;     /* CiRXOVIF */
    uint32_t txreq;      /* CiTXREQ */
    uint32_t trec;       /* CiTREC */

    /* TXQ */
    uint32_t txqcon;
    uint32_t txqsta;
    uint32_t txqua;

    /* TEF */
    uint32_t tefcon;
    uint32_t tefsta;
    uint32_t tefua;

    /* Per-FIFO (31 FIFOs, index 1-31) */
    uint32_t fifocon[32];
    uint32_t fifosta[32];
    uint32_t fifoua[32];
    uint8_t  fifo_head[32];
    uint8_t  fifo_tail[32];
    uint8_t  fifo_count[32];

    /* Filters */
    uint32_t fltcon[8];   /* 4 filters per register × 8 = 32 filters */
    uint32_t fltobj[32];
    uint32_t mask[32];

    /* Message RAM backing store */
    uint8_t  *msg_ram_buf;
    uint32_t  msg_ram_size;

    /* IRQ line to INTC */
    qemu_irq irq;
} PIC32CANFDState;
```

### 9.2 MemoryRegion Setup

```c
static void pic32_canfd_realize(DeviceState *dev, Error **errp) {
    PIC32CANFDState *s = PIC32_CANFD(dev);

    /* SFR region */
    memory_region_init_io(&s->sfr_mmio, OBJECT(s),
                          &canfd_sfr_ops, s, "can-sfr", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->sfr_mmio);

    /* Message RAM — allocate maximum possible size upfront */
    /* Max: TXQ(32×72) + TEF(32×8) + 31 FIFOs(32×72 each) = ~74 KB */
    s->msg_ram_size = 74 * 1024;
    s->msg_ram_buf  = g_malloc0(s->msg_ram_size);

    memory_region_init_ram_ptr(&s->msg_ram, OBJECT(s),
                               "can-msgram", s->msg_ram_size,
                               s->msg_ram_buf);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->msg_ram);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
}
```

### 9.3 MMIO Read/Write Dispatcher

```c
static uint64_t canfd_sfr_read(void *opaque, hwaddr offset, unsigned size) {
    PIC32CANFDState *s = opaque;

    switch (offset) {
    case 0x000: return s->con;
    case 0x004: return s->nbtcfg;
    case 0x008: return s->dbtcfg;
    case 0x01C: return s->cint;
    case 0x020: return s->rxif;
    case 0x028: return s->txif;
    case 0x030: return s->rxovif;
    case 0x038: return s->txreq;
    case 0x058: return s->txqcon;
    case 0x05C: return s->txqsta;
    case 0x060: return s->txqua;
    /* FIFOs: offset 0x064 + (n-1)*0xC */
    default:
        if (offset >= 0x064 && offset < 0x1D0) {
            int idx = (offset - 0x064) / 0xC + 1;
            int sub = (offset - 0x064) % 0xC;
            if (idx < 32) {
                if (sub == 0) return s->fifocon[idx];
                if (sub == 4) return s->fifosta[idx];
                if (sub == 8) return s->fifoua[idx];
            }
        }
        /* Filters */
        if (offset >= 0x1D0 && offset < 0x1F0)
            return s->fltcon[(offset - 0x1D0) / 4];
        if (offset >= 0x1F0) {
            int fn = (offset - 0x1F0) / 8;
            return ((offset - 0x1F0) % 8 == 0) ? s->fltobj[fn] : s->mask[fn];
        }
        return 0;
    }
}
```

### 9.4 Mode Transition Handler

```c
case 0x000: { /* CiCON write */
    uint32_t new_con = val;  /* Apply SET/CLR/INV if needed */
    uint8_t reqop = (new_con >> 25) & 0x7;
    uint8_t old_opmod = (s->con >> 22) & 0x7;

    /* Reflect REQOP into OPMOD immediately */
    new_con = (new_con & ~(0x7u << 22)) | ((uint32_t)reqop << 22);

    /* Timing/filter registers only writable in Config mode (OPMOD=100) */
    /* Track mode change */
    bool mode_changed = (reqop != old_opmod);
    s->con = new_con;

    if (mode_changed) {
        s->cint |= (1u << 4);  /* Set MODIF */
        canfd_update_irq(s);
    }

    /* Handle ABAT (Abort All TX) */
    if (new_con & (1u << 28)) {
        canfd_abort_all_tx(s);
        s->con &= ~(1u << 28);  /* HW clears ABAT */
    }
    break;
}
```

### 9.5 FIFO UINC Protocol

This is the most critical interaction to get right. UINC advances the head/tail pointer after firmware reads (RX) or writes (TX) a message slot.

```c
/* Inside CiFIFOCONn write handler */
if (new_fifocon & (1u << 19)) {  /* UINC set */
    new_fifocon &= ~(1u << 19);  /* HW clears UINC immediately */
    bool is_tx = (s->fifocon[n] >> 20) & 1;
    int depth  = ((s->fifocon[n] >> 24) & 0x1F) + 1;  /* FSIZE+1 */

    if (is_tx) {
        /* Firmware finished writing TX object — advance tail */
        s->fifo_tail[n] = (s->fifo_tail[n] + 1) % depth;
        s->fifo_count[n]++;
        /* UA now points to next available TX write slot */
        s->fifoua[n] = canfd_fifo_ua(s, n, s->fifo_tail[n]);
    } else {
        /* Firmware finished reading RX object — advance head */
        s->fifo_head[n] = (s->fifo_head[n] + 1) % depth;
        s->fifo_count[n]--;
        /* UA now points to next unread RX slot */
        s->fifoua[n] = canfd_fifo_ua(s, n, s->fifo_head[n]);

        if (s->fifo_count[n] == 0)
            s->rxif &= ~(1u << n);  /* FIFO empty: clear RXIF bit */
        canfd_update_irq(s);
    }
}

/* Compute physical RAM address of a FIFO slot */
static uint32_t canfd_fifo_ua(PIC32CANFDState *s, int n, int slot) {
    uint32_t base     = canfd_fifo_ram_base(s, n);
    uint32_t obj_size = canfd_obj_size_bytes(s, n);  /* header(8) + payload */
    return base + (uint32_t)slot * obj_size;
}
```

### 9.6 TX Path

```c
static void canfd_process_tx(PIC32CANFDState *s, int fifo) {
    uint32_t ua = (fifo == 0) ? s->txqua : s->fifoua[fifo];
    uint32_t ram_offset = ua - msg_ram_phys_base;
    uint8_t *obj = s->msg_ram_buf + ram_offset;

    /* Parse T0/T1 header words */
    uint32_t t0  = *(uint32_t *)(obj + 0);
    uint32_t t1  = *(uint32_t *)(obj + 4);
    uint8_t  dlc = t1 & 0xF;
    bool     fdf = (t1 >> 7) & 1;
    bool     xtd = (t0 >> 30) & 1;
    uint32_t id  = xtd ? (t0 & 0x1FFFFFFFu)
                       : ((t0 >> 18) & 0x7FF);
    int len = dlc_to_bytes(dlc, fdf);
    uint8_t *data = obj + 8;

    if (canfd_is_loopback(s)) {
        /* Internal loopback: deliver to matching RX FIFO */
        canfd_rx_deliver(s, id, xtd, fdf, dlc, data, len, t1);
    } else {
        /* External: send to virtual CAN bus (SocketCAN / vcan) */
        canfd_send_to_bus(s, id, xtd, fdf, dlc, data, len);
    }

    /* TX complete */
    if (fifo == 0) {
        s->txqsta |= (1u << 2);   /* TXQNIF — queue not full */
        s->txqcon  &= ~(1u << 18); /* Clear TXREQ */
    } else {
        s->fifosta[fifo] |= (1u << 4);  /* TXATIF */
        s->fifocon[fifo] &= ~(1u << 18); /* Clear TXREQ */
        s->txif |= (1u << fifo);
    }
    canfd_update_irq(s);
}
```

### 9.7 RX Deliver Path

Called when a frame arrives from the virtual bus or loopback.

```c
static void canfd_rx_deliver(PIC32CANFDState *s,
                              uint32_t id, bool xtd, bool fdf,
                              uint8_t dlc, uint8_t *data, int len,
                              uint32_t tx_t1_seq)
{
    /* 1. Find matching filter */
    int dest_fifo = canfd_find_fifo(s, id, xtd);
    if (dest_fifo < 0) return;  /* No filter match — discard */

    /* 2. Check for overflow */
    int depth = ((s->fifocon[dest_fifo] >> 24) & 0x1F) + 1;
    if (s->fifo_count[dest_fifo] >= depth) {
        s->rxovif |= (1u << dest_fifo);
        s->cint   |= (1u << 11);  /* RXOVIF in CiINT */
        canfd_update_irq(s);
        return;
    }

    /* 3. Write message object to tail slot */
    uint32_t ua = canfd_fifo_ua(s, dest_fifo, s->fifo_tail[dest_fifo]);
    uint8_t *obj = s->msg_ram_buf + (ua - msg_ram_phys_base);

    /* Build R0 */
    uint32_t r0 = xtd ? (id | (1u << 30)) : (id << 18);
    *(uint32_t *)(obj + 0) = r0;

    /* Build R1 — fill FILHIT and RXTS */
    uint32_t r1 = dlc
                | (xtd  ? (1u << 4) : 0)
                | (fdf  ? (1u << 7) : 0)
                | ((uint32_t)(dest_fifo & 0x1F) << 11)  /* FILHIT */
                | ((s->tbc & 0xFFFF) << 16);             /* RXTS */
    *(uint32_t *)(obj + 4) = r1;

    /* Copy payload */
    memcpy(obj + 8, data, len);

    /* 4. Advance tail */
    s->fifo_tail[dest_fifo] = (s->fifo_tail[dest_fifo] + 1) % depth;
    s->fifo_count[dest_fifo]++;

    /* 5. Set interrupt flags */
    s->fifosta[dest_fifo] |= (1u << 0);  /* RXIF in FIFO status */
    s->rxif |= (1u << dest_fifo);

    canfd_update_irq(s);
}
```

### 9.8 IRQ Update Logic

```c
static void canfd_update_irq(PIC32CANFDState *s) {
    bool fire = false;

    /* RXIF: any RX FIFO has data and RXIE is enabled */
    if ((s->cint >> 18) & 1) {  /* RXIE */
        if (s->rxif) { s->cint |=  (1u << 1); fire = true; }
        else          { s->cint &= ~(1u << 1); }
    }

    /* TXIF: any TX FIFO completed and TXIE is enabled */
    if ((s->cint >> 17) & 1) {  /* TXIE */
        if (s->txif) { s->cint |=  (1u << 0); fire = true; }
        else          { s->cint &= ~(1u << 0); }
    }

    /* MODIF: mode changed and MODIE is enabled */
    if (((s->cint >> 20) & 1) && ((s->cint >> 4) & 1))
        fire = true;

    /* RXOVIF: any overflow and RXOVIE enabled */
    if (((s->cint >> 27) & 1) && ((s->cint >> 11) & 1))
        fire = true;

    qemu_set_irq(s->irq, fire);
}
```

### 9.9 Virtual CAN Bus — Host Integration

Each emulated CAN FD instance exposes a **virtual CAN interface on the host** using QEMU's built-in SocketCAN backend (`hw/net/can/`). When the emulated controller is not in loopback mode, frames flow between guest firmware and real host CAN tooling.

#### Host-side setup

```bash
# Load the vcan kernel module (once)
sudo modprobe vcan

# Create a virtual CAN interface for each instance
sudo ip link add dev vcan0 type vcan   # CAN1
sudo ip link add dev vcan1 type vcan   # CAN2
sudo ip link set up vcan0
sudo ip link set up vcan1

# Monitor traffic on vcan0
candump vcan0

# Inject a frame into the guest from the host
cansend vcan0 123#DEADBEEF
```

#### QEMU launch options

Pass `-object can-bus,id=canbus0` and `-device pic32mk-canfd,canbus=canbus0,instance=0` (or equivalent board properties) to connect CAN1 to `vcan0`:

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/freertos/hello_freertos.bin \
    -object can-bus,id=canbus0 \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors
```

> The exact `-device` / `-object` flags depend on how the board wires the `can-bus` object to the peripheral instance. Check `hw/mips/pic32mk.c` for the current wiring.

#### Data flow (non-loopback mode)

```
Guest firmware                 QEMU emulator               Host kernel
─────────────                  ─────────────               ───────────
Write TX object  ──TXREQ──►  canfd_process_tx()
                              canfd_send_to_bus()  ──────►  vcan0  ──►  candump / cansend
                                                   ◄──────  vcan0  ◄──  cansend
                              canfd_rx_deliver()
Read from RX FIFO ◄──IRQ──   canfd_update_irq()
```

#### CAN FD frames vs. classic CAN

QEMU's SocketCAN backend uses `struct canfd_frame` (64-byte payload) when `FDF=1` and `struct can_frame` (8-byte payload) for classic frames. The `canfd_send_to_bus()` helper must set `CANFD_FDF` in `flags` for FD frames so the host interface sends the right frame type.

```c
static void canfd_send_to_bus(PIC32CANFDState *s,
                               uint32_t id, bool xtd, bool fdf,
                               uint8_t dlc, uint8_t *data, int len)
{
    qemu_can_frame frame = {};
    frame.can_id  = xtd ? (id | CAN_EFF_FLAG) : id;
    frame.can_dlc = dlc;
    frame.flags   = fdf ? CANFD_FDF : 0;
    memcpy(frame.data, data, len);
    can_bus_client_send(&s->bus_client, &frame, 1);
}
```

---

## 10. TX Sequence (Firmware Perspective)

This is what firmware does to transmit a frame via the TX Queue. Your emulator must handle every step.

1. Set `CiCON.REQOP = 100` → Configuration mode
2. Poll `CiCON.OPMOD` until = `100`
3. Configure `CiNBTCFG`, `CiDBTCFG`, `CiTDC` (baud rates and delay compensation)
4. Configure `CiTXQCON`: set `PLSIZE` (payload size), `FSIZE` (depth), `TXPRI` (priority)
5. Set `CiCON.TXQEN = 1`, then `CiCON.REQOP = 000` → Normal mode
6. Poll `CiCON.OPMOD` until = `000`
7. Poll `CiTXQSTA.TXQNIF = 1` (queue slot available)
8. Read `CiTXQUA` — this is the Message RAM address for the next TX object
9. Write T0/T1 header words and payload to the address in `CiTXQUA`
10. Set `CiTXQCON.UINC = 1` — advance the write pointer (hardware clears this bit)
11. Set `CiTXQCON.TXREQ = 1` — request transmission
12. **Emulator:** process message from Message RAM, clear TXREQ, set TXATIF, update `CiTXIF`, call `canfd_update_irq()`

---

## 11. RX Sequence (Firmware Perspective)

1. Configure FIFO n as RX: `CiFIFOCONn.TXEN = 0`, set `PLSIZE`, `FSIZE`
2. Configure filter k: write `CiFLTOBJk` (ID to match), `CiMASKk` (mask), set `CiFLTCONk.FIFO = n`, set `CiFLTCONk.FLTEN = 1`
3. Enable RX IRQ: `CiINT.RXIE = 1`, `CiFIFOCONn.TFNRFNIE = 1`
4. Set `CiCON.REQOP = 000` → Normal mode
5. **On IRQ:** Read `CiINT.RXIF` → read `CiRXIF` bitmask → find FIFO n with bit set
6. Read `CiFIFOUAn` — Message RAM address of next unread RX object
7. Read R0/R1 header words and payload from the address in `CiFIFOUAn`
8. Set `CiFIFOCONn.UINC = 1` — advance the read pointer (hardware clears immediately)
9. Clear `CiFIFOSTAn.RXIF` — acknowledge this FIFO's interrupt
10. **Emulator:** update `CiRXIF`, re-evaluate `CiINT.RXIF`, lower IRQ if all clear

---

## 12. Key Emulation Pitfalls

### FIFO pointer protocol (most common source of bugs)

The `UINC` bit advances head/tail pointers and must be cleared by the emulator immediately after it is set by firmware. If you don't do this:
- Firmware gets stuck polling for UINC to clear (it writes 1 expecting to see 0 on next read).
- The UA register never advances, causing firmware to overwrite the same slot repeatedly.

### Mode-gated register writes

Timing registers (`CiNBTCFG`, `CiDBTCFG`, `CiTDC`) and filter configuration registers can only be written while `OPMOD = 100` (Configuration mode). Writes in other modes must be silently ignored.

### Internal loopback mode

When `OPMOD = 111`, TX objects must be delivered directly to the RX path through the filter system — without going to any external bus. Firmware uses this for self-tests. If loopback doesn't work, any firmware that validates the CAN controller on boot will fail.

### `CiINT` is a two-level aggregator

Firmware does not directly poll FIFO status. It reads `CiINT` first, then branches. The `RXIF` and `TXIF` flags in `CiINT` are **derived** from `CiRXIF`/`CiTXIF` bitmasks. You must recompute `CiINT.RXIF` and `CiINT.TXIF` every time any FIFO state changes.

### Reset state of `CiCON`

`CiCON` resets to `0x04980760`, which means `OPMOD = 100` (Configuration mode) and `REQOP = 100`. Do not reset to 0 — firmware expects to start in Configuration mode.

### Message RAM addressing

The `UA` register contains an **absolute physical RAM address**, not an offset. Firmware adds nothing to it; it casts `CiTXQUA` or `CiFIFOUAn` directly to a pointer. The emulator must compute UA values as physical addresses within the Message RAM region.

### ABAT (abort all TX)

When `CiCON.ABAT` is written 1, all pending TX requests across all FIFOs and TXQ must be cancelled. Set `TXATIF` in each active FIFO status, clear all `CiTXREQ` bits, then clear the `ABAT` bit in `CiCON`.

### TX IDE bit is in T1, not T0 — 29-bit IDs transmitted as 11-bit (2026-03-27)

**Symptom:** Extended CAN frames sent by firmware appear on the SocketCAN bus (vcan) with a
truncated 11-bit ID instead of the full 29-bit ID. For example, ID `0x18FE0909` shows up as `0x63F`.

**Root cause:** The TX message object layout has an asymmetry between T0 and T1:

| Word | Field | Bits |
|------|-------|------|
| T0   | SID[10:0] | [10:0] |
| T0   | EID[17:0] | [28:11] |
| T0   | *(no IDE here)* | — |
| T1   | DLC | [3:0] |
| **T1** | **IDE** | **[4]** ← extended frame flag |
| T1   | RTR, BRS, FDF, ESI | [5:8] |

The plib (`CAN4_MessageTransmit`) correctly sets `t1 |= CANFD_MSG_IDE_MASK` (`0x10`) to signal an
extended frame. The QEMU emulator was incorrectly reading the IDE bit from `t0[30]`:

```c
/* WRONG — t0 bit 30 is always 0; CANFD_MSG_EID_MASK only covers bits [28:0] */
bool xtd = (t0 >> 30) & 1u;
```

Because `t0[30]` is never set by the plib, QEMU always treated TX frames as standard 11-bit, and
decoded only the SID portion: `t0 & 0x7FF`.

For `id = 0x18FE0909`, the SID portion packed into `t0[10:0]` is:
```
(0x18FE0909 & CANFD_MSG_TX_EXT_SID_MASK) >> 18
= (0x18FE0909 & 0x1FFC0000) >> 18
= 0x18FC0000 >> 18
= 0x63F          ← exactly the wrong ID seen on the bus
```

**Fix applied** (`hw/mips/pic32mk_canfd.c`):

```c
/* Before (wrong): */
bool xtd = (t0 >> 30) & 1u;

/* After (correct): IDE is in T1[4] per DS60001507 §3.1 */
bool xtd = (t1 >> 4) & 1u;
```

**RX path is unaffected.** The RX message object (R0/R1) does carry `EXIDE` in both `r0[30]` and
`r1[4]`. QEMU sets both fields, and the plib ISR reads `r1[4]` (`rxMessage->r1 & CANFD_MSG_IDE_MASK`),
so the RX decode was already correct.

**Where to look if this symptom reappears:** `canfd_process_tx()` in `hw/mips/pic32mk_canfd.c`, at
the point where `t0` and `t1` are read from the Message RAM TX slot.

---

## 13. Useful References

| Resource | Use |
|---|---|
| **DS60001519E** (this datasheet) | Base addresses, interrupt vectors, SFR table, instance count |
| **DS60001507** (PIC32 CAN FD Reference Manual) | Full register bit descriptions, message object format, FIFO state machines |
| **Linux `mcp251xfd` driver** (`drivers/net/can/spi/mcp251xfd/`) | Best software reference for the same IP; shows filter programming, FIFO management, and loopback |
| **QEMU `hw/mips/mips_malta.c`** | Reference for how to add a board with multiple SysBus devices |
| **QEMU `hw/net/can/`** | CAN bus abstraction layer used in QEMU; `can_core.c` handles virtual bus framing |
| **ISO 11898-1:2015** | CAN FD frame format specification |
| **SocketCAN** (`include/uapi/linux/can.h`) | `canfd_frame` struct useful as a reference for the 64-byte payload format |
