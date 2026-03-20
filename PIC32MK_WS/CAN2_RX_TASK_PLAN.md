# Plan: CAN2 RX task — interrupt-driven receive with FreeRTOS queue

## Context

CAN1 TX is already working (vCanTxTask sends "hello" frames every 3 s). The user now wants
a CAN2 **receive** task that:
- Receives frames via interrupt (EVIC source 168, IFS5 bit 8)
- Posts each received frame to a FreeRTOS queue from ISR context
- A consumer task reads from the queue and prints on UART1 as:
  `[CAN2 RX] id=0x%03X len=%u data='...'`

QEMU already instantiates CAN2 (`pic32mk_canfd_create` with `PIC32MK_CAN2_OFFSET=0x081000`,
`PIC32MK_IRQ_CAN2=168`, `pic32mk_find_canbus(1)`), so no emulator changes are needed.
The firmware side needs the Harmony3 plib + support macros + ISR wiring + task.

---

## Key addresses

| Item | Value |
|------|-------|
| CAN2 SFR base (KSEG1) | `0xBF881000` |
| CAN2 EVIC source | 168 |
| CAN2IF/CAN2IE register | IFS5/IEC5 (EVIC+0x0090/0x0110) |
| CAN2IF bit | bit 8 → mask `0x00000100` |
| CAN1IF bit | bit 7 → mask `0x00000080` (existing, for reference) |

plib_canfd2 uses FIFO1 for TX, **FIFO2 for RX**.

---

## Steps

### Step 1 — Copy plib files

Copy from `/home/ericson/PROJECTS/VOLTU/C_C++/PRG-IDU/PRG-IDU-0003/firmware/src/config/default/peripheral/canfd/`:
```
plib_canfd2.c  →  tests/freertos/plib_canfd2.c
plib_canfd2.h  →  tests/freertos/plib_canfd2.h
```
No modifications to plib files — they are unmodified Harmony3 copies.

### Step 2 — `tests/freertos/xc.h`

Add CFD2* macros block immediately after the CFD1 block. Pattern is identical to CFD1
with `_CFD1_BASE = 0xBF880000` → `_CFD2_BASE = 0xBF881000` and all macro names
`_CFD1xxx` → `_CFD2xxx`. Registers needed by plib_canfd2:

**SFR address macros** (volatile pointer dereferences at `_CFD2_BASE + offset`):
- `CFD2CON`, `CFD2CONSET`, `CFD2CONCLR`
- `CFD2NBTCFG`, `CFD2DBTCFG`
- `CFD2TBC`, `CFD2VEC`
- `CFD2INT`, `CFD2INTSET`, `CFD2INTCLR`
- `CFD2RXIF`, `CFD2TXIF`, `CFD2RXOVIF`, `CFD2TXATIF`
- `CFD2TEFCON`, `CFD2TEFSTA`, `CFD2TEFUA`
- `CFD2FIFOBA`
- `CFD2TXQCON`, `CFD2TXQCONSET`, `CFD2TXQCONCLR`, `CFD2TXQSTA`, `CFD2TXQUA`
- `CFD2FIFOCON1`, `CFD2FIFOCON1SET`, `CFD2FIFOCON1CLR`, `CFD2FIFOSTA1`, `CFD2FIFOUA1`
- `CFD2FIFOCON2`, `CFD2FIFOCON2SET`, `CFD2FIFOCON2CLR`, `CFD2FIFOSTA2`, `CFD2FIFOUA2`
- `CFD2FLTCON0`, `CFD2FLTCON0SET`, `CFD2FLTOBJ0`, `CFD2MASK0`

**Bit-field macros** (same values as CFD1 equivalents, just renamed):
- `_CFD2CON_ON_MASK`, `_CFD2CON_CLKSEL0_MASK`, `_CFD2CON_REQOP_POSITION/_MASK`,
  `_CFD2CON_OPMOD_POSITION/_MASK`, `_CFD2CON_STEF_MASK`, `_CFD2CON_TXQEN_MASK`
- All `_CFD2NBTCFG_*`, `_CFD2DBTCFG_*` (BRP/TSEG1/TSEG2/SJW position+mask)
- `_CFD2INT_TXIF_MASK`, `_CFD2INT_RXIF_MASK`, `_CFD2INT_MODIF_MASK`,
  `_CFD2INT_TXIE_MASK`, `_CFD2INT_RXIE_MASK`, `_CFD2INT_MODIE_MASK`,
  `_CFD2INT_SERRIE_MASK`, `_CFD2INT_CERRIE_MASK`, `_CFD2INT_IVMIE_MASK`,
  `_CFD2INT_SERRIF_MASK`, `_CFD2INT_CERRIF_MASK`, `_CFD2INT_IVMIF_MASK`
