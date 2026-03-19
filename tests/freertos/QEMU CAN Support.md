# QEMU CAN Support for PIC32MK Emulation

QEMU has built-in CAN bus infrastructure that can be wired to the PIC32MK emulation.

## QEMU's CAN Support

QEMU already provides:

1. **Virtual CAN bus object** (`can-bus`) — an in-process bus that connects multiple CAN controllers
2. **Host CAN backend** (`can-host-socketcan`) — bridges QEMU's virtual CAN to a Linux SocketCAN interface (`vcan0`, `can0`)
3. **Existing CAN controller models** — Xilinx CAN, Kvaser PCI CAN (based on SJA1000) — these serve as implementation references

## Architecture for PIC32MK CAN FD

```
┌─────────────────────────────────────┐
│  Host Linux                         │
│                                     │
│  candump vcan0  ←──┐               │
│  cansend vcan0 ──┐ │               │
│                  │ │               │
│         ┌────────▼─┴────────┐      │
│         │  vcan0 (SocketCAN)│      │
│         └────────┬──────────┘      │
│                  │                  │
│  ┌───────────────▼───────────────┐ │
│  │  QEMU                         │ │
│  │                               │ │
│  │  can-host-socketcan ←→ can-bus│ │
│  │                          │    │ │
│  │              ┌───────────┘    │ │
│  │              ▼                │ │
│  │   pic32mk_canfd (CxFIFOUA,   │ │
│  │    CxCON, CxINT, etc.)       │ │
│  │              │                │ │
│  │              ▼                │ │
│  │   EVIC → CPU IRQ             │ │
│  └───────────────────────────────┘ │
└─────────────────────────────────────┘
```

## How to Use It

### 1. Create a virtual CAN interface on the host

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

### 2. Launch QEMU with CAN wired to vcan0

```bash
qemu-system-mipsel -M pic32mk \
    -bios hello_freertos.bin \
    -object can-bus,id=canbus0 \
    -object can-host-socketcan,id=canhost0,if=vcan0,canbus=canbus0 \
    -serial stdio -nographic
```

The `-object can-bus` creates the virtual bus. The `-object can-host-socketcan` bridges it to the host's `vcan0`.

### 3. Send/receive from the host

```bash
# Send a CAN frame to the emulated PIC32MK
cansend vcan0 123#DEADBEEF

# Watch what the emulated firmware transmits
candump vcan0
```

CAN FD frames:

```bash
cansend vcan0 123##1.DEADBEEFCAFEBABE   # CAN FD frame with BRS
```

## What Needs to Be Implemented (Phase 3)

The CAN controller peripheral model (`pic32mk_canfd.c`) needs to:

| Component | What it does |
|---|---|
| **Register interface** | CxCON, CxINT, CxFIFOCON, CxFIFOUA, CxFLTCON, CxMASK, CxTXQ — per datasheet register map |
| **FIFO engine** | TX FIFOs read messages from RAM (via CxFIFOUA pointer), RX FIFOs write received messages to RAM |
| **Acceptance filters** | CxFLTOBJ / CxMASK matching on incoming frames |
| **`CanBusClientState` integration** | Implement QEMU's `can_bus_client_receive()` callback to accept frames from the virtual bus, and call `can_bus_client_send()` for TX |
| **Interrupt generation** | TXIF, RXIF, RXOVIF → EVIC IRQ lines |

## Key QEMU CAN API (`include/net/can_emu.h`)

```c
// Your controller registers as a bus client:
can_bus_insert_client(canbus, &your_client);

// TX: firmware writes to TX FIFO, you call:
can_bus_client_send(&your_client, &frame, 1);

// RX: QEMU calls your callback when a frame arrives:
static bool pic32mk_can_receive(CanBusClientState *client,
    const qemu_can_frame *frames, size_t frames_cnt);
```

## Summary

Full bidirectional CAN communication between host tools and emulated firmware is achievable. The QEMU infrastructure and Linux SocketCAN plumbing already exist — only the PIC32MK-specific CAN FD register model needs to be implemented and wired to QEMU's `can-bus` object.
