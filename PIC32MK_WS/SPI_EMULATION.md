# PIC32MK SPI Peripheral Emulation

> **Datasheet:** DS60001519E, §23 — Serial Peripheral Interface (SPI)
> **Device:** PIC32MK1024MCM100
> **Phase:** 2B — Full register model with FIFO, chardev socket link, and interrupt-driven Harmony PLIB support

---

## Table of Contents

1. [Overview](#1-overview)
2. [Hardware Register Map](#2-hardware-register-map)
   - 2.1 [Instance Addresses](#21-instance-addresses)
   - 2.2 [Register Offsets Within Each Instance](#22-register-offsets-within-each-instance)
   - 2.3 [CLR/SET/INV Convention](#23-clrsetinv-convention)
3. [SPIxCON — Control Register](#3-spixcon--control-register)
4. [SPIxSTAT — Status Register](#4-spixstat--status-register)
5. [SPIxCON2 — Control Register 2](#5-spixcon2--control-register-2)
6. [Enhanced Buffer (FIFO) Mode](#6-enhanced-buffer-fifo-mode)
7. [Interrupt System](#7-interrupt-system)
   - 7.1 [EVIC IRQ Numbers](#71-evic-irq-numbers)
   - 7.2 [IFS/IEC Bit Mapping](#72-ifsiec-bit-mapping)
   - 7.3 [SRXISEL / STXISEL Interrupt Condition Logic](#73-srxisel--stxisel-interrupt-condition-logic)
8. [Chardev Socket Protocol](#8-chardev-socket-protocol)
   - 8.1 [Frame Format](#81-frame-format)
   - 8.2 [Flag Bits](#82-flag-bits)
   - 8.3 [Exchange Sequence](#83-exchange-sequence)
   - 8.4 [Python Host Example](#84-python-host-example)
9. [QEMU Implementation](#9-qemu-implementation)
   - 9.1 [Source Files](#91-source-files)
   - 9.2 [Device State Struct](#92-device-state-struct)
   - 9.3 [MemoryRegion & MMIO Ops](#93-memoryregion--mmio-ops)
   - 9.4 [FIFO Operations](#94-fifo-operations)
   - 9.5 [IRQ Update Logic](#95-irq-update-logic)
   - 9.6 [Chardev Frame Handling](#96-chardev-frame-handling)
   - 9.7 [Master TX Path](#97-master-tx-path)
   - 9.8 [Slave RX Path](#98-slave-rx-path)
   - 9.9 [Board Wiring (pic32mk.c)](#99-board-wiring-pic32mkc)
10. [Firmware Integration](#10-firmware-integration)
    - 10.1 [Harmony PLIB Files](#101-harmony-plib-files)
    - 10.2 [xc.h Definitions](#102-xch-definitions)
    - 10.3 [Interrupt Dispatch (crt0.S)](#103-interrupt-dispatch-crt0s)
    - 10.4 [ISR Wrappers (port_asm_patched.S)](#104-isr-wrappers-port_asm_patcheds)
    - 10.5 [Demo Task (main.c)](#105-demo-task-mainc)
11. [Running the SPI Demo](#11-running-the-spi-demo)
    - 11.1 [QEMU Launch Command](#111-qemu-launch-command)
    - 11.2 [Sending Data from Host](#112-sending-data-from-host)
    - 11.3 [Expected Output](#113-expected-output)
12. [Known Issues & Pitfalls](#12-known-issues--pitfalls)
13. [References](#13-references)

---

## 1. Overview

The PIC32MK1024MCM100 has **6 SPI instances** (SPI1–SPI6). Each supports:

| Feature | Value |
|---------|-------|
| Modes | Master / Slave |
| Data Width | 8-bit, 16-bit, 32-bit |
| Enhanced Buffer | 8-deep FIFO (when ENHBUF=1) |
| Interrupts | 3 per instance: RX, TX, Fault (overflow) |
| Chip Select | Hardware SS pin (SSEN/MSSEN) or software CS |
| Clock | Configurable polarity (CKP) and edge (CKE) |

The QEMU emulation provides:

- Full SPIxCON, SPIxSTAT, SPIxBUF, SPIxBRG, SPIxCON2 register model
- 8-deep TX/RX FIFOs (enhanced buffer mode)
- SRXISEL/STXISEL-aware interrupt generation
- Chardev socket for external SPI master/slave simulation
- Loopback when no chardev is connected (master mode)
- Compatibility with unmodified Microchip Harmony v3 SPI PLIB

---

## 2. Hardware Register Map

### 2.1 Instance Addresses

SPI1–SPI2 are in the peripheral-1 SFR block (base `0xBF800000`).
SPI3–SPI6 are in the peripheral-2 SFR block (base `0xBF800000`).

| Instance | SFR Offset | KSEG1 Base Address |
|----------|------------|--------------------|
| SPI1 | `0x021800` | `0xBF821800` |
| SPI2 | `0x021A00` | `0xBF821A00` |
| SPI3 | `0x040800` | `0xBF840800` |
| SPI4 | `0x040A00` | `0xBF840A00` |
| SPI5 | `0x040C00` | `0xBF840C00` |
| SPI6 | `0x040E00` | `0xBF840E00` |

Each instance occupies a **0x200** (512 bytes) block.

### 2.2 Register Offsets Within Each Instance

| Register | Offset | Description |
|----------|--------|-------------|
| SPIxCON | `0x00` | Control register |
| SPIxSTAT | `0x10` | Status register |
| SPIxBUF | `0x20` | Data buffer (TX write / RX read) |
| SPIxBRG | `0x30` | Baud rate generator |
| SPIxCON2 | `0x40` | Control register 2 |

### 2.3 CLR/SET/INV Convention

PIC32MK uses the standard Microchip SFR convention:

| Access | Offset | Operation |
|--------|--------|-----------|
| Direct | `+0x0` | Write replaces register |
| CLR | `+0x4` | Bits in written value are cleared |
| SET | `+0x8` | Bits in written value are set |
| INV | `+0xC` | Bits in written value are toggled |

Example: `SPI5CONSET = _SPI5CON_ON_MASK` sets the ON bit without disturbing others.

---

## 3. SPIxCON — Control Register

Key bit fields used by the emulation:

| Bit(s) | Name | Description |
|--------|------|-------------|
| 0–1 | SRXISEL | RX interrupt condition (see §7.3) |
| 2–3 | STXISEL | TX interrupt condition (see §7.3) |
| 5 | MSTEN | Master mode enable (0=slave, 1=master) |
| 6 | CKP | Clock polarity |
| 7 | SSEN | Slave select enable |
| 8 | CKE | Clock edge select |
| 10 | MODE16 | 16-bit data width |
| 11 | MODE32 | 32-bit data width |
| 15 | ON | Module enable |
| 16 | ENHBUF | Enhanced buffer (FIFO) enable |
| 23 | MCLKSEL | Master clock select |
| 28 | MSSEN | Master SS output enable |

Reset value: `0x00000000`

When ON is cleared, the FIFOs are flushed and STAT is reset to
`SPITBE | SPIRBE | SRMT`.

---

## 4. SPIxSTAT — Status Register

| Bit | Name | Description |
|-----|------|-------------|
| 0 | SPIRBF | RX buffer full |
| 1 | SPITBF | TX buffer full |
| 3 | SPITBE | TX buffer empty |
| 5 | SPIRBE | RX buffer empty |
| 6 | SPIROV | RX overflow (sticky, clear via SPIxSTATCLR) |
| 11 | SRMT | Shift register empty (all TX done) |

The emulation auto-computes SPIRBF, SPITBF, SPITBE, SPIRBE, and SRMT from
FIFO counts. SPIROV is set on overflow and must be explicitly cleared.

---

## 5. SPIxCON2 — Control Register 2

| Bit | Name | Description |
|-----|------|-------------|
| 0 | SPIROVEN | Generate interrupt on receiver overflow |

Other CON2 bits (APTS, AUDEN, etc.) are stored but not functionally emulated.

---

## 6. Enhanced Buffer (FIFO) Mode

When `ENHBUF=1` (SPIxCON bit 16), each direction has an **8-deep FIFO**.
When `ENHBUF=0`, only a single-entry buffer is available (depth = 1).

The TX FIFO is populated by writing to SPIxBUF. The RX FIFO is drained by
reading from SPIxBUF. Multi-byte data widths (MODE16, MODE32) consume
2 or 4 FIFO slots per transfer.

---

## 7. Interrupt System

### 7.1 EVIC IRQ Numbers

From `include/hw/mips/pic32mk.h` and Table 8-3 of DS60001519E:

| Instance | Fault IRQ | RX IRQ | TX IRQ |
|----------|-----------|--------|--------|
| SPI1 | 35 | 36 | 37 |
| SPI2 | 53 | 54 | 55 |
| SPI3 | 154 | 155 | 156 |
| SPI4 | 166 | 167 | 168 |
| SPI5 | 169 | 170 | 171 |
| SPI6 | 172 | 173 | 174 |

### 7.2 IFS/IEC Bit Mapping

SPI5 example (used in the demo firmware):

| Interrupt | EVIC Source | IFS/IEC Register | Bit | Mask |
|-----------|-------------|-------------------|-----|------|
| SPI5 Fault | 169 | IFS5/IEC5 | 9 | `0x00000200` |
| SPI5 RX | 170 | IFS5/IEC5 | 10 | `0x00000400` |
| SPI5 TX | 171 | IFS5/IEC5 | 11 | `0x00000800` |

**Critical note:** These are bits 9–11 (lower 16 bits), so `andi` must be used
in assembly dispatch — **not** `lui` which loads into the upper 16 bits.

### 7.3 SRXISEL / STXISEL Interrupt Condition Logic

The QEMU model respects the SRXISEL and STXISEL fields to determine when
interrupt lines are asserted:

**SRXISEL (SPIxCON[1:0]) — RX interrupt fires when:**

| Value | Condition |
|-------|-----------|
| 0 | RX FIFO is empty (edge: last byte read) |
| 1 | RX FIFO is not empty (any data available) |
| 2 | RX FIFO is half full or more |
| 3 | RX FIFO is completely full |

**STXISEL (SPIxCON[3:2]) — TX interrupt fires when:**

| Value | Condition |
|-------|-----------|
| 0 | TX FIFO is empty (all transmitted) |
| 1 | TX FIFO is empty (same as 0) |
| 2 | TX FIFO is half empty or more |
| 3 | TX FIFO is not full (space available) |

The Harmony slave PLIB typically uses `SRXISEL=1` (not empty) and `STXISEL=3`
(not full), which means:
- **RX interrupt fires as soon as any byte arrives in the RX FIFO**
- **TX interrupt fires as long as the TX FIFO has room**

---

## 8. Chardev Socket Protocol

### 8.1 Frame Format

The SPI emulation uses a simple binary framing protocol over a QEMU chardev
(typically a TCP socket):

```
Byte 0:    flags     (1 byte)
Byte 1:    length    (1 byte, 0–255)
Byte 2..:  payload   (length bytes)
```

Total frame size: `2 + length` bytes.

### 8.2 Flag Bits

| Bit | Name | Description |
|-----|------|-------------|
| 0 | CS_ASSERT | Assert chip select (start of transfer) |
| 1 | CS_DEASSERT | Deassert chip select (end of transfer) |

Typical usage: a single frame with both flags set (flags = `0x03`) represents a
complete SPI transaction.

### 8.3 Exchange Sequence

SPI is full-duplex: for every byte the master sends (MOSI), the slave returns
a byte (MISO). The protocol reflects this:

1. **Host → QEMU:** Frame with `[flags=0x03][len=N][payload N bytes]`
2. For each payload byte:
   - Pushed into the SPI RX FIFO (firmware reads via SPIxBUF)
   - One byte popped from the TX FIFO (pre-loaded by firmware) as the MISO response
3. **QEMU → Host:** Response frame `[flags=0x00][len=N][response N bytes]`

If the TX FIFO is empty, the response bytes default to `0xFF`.

### 8.4 Python Host Example

```python
#!/usr/bin/env python3
"""Send HELLO to SPI5 on QEMU and receive the slave response."""

import socket
import struct
import sys

SOCK_PATH = "/tmp/qemu-spi5.sock"  # or use TCP host:port

def spi_exchange(sock, data: bytes) -> bytes:
    """Send an SPI frame and receive the response."""
    flags = 0x03  # CS_ASSERT | CS_DEASSERT
    frame = struct.pack("BB", flags, len(data)) + data
    sock.sendall(frame)

    # Read response header (2 bytes)
    hdr = b""
    while len(hdr) < 2:
        hdr += sock.recv(2 - len(hdr))

    resp_flags, resp_len = struct.unpack("BB", hdr)

    # Read response payload
    payload = b""
    while len(payload) < resp_len:
        payload += sock.recv(resp_len - len(payload))

    return payload

def main():
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(SOCK_PATH)

    response = spi_exchange(sock, b"HELLO")
    print(f"MISO response: {response!r}")
    # Expected: b'WORLD' (if firmware pre-loaded reply)

    sock.close()

if __name__ == "__main__":
    main()
```

For TCP instead of Unix socket:
```python
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("localhost", 12345))
```

---

## 9. QEMU Implementation

### 9.1 Source Files

| File | Purpose |
|------|---------|
| `hw/mips/pic32mk_spi.c` | SPI device model (registers, FIFOs, chardev, IRQs) |
| `hw/mips/pic32mk.c` | Board wiring: creates SPI1–6, maps to SFR, connects IRQs |
| `include/hw/mips/pic32mk.h` | SFR offsets, IRQ numbers, register offset constants |

### 9.2 Device State Struct

```c
struct PIC32MKSpiState {
    SysBusDevice parent_obj;
    MemoryRegion mr;

    uint32_t con;       /* SPIxCON */
    uint32_t stat;      /* SPIxSTAT */
    uint32_t buf;       /* SPIxBUF (unused — FIFO handles data) */
    uint32_t brg;       /* SPIxBRG */
    uint32_t con2;      /* SPIxCON2 */

    uint8_t index;      /* SPI instance 1..6 */

    /* 8-deep TX/RX circular FIFOs */
    uint8_t tx_fifo[8], rx_fifo[8];
    uint8_t tx_head, tx_tail, tx_count;
    uint8_t rx_head, rx_tail, rx_count;

    /* Chardev frame reassembly buffer */
    uint8_t frame_buf[260];
    uint16_t frame_pos, frame_len;

    bool cs_active;

    CharFrontend chr;       /* Socket link */

    qemu_irq irq_rx;       /* RX interrupt line to EVIC */
    qemu_irq irq_tx;       /* TX interrupt line to EVIC */
    qemu_irq irq_err;      /* Fault/overflow line to EVIC */
};
```

### 9.3 MemoryRegion & MMIO Ops

- Block size: 0x200 bytes
- Access: 32-bit only (`min_access_size=4, max_access_size=4`)
- Little-endian
- Supports CLR/SET/INV via `apply_sci()` helper

Read path:
- SPIxBUF read → pops from RX FIFO (respects MODE16/MODE32 for multi-byte)
- SPIxSTAT read → calls `spi_update_stat()` + `spi_update_irq()`
- Other registers → direct return

Write path:
- SPIxBUF write → pushes to TX FIFO; in master mode, sends frame and pushes
  loopback/response into RX FIFO
- SPIxCON write → on ON→0 transition, flushes all FIFOs and resets STAT
- Other registers → apply CLR/SET/INV, then update stat/irq

### 9.4 FIFO Operations

| Function | Description |
|----------|-------------|
| `spi_rx_push(s, val)` | Push byte into RX FIFO; sets SPIROV on overflow |
| `spi_rx_pop(s, &val)` | Pop byte from RX FIFO; returns false if empty |
| `spi_tx_push(s, val)` | Push byte into TX FIFO; silently drops on full |
| `spi_tx_pop(s, &val)` | Pop byte from TX FIFO; returns 0xFF if empty |
| `spi_fifo_depth(s)` | Returns 8 if ENHBUF=1, else 1 |

### 9.5 IRQ Update Logic

`spi_update_irq()` is called after every register access and FIFO operation:

1. If `ON=0`: deassert all three IRQ lines → return
2. Evaluate SRXISEL field against RX FIFO count → set/clear `irq_rx`
3. Evaluate STXISEL field against TX FIFO count → set/clear `irq_tx`
4. Check SPIROV → set/clear `irq_err`

```c
int srxisel = (con >> 0) & 3;
switch (srxisel) {
    case 0:  rx_pending = (rx_count == 0); break;
    case 1:  rx_pending = (rx_count > 0);  break;   /* not empty */
    case 2:  rx_pending = (rx_count >= depth/2); break;
    case 3:  rx_pending = (rx_count >= depth); break;
}

int stxisel = (con >> 2) & 3;
switch (stxisel) {
    case 0:  tx_pending = (tx_count == 0); break;
    case 1:  tx_pending = (tx_count == 0); break;
    case 2:  tx_pending = (tx_count <= depth/2); break;
    case 3:  tx_pending = (tx_count < depth); break; /* not full */
}
```

The interrupt lines connect to the EVIC which manages IFSx/IECx bits and CPU
interrupt delivery.

### 9.6 Chardev Frame Handling

`spi_chr_receive()` reassembles incoming chardev data into frames:

1. Buffer bytes until 2-byte header complete (flags, length)
2. Continue buffering until full payload received
3. Call `spi_handle_frame()` with flags + payload
4. Reset buffer for next frame

### 9.7 Master TX Path

When firmware writes to SPIxBUF with `MSTEN=1`:

1. Byte(s) pushed to TX FIFO
2. If chardev connected: send frame with `CS_ASSERT|CS_DEASSERT`
3. If chardev **not** connected: loopback (TX data pushed into RX FIFO)
4. Pop TX entry
5. Update stat + IRQ

### 9.8 Slave RX Path

When `MSTEN=0` (slave mode) and chardev receives a frame:

1. `spi_handle_frame()` receives flags + payload from remote master
2. Each payload byte pushed into RX FIFO via `spi_rx_push()`
3. Each payload byte pops a corresponding TX FIFO entry as the MISO response
4. Response frame sent back to chardev (MISO data)
5. `spi_update_stat()` and `spi_update_irq()` called
6. EVIC sets IFSx.SPIxRXIF → firmware ISR runs → ISR drains FIFO

### 9.9 Board Wiring (pic32mk.c)

```c
static void pic32mk_spi_create(PIC32MKState *s, int index, hwaddr sfr_offset,
                               int irq_rx, int irq_tx, int irq_err)
{
    DeviceState *dev = qdev_new(TYPE_PIC32MK_SPI);
    qdev_prop_set_uint8(dev, "spi-index", (uint8_t)index);

    // Look up chardev "spi<N>" (e.g. "spi5")
    Chardev *chr = qemu_chr_find(chr_name);
    if (chr) qdev_prop_set_chr(dev, "chardev", chr);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    // Map into SFR window at priority 1 (overrides catch-all)
    memory_region_add_subregion_overlap(&s->sfr, sfr_offset, mr, 1);

    // Wire 3 IRQ lines to EVIC inputs
    sysbus_connect_irq(dev, 0, qdev_get_gpio_in(s->evic, irq_rx));
    sysbus_connect_irq(dev, 1, qdev_get_gpio_in(s->evic, irq_tx));
    sysbus_connect_irq(dev, 2, qdev_get_gpio_in(s->evic, irq_err));
}
```

---

## 10. Firmware Integration

### 10.1 Harmony PLIB Files

The demo uses **unmodified** Microchip Harmony v3 SPI Slave PLIB:

| File | Description |
|------|-------------|
| `plib_spi5_slave.c` | SPI5 slave driver (init, read, write, ISR handlers) |
| `plib_spi5_slave.h` | Public API header |
| `plib_spi_slave_common.h` | Common types (SPI_SLAVE_OBJECT, SPI_SLAVE_CALLBACK) |

Location: `tests/pic32mk_rtos_demo/firmware/src/config/default/peripheral/spi/`

Key Harmony PLIB behavior:
- `SPI5_Initialize()`: Sets `SRXISEL=1` (not empty), `STXISEL=3` (not full),
  `ENHBUF=1`, slave mode, enables RX + Fault interrupts
- `SPI5_RX_InterruptHandler()`: Drains RX FIFO → internal 256-byte buffer
- `SPI5_TX_InterruptHandler()`: Refills TX FIFO from internal write buffer
- `SPI5_Read()`: Copies from internal RX buffer to caller's buffer
- `SPI5_Write()`: Fills TX FIFO + internal write buffer, enables TX interrupt
- `SPI5_ReadCountGet()`: Returns number of bytes in internal RX buffer

### 10.2 xc.h Definitions

SPI5 register addresses and bit field definitions in
`tests/pic32mk_rtos_demo/firmware/src/config/default/xc.h`:

```c
/* SPI5 registers (base 0xBF840C00) */
#define SPI5CON         (*(volatile uint32_t *)(0xBF840C00u))
#define SPI5CONCLR      (*(volatile uint32_t *)(0xBF840C04u))
#define SPI5CONSET      (*(volatile uint32_t *)(0xBF840C08u))
#define SPI5CONINV      (*(volatile uint32_t *)(0xBF840C0Cu))

#define SPI5STAT        (*(volatile uint32_t *)(0xBF840C10u))
#define SPI5STATCLR     (*(volatile uint32_t *)(0xBF840C14u))
#define SPI5STATSET     (*(volatile uint32_t *)(0xBF840C18u))
#define SPI5STATINV     (*(volatile uint32_t *)(0xBF840C1Cu))

#define SPI5BUF         (*(volatile uint32_t *)(0xBF840C20u))
#define SPI5BRG         (*(volatile uint32_t *)(0xBF840C30u))

#define SPI5CON2        (*(volatile uint32_t *)(0xBF840C40u))
#define SPI5CON2CLR     (*(volatile uint32_t *)(0xBF840C44u))
#define SPI5CON2SET     (*(volatile uint32_t *)(0xBF840C48u))
#define SPI5CON2INV     (*(volatile uint32_t *)(0xBF840C4Cu))

/* SPIxCON bit positions */
#define _SPI5CON_SRXISEL_POSITION  0u
#define _SPI5CON_STXISEL_POSITION  2u
#define _SPI5CON_MSTEN_POSITION    5u
#define _SPI5CON_CKP_POSITION      6u
#define _SPI5CON_SSEN_POSITION     7u
#define _SPI5CON_CKE_POSITION      8u
#define _SPI5CON_MODE16_POSITION   10u
#define _SPI5CON_MODE32_POSITION   11u
#define _SPI5CON_ENHBUF_POSITION   16u
#define _SPI5CON_ON_MASK           (1u << 15u)

/* SPIxSTAT masks */
#define _SPI5STAT_SPITBF_MASK      (1u << 1u)
#define _SPI5STAT_SPIRBE_MASK      (1u << 5u)
#define _SPI5STAT_SPIROV_MASK      (1u << 6u)
#define _SPI5STAT_SRMT_MASK        (1u << 11u)

/* SPI5CON2 masks */
#define _SPI5CON2_SPIROVEN_MASK    (1u << 0u)

/* SPI5 interrupt masks (IFS5/IEC5 bits 9..11) */
#define _IFS5_SPI5EIF_MASK         (1u << 9u)   /* 0x200 */
#define _IFS5_SPI5RXIF_MASK        (1u << 10u)  /* 0x400 */
#define _IFS5_SPI5TXIF_MASK        (1u << 11u)  /* 0x800 */
#define _IEC5_SPI5EIE_MASK         (1u << 9u)
#define _IEC5_SPI5RXIE_MASK        (1u << 10u)
#define _IEC5_SPI5TXIE_MASK        (1u << 11u)
```

### 10.3 Interrupt Dispatch (crt0.S)

The single-vector interrupt handler at `EBASE+0x200` dispatches SPI5
interrupts from `IFS5 & IEC5`:

```asm
    /* Load IFS5 and IEC5, compute pending */
    lui     $26, 0xBF81             /* EVIC base */
    lw      $27, 0x0090($26)        /* IFS5 */
    lw      $26, 0x0110($26)        /* IEC5 */
    and     $26, $27, $26           /* pending5 = IFS5 & IEC5 */

    andi    $27, $26, 0x400         /* SPI5RXIF (bit 10) */
    bnez    $27, _spi5_rx_int

    andi    $27, $26, 0x800         /* SPI5TXIF (bit 11) */
    bnez    $27, _spi5_tx_int

    andi    $27, $26, 0x200         /* SPI5EIF (bit 9) */
    bnez    $27, _spi5_fault_int
```

**Important:** Bits 9–11 are in the lower 16 bits of the register, so `andi`
(16-bit immediate AND) must be used. Using `lui` (which loads into the upper
16 bits) was a bug that prevented SPI5 interrupts from being detected.

The SPI5 checks are placed **before** CAN1/CAN2 in the dispatch order since
all share IFS5/IEC5. The computed `$26` (pending5) is preserved across all
checks via `andi` (which only writes to `$27`).

### 10.4 ISR Wrappers (port_asm_patched.S)

FreeRTOS ISR wrappers save/restore context around the Harmony interrupt handlers:

```asm
MAKE_ISR_WRAPPER vSPI5RXInterruptWrapper,    SPI5_RX_InterruptHandler
MAKE_ISR_WRAPPER vSPI5TXInterruptWrapper,    SPI5_TX_InterruptHandler
MAKE_ISR_WRAPPER vSPI5FaultInterruptWrapper, SPI5_FAULT_InterruptHandler
```

These are defined in
`tests/pic32mk_rtos_demo/firmware/src/config/default/port_asm_patched.S`
and prototyped in `interrupts.h`.

### 10.5 Demo Task (main.c)

The FreeRTOS demo task `vSpi5SlaveTask` in `main.c`:

```c
static void vSpi5SlaveTask(void *pvParam)
{
    static const uint8_t reply[] = "WORLD";
    uint8_t buf[64];

    uart1_puts("[SPI5] Slave demo ready (send HELLO over socket)\r\n");

    /* Preload MISO response */
    SPI5_Write((void *)reply, sizeof(reply) - 1);

    for (;;) {
        size_t count = SPI5_ReadCountGet();
        if (count > 0) {
            SPI5_Read(buf, count);
            uart1_puts("[SPI5] RX '");
            /* print received data */
            uart1_puts("'\r\n");

            /* Refill reply buffer */
            SPI5_Write((void *)reply, sizeof(reply) - 1);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

Flow:
1. `SPI5_Initialize()` called in `main()` → configures slave mode, enables
   RX + Fault interrupts
2. Task preloads "WORLD" into the TX FIFO
3. When host sends "HELLO" via chardev socket, QEMU pushes bytes into RX FIFO
4. EVIC asserts SPI5RXIF → ISR fires → Harmony drains FIFO into internal buffer
5. Task polls `SPI5_ReadCountGet()` every 50 ms → reads data → prints via UART1
6. Task preloads "WORLD" again for the next exchange

---

## 11. Running the SPI Demo

### 11.1 QEMU Launch Command

```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors \
    -chardev socket,id=spi5,path=/tmp/qemu-spi5.sock,server=on,wait=off
```

Or using TCP:
```bash
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none \
    -d unimp,guest_errors \
    -chardev socket,id=spi5,host=localhost,port=12345,server=on,wait=off
```

### 11.2 Sending Data from Host

Using Python:
```python
import socket, struct

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/qemu-spi5.sock")

# Send "HELLO" with CS assert+deassert
frame = struct.pack("BB", 0x03, 5) + b"HELLO"
sock.sendall(frame)

# Read response
hdr = sock.recv(2)
flags, length = struct.unpack("BB", hdr)
response = sock.recv(length)
print(f"MISO: {response}")   # Expected: b'WORLD'

sock.close()
```

Using `socat` + `printf`:
```bash
printf '\x03\x05HELLO' | socat - UNIX-CONNECT:/tmp/qemu-spi5.sock | xxd
```

### 11.3 Expected Output

**UART1 (QEMU serial console):**
```
PIC32MK QEMU booting FreeRTOS...
SPI5 slave ready on chardev 'spi5'
[SPI5] Slave demo ready (send HELLO over socket)
[SPI5] RX 'HELLO'
```

**Host socket response:**
```
b'\x00\x05WORLD'   →  flags=0, len=5, payload="WORLD"
```

---

## 12. Known Issues & Pitfalls

| # | Issue | Root Cause | Fix |
|---|-------|------------|-----|
| 1 | SPI5 interrupts never fire | `lui` in crt0.S checks bits 25–27 instead of 9–11 | Use `andi` for bits in lower 16 |
| 2 | `SPI5_ReadCountGet()` always 0 | QEMU only fired RX IRQ on FIFO full (SPIRBF) | Respect SRXISEL field (value 1 = not empty) |
| 3 | TX interrupt fires immediately | STXISEL=3 means "not full" — empty FIFO qualifies | Harmony ISR disables TX IRQ when nothing to send |
| 4 | TCP partial frames | TCP may split chardev data | Frame reassembly in `spi_chr_receive()` |
| 5 | RX overflow | Host sends faster than firmware drains | SPIROV set; FAULT ISR clears it |
| 6 | Chardev naming | Each SPI instance looks for chardev `spi<N>` | ID must match (e.g. `-chardev socket,id=spi5,...`) |

---

## 13. References

- **DS60001519E** — PIC32MK GPK/MCM with CAN FD Family Datasheet, §23 (SPI)
- **Microchip Harmony v3** — SPI Slave PLIB source code
- **QEMU Device Model API** — MemoryRegionOps, SysBusDevice, CharFrontend
- `hw/mips/pic32mk_spi.c` — QEMU SPI device implementation
- `hw/mips/pic32mk.c` — Board instantiation and wiring
- `include/hw/mips/pic32mk.h` — Constants and IRQ definitions