- `_CFD2VEC_ICODE_MASK`
- `_CFD2TEFCON_FSIZE_POSITION/_MASK`
- `_CFD2TXQCON_UINC_MASK`, `_CFD2TXQCON_TXREQ_MASK`, `_CFD2TXQCON_TXPRI_POSITION`,
  `_CFD2TXQCON_FSIZE_POSITION/_MASK`, `_CFD2TXQCON_PLSIZE_POSITION/_MASK`,
  `_CFD2TXQCON_TXQEIE_MASK`, `_CFD2TXQSTA_TXQNIF_MASK`
- `_CFD2FIFOCON1_TXEN_MASK`, `_CFD2FIFOCON1_UINC_MASK`, `_CFD2FIFOCON1_TXREQ_MASK`,
  `_CFD2FIFOCON1_TXPRI_POSITION`, `_CFD2FIFOCON1_FSIZE_POSITION/_MASK`,
  `_CFD2FIFOCON1_PLSIZE_POSITION/_MASK`, `_CFD2FIFOCON1_RTREN_POSITION`,
  `_CFD2FIFOCON1_TFNRFNIE_MASK`, `_CFD2FIFOCON1_TFERFFIE_MASK`,
  `_CFD2FIFOCON1_TFNRFNIF_MASK`, `_CFD2FIFOSTA1_TFNRFNIF_MASK`
- `_CFD2FIFOCON2_FSIZE_POSITION/_MASK`, `_CFD2FIFOCON2_PLSIZE_POSITION/_MASK`,
  `_CFD2FIFOSTA2_TFNRFNIF_MASK`
- `_CFD2FLTCON0_F0BP_POSITION/_MASK`, `_CFD2FLTCON0_FLTEN0_MASK`
- `_CFD2FLTOBJ0_EXIDE_MASK`, `_CFD2MASK0_MIDE_MASK`

Also add **IFS5/IEC5 CAN2 bit**:
```c
#define _IFS5_CAN2IF_MASK   0x00000100u   /* source 168 = IFS5 bit 8 */
#define _IEC5_CAN2IE_MASK   0x00000100u
```
(IFS5SET/IFS5CLR/IEC5SET/IEC5CLR macros already exist from CAN1 work.)

### Step 3 — `tests/freertos/interrupts.h`

Add:
```c
void CAN2_InterruptHandler(void);
```

### Step 4 — `tests/freertos/port_asm_patched.S`

Add after the `vCAN1InterruptWrapper` block:
```asm
    .extern CAN2_InterruptHandler
    .global vCAN2InterruptWrapper
    .ent    vCAN2InterruptWrapper
vCAN2InterruptWrapper:
    portSAVE_CONTEXT
    jal     CAN2_InterruptHandler
    nop
    portRESTORE_CONTEXT
    .end    vCAN2InterruptWrapper
```

### Step 5 — `tests/freertos/crt0.S`

In `_int_handler`, after the existing CAN1 check block and before the default Timer1
handler, add CAN2 dispatch (IFS5 bit 8):

```asm
    /* --- Check CAN2 interrupt in IFS5/IEC5 ---
     * CAN2 = EVIC source 168 = IFS5 bit 8 */
    lui     $26, 0xBF81
    lw      $27, 0x0090($26)        /* IFS5 */
    lw      $26, 0x0110($26)        /* IEC5 */
    and     $26, $27, $26

    lui     $27, 0x0001             /* bit 8 = 0x00000100 — need full 32-bit */
    /* Actually: bit 8 = 0x100, fits in andi */
    andi    $27, $26, 0x100         /* CAN2IF (bit 8) */
    bnez    $27, _can2_int
    nop

    /* --- Default: Timer1 tick --- */
    ...

_can2_int:
    .extern vCAN2InterruptWrapper
    la      $26, vCAN2InterruptWrapper
    jr      $26
    nop
```

Note: `andi` is 16-bit immediate, 0x100 = 256 fits within 16-bit unsigned range. No `lui` needed.

### Step 6 — `tests/freertos/Makefile`

Add `plib_canfd2.c` to `C_SRCS`:
```makefile
C_SRCS = \
    main.c \
    libc_stubs.c \
    plib_uart1.c \
    plib_uart2.c \
    plib_canfd1.c \
    plib_canfd2.c \      ← add
    ...
```

### Step 7 — `tests/freertos/main.c`

Add `#include "plib_canfd2.h"` near the top.

Define a compact frame struct and queue:
```c
typedef struct {
    uint32_t id;
    uint8_t  length;
    uint8_t  data[8];
} CAN2Frame_t;

static QueueHandle_t xCan2RxQueue;

/* Static buffer pre-registered with plib (refilled after each ISR) */
static uint32_t               can2_rx_id;
static uint8_t                can2_rx_len;
static uint8_t                can2_rx_data[8];
static uint32_t               can2_rx_ts;
static CANFD_MSG_RX_ATTRIBUTE can2_rx_attr;
```

ISR callback (called from `CAN2_InterruptHandler` inside `vCAN2InterruptWrapper`):
```c
static void can2_rx_callback(uintptr_t context)
{
    (void)context;
    CAN2Frame_t frame;
    frame.id     = can2_rx_id;
    frame.length = can2_rx_len;
    uint8_t i;
    for (i = 0; i < can2_rx_len && i < 8U; i++) {
        frame.data[i] = can2_rx_data[i];
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(xCan2RxQueue, &frame, &xHigherPriorityTaskWoken);
    /* Re-arm receive buffer for next frame */
    CAN2_MessageReceive(&can2_rx_id, &can2_rx_len, can2_rx_data,
                        &can2_rx_ts, 2U, &can2_rx_attr);
    /* Yield handled by portRESTORE_CONTEXT in vCAN2InterruptWrapper */
}
```

Consumer task:
```c
static void vCan2RxTask(void *pvParam)
{
    (void)pvParam;
    CAN2Frame_t frame;
    for (;;) {
        if (xQueueReceive(xCan2RxQueue, &frame, portMAX_DELAY) == pdTRUE) {
            uart1_puts("[CAN2 RX] id=0x");
            uart2_puthex((uint8_t)(frame.id >> 8));   /* reuse uart2_puthex */
            uart2_puthex((uint8_t)(frame.id));
            uart1_puts(" len=");
            uart1_putc('0' + frame.length);
            uart1_puts(" data='");
            uint8_t i;
            for (i = 0; i < frame.length && i < 8U; i++) {
                uart1_putc((char)frame.data[i]);
            }
            uart1_puts("'\r\n");
        }
    }
}
```

In `main()`:
```c
CAN2_Initialize();

xCan2RxQueue = xQueueCreate(16, sizeof(CAN2Frame_t));
CAN2_CallbackRegister(can2_rx_callback, 0, 2U);   /* FIFO2 = RX */
CAN2_MessageReceive(&can2_rx_id, &can2_rx_len, can2_rx_data,
                    &can2_rx_ts, 2U, &can2_rx_attr);

xTaskCreate(vCan2RxTask, "C2Rx", configMINIMAL_STACK_SIZE,
            NULL, tskIDLE_PRIORITY + 2, NULL);
```

---

## Testing

```bash
# 1. Rebuild firmware
cd tests/freertos && make

# 2a. Loopback test: connect CAN1 TX → CAN2 RX via shared virtual bus
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0

./build/qemu-system-mipsel -M pic32mk \
    -bios tests/freertos/hello_freertos.bin \
    -object can-bus,id=canbus0 \
    -object can-host-socketcan,id=canhost0,if=vcan0,canbus=canbus0 \
    -serial stdio -nographic -monitor none 2>/dev/null

# CAN1 TX and CAN2 RX share canbus0 (bus index 0 for CAN1, bus index 1 for CAN2)
# To make CAN1 TX frames visible to CAN2 RX they must share the same canbus.
# Update pic32mk_find_canbus(1) to use canbus0 as well (or connect via vcan0 loopback).

# 2b. Simpler: inject a frame from host with cansend on vcan0
cansend vcan0 100#68656C6C6F   # "hello"

# Expected UART1:
# [CAN2 RX] id=0x0100 len=5 data='hello'
```

**Note on CAN1→CAN2 loopback**: QEMU's `pic32mk_find_canbus(n)` uses separate bus IDs
per argument position. To loop CAN1 TX frames back to CAN2 RX, both must share the
same `can-bus` object. This requires passing the same `canbus=canbus0` to both CAN1
and CAN2 -object entries, OR simply using `cansend` from the host to inject test frames.

---

## Files to create / modify

| Action | Path |
|--------|------|
| COPY   | `tests/freertos/plib_canfd2.c` (from Harmony3) |
| COPY   | `tests/freertos/plib_canfd2.h` (from Harmony3) |
| MODIFY | `tests/freertos/xc.h` — add CFD2* macros + `_IFS5_CAN2IF_MASK` |
| MODIFY | `tests/freertos/interrupts.h` — add `CAN2_InterruptHandler` |
| MODIFY | `tests/freertos/port_asm_patched.S` — add `vCAN2InterruptWrapper` |
| MODIFY | `tests/freertos/crt0.S` — add CAN2 dispatch in `_int_handler` |
| MODIFY | `tests/freertos/Makefile` — add `plib_canfd2.c` |
| MODIFY | `tests/freertos/main.c` — add CAN2 RX queue + task |
