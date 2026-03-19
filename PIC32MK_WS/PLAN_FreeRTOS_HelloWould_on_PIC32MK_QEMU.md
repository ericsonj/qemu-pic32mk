# Plan: FreeRTOS Hello World on PIC32MK QEMU

## Context
Phase 1 is complete: the PIC32MK board boots, RAM/Flash/SFR are mapped, the reset vector trampoline works, UART1 TX is functional, and a C Hello World runs under QEMU. The next milestone is running **FreeRTOS with a Hello World task** that prints to UART1. This requires Phase 2 peripherals. The user wants the full set: EVIC, Timers, UART (full), SPI, I2C, GPIO, DMA.

---

## Critical Insight: Minimal Path vs Full Phase 2

**FreeRTOS Hello World requires only 3 things:**
1. **EVIC** — routes interrupts to CPU; FreeRTOS can't enable/switch tasks without it
2. **CP0 Core Timer** — already works in QEMU's MIPS CPU; no new code needed for tick
3. **UART1 TX** — already done

Everything else (SPI, I2C, GPIO, DMA, Timers 1–9) is needed for motor control firmware but **not** for a basic FreeRTOS Hello World. The plan is phased accordingly.

---

## Phase 2A — FreeRTOS Hello World (Critical Path, ~2–3 weeks)

### Task 1: EVIC Interrupt Controller
**File**: `hw/mips/pic32mk_evic.c` + `hw/mips/pic32mk_evic.h`

**What it must do:**
- Implement INTCON, IFS0–7, IEC0–7, IPC0–63, OFF0–190 register banks
- Support atomic SET/CLR/INV sub-registers (offset +4/+8/+C from each base)
- Route interrupt sources → CPU pins via priority logic
- Wire CP0 Core Timer (source 0, IRQ2) through EVIC to CPU
- Support both single-vector mode (INTCON.MVEC=0, simple) and multi-vector mode (MVEC=1, needed for FreeRTOS full port)

**EVIC register map** (from `pic32mk.h` constants, base physical 0x1F810000):
```
INTCON   offset 0x0000    MVEC bit, TPC, INT0–4 edge polarity
PRISS    offset 0x0010    Priority shadow register select
INTSTAT  offset 0x0020    Last IRQ number serviced + SRIPL
IPTMR    offset 0x0030    Interrupt proximity timer (stub OK)
IFS0–7   offset 0x0040–0x00BF   Interrupt flag status (216 sources)
IEC0–7   offset 0x00C0–0x013F   Interrupt enable control
IPC0–63  offset 0x0140–0x023F   Priority configuration (4 sources/reg)
OFF0–190 offset 0x0540–0x083C   Vector address offsets (multi-vector)
```

**Internal state struct:**
```c
typedef struct {
    SysBusDevice parent;
    MemoryRegion mr;

    uint32_t intcon;
    uint32_t intstat;
    uint32_t ifsreg[8];    /* IFS0–7: 256 flag bits covering 216 sources */
    uint32_t iecreg[8];    /* IEC0–7: 256 enable bits */
    uint32_t ipcreg[64];   /* IPC0–63: priority (3 bits) + subpri (2 bits) per source */
    uint32_t offreg[191];  /* OFF0–190: vector address offsets */

    qemu_irq cpu_irq[8];   /* Connected to MIPS CPU env->irq[0..7] */
} PIC32MKEVICState;
```

**IRQ routing logic** (called after any IFS/IEC/IPC write):
```
for each source N (0..215):
    if IFSx[N] && IECx[N]:
        priority = IPCx[N].PRIORITY (3 bits)
        subpri   = IPCx[N].SUBPRI   (2 bits)
        cpu_pin  = priority (maps to env->irq[priority])
        raise cpu_irq[cpu_pin] if higher than currently pending on that pin
```

**Single-vector mode (start here):**
- All enabled+pending interrupts assert the highest-priority CPU pin
- CPU takes general exception, handler reads INTSTAT to find source number

**Multi-vector mode (add after single-vector works):**
- CPU fetches handler from EBASE + OFFx[source] << 2
- EVIC sets INTSTAT.VECNUM to source number before CPU fetches handler

**Wiring EVIC into pic32mk.c:**
- `pic32mk_evic.c` should export `qemu_irq pic32mk_evic_get_irq(state, source_num)`
- Board code in `pic32mk.c` connects CP0 timer's IRQ output to EVIC source 0

**CP0 Core Timer → EVIC source 0 wiring:**
The CP0 Compare interrupt fires on `env->irq[IPTI]`. IPTI is encoded in CP0_IntCtl[IPTI] (bits 31:29). For M14K default: IPTI = 7 (fires on env->irq[7]).

In `pic32mk_cpu_init()`, after CPU creation, wire CP0 timer output into EVIC:
```c
/* CP0 core timer fires on env->irq[7] by default (IPTI=7 in M14K) */
/* Reroute: replace env->irq[7] with an EVIC input so CP0 timer → EVIC → CPU */
```

Actually simpler: let EVIC export irq_in[0] for the core timer, and in the board's machine_init, set env->irq[7] to be an EVIC pass-through.

**Estimate**: 4–6 days for single-vector mode; +2 days for multi-vector.

**Reference**: `hw/intc/mips_gic.c` (MIPS GIC, closest analog, 560 lines)

---

### Task 2: FreeRTOS Port for PIC32MK + QEMU
**Files**: `tests/freertos/` directory

**FreeRTOS source**: Use the official Microchip/MIPS FreeRTOS port for PIC32
- FreeRTOS kernel: `tasks.c`, `queue.c`, `timers.c`, `list.c`, `portable/MIPS32/`
- Port layer: `portmacro.h`, `port.c` — handles context switch, CP0 timer, shadow registers

**Key adaptations for QEMU:**
1. `configCPU_CLOCK_HZ = 120000000` (120 MHz, from `PIC32MK_CPU_HZ`)
2. `configTICK_RATE_HZ = 1000` (1ms tick)
3. Use CP0 Core Timer for tick (no Timer1 peripheral needed)
4. `vApplicationMallocFailedHook`, `vApplicationStackOverflowHook` → log to UART1

**Hello World task:**
```c
void vHelloTask(void *pvParam) {
    for (;;) {
        uart1_puts("Hello from FreeRTOS!\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
int main(void) {
    uart1_init();
    xTaskCreate(vHelloTask, "Hello", 512, NULL, 1, NULL);
    vTaskStartScheduler();
}
```

**Linker script additions for FreeRTOS:**
- Heap section in RAM (at least 16 KB for `configTOTAL_HEAP_SIZE`)
- Stack for each task in RAM
- FreeRTOS needs `.data` initialized from flash → LMA/VMA copy in crt0

**Estimate**: 3–5 days (most effort is in port.c adaptation)

---

## Phase 2B — Full Peripheral Set (~6–8 weeks additional)

Order by importance / dependency:

### Task 3: UART×6 (full, with RX + interrupts)
**File**: `hw/mips/pic32mk_uart.c`

Move UART1 out of `pic32mk.c` into its own SysBusDevice. Add:
- RX buffering via `qemu_chr_fe_set_handlers()` receive callback
- RX interrupt routing through EVIC
- Status bits: OERR (overrun), FERR (framing), PERR (parity)
- All 6 UART instances (U1..U6) with configurable base addresses

**UART register block** (per instance, 0x50 bytes):
```
UxMODE   +0x00   Mode (enable, stop bits, parity, data width)
UxSTA    +0x10   Status (TRMT, UTXBF, URXDA, UTXEN, URXEN)
UxTXREG  +0x20   TX data (write = transmit)
UxRXREG  +0x30   RX data (read = receive, clears URXDA)
UxBRG    +0x40   Baud rate generator
```

**Estimate**: 3–4 days

### Task 4: Timers 1–9
**File**: `hw/mips/pic32mk_timer.c`

PIC32MK has Type A (16-bit), Type B (16/32-bit), and Type C timers.
Key registers per timer (`hw/mips/pic32mk.h` has PER1 block base 0xBF820000):
```
TxCON    +0x00   Control (ON bit, TCKPS prescaler, T32 for 32-bit)
TMRx     +0x10   Current count (read/write)
PRx      +0x20   Period register (interrupt when TMR == PR)
```

Use QEMU `ptimer` or `timer_mod()` for scheduling. On expiry:
- Set TxIF bit in EVIC IFS registers
- Fire EVIC IRQ if TxIE is set

**Estimate**: 5–7 days

### Task 5: GPIO (PORTA–PORTG)
**File**: `hw/mips/pic32mk_gpio.c`

Base 0xBF860000. Registers per port (TRIS, PORT, LAT, ODC, CNPU, CNPD, CNCON, CNEN, CNSTAT):
- Input lines: drive from QEMU command-line or test framework
- Output lines: expose as named GPIO outputs (for LED blinking tests)
- PPS (Peripheral Pin Select) table: accept writes, log remapping

**Estimate**: 3–4 days

### Task 6: SPI×4 (SPI1–SPI4, later SPI5–6)
**File**: `hw/mips/pic32mk_spi.c`

Registers (per instance): SPIxCON, SPIxSTAT, SPIxBUF, SPIxBRG, SPIxCON2
- TX/RX via loopback or user-space socket
- For FreeRTOS driver testing: implement SPIROV, SPIBUSY, SPITBE, SPIRBF status bits

**Estimate**: 4–5 days

### Task 7: I2C×4
**File**: `hw/mips/pic32mk_i2c.c`

Registers: I2CxCON, I2CxSTAT, I2CxADD, I2CxMSK, I2CxTRN, I2CxRCV
- Master mode: START/STOP/ACK state machine
- QEMU i2c_bus integration via `hw/i2c/i2c.h`

**Estimate**: 5–6 days

### Task 8: DMA×8
**File**: `hw/mips/pic32mk_dma.c`

Registers (per channel): DCHxCON, DCHxECON, DCHxINT, DCHxSSA, DCHxDSA, DCHxSSIZ, DCHxDSIZ, DCHxSPTR, DCHxDPTR
- Transfer on event trigger (peripheral interrupt, software FORCE)
- Memory-to-memory, peripheral-to-memory, memory-to-peripheral
- Chain DMA channels
- Fire EVIC source on completion

**Estimate**: 6–8 days (most complex peripheral)

---

## Build System Changes

Each new `.c` file needs to be added to `hw/mips/meson.build`:
```meson
mips_ss.add(when: 'CONFIG_PIC32MK', if_true: files(
  'pic32mk.c',
  'pic32mk_evic.c',    ← Phase 2A
  'pic32mk_uart.c',    ← Phase 2B
  'pic32mk_timer.c',   ← Phase 2B
  'pic32mk_gpio.c',    ← Phase 2B
  'pic32mk_spi.c',     ← Phase 2B
  'pic32mk_i2c.c',     ← Phase 2B
  'pic32mk_dma.c',     ← Phase 2B
))
```

New header constants need to go into `include/hw/mips/pic32mk.h`.

---

## Critical Files

| File | Role |
|---|---|
| `hw/mips/pic32mk.c` | Board init — will call evic_create(), timers_init() etc. |
| `hw/mips/pic32mk_evic.c` | **To create** — EVIC device model |
| `hw/mips/pic32mk_uart.c` | **To create** — Full UART×6 (move UART1 stub out of pic32mk.c) |
| `hw/mips/pic32mk_timer.c` | **To create** — Timers 1–9 |
| `hw/mips/pic32mk_gpio.c` | **To create** — GPIO A–G |
| `hw/mips/pic32mk_spi.c` | **To create** — SPI 1–6 |
| `hw/mips/pic32mk_i2c.c` | **To create** — I2C 1–4 |
| `hw/mips/pic32mk_dma.c` | **To create** — DMA 1–8 |
| `include/hw/mips/pic32mk.h` | Add per-peripheral register offsets as needed |
| `hw/mips/meson.build` | Add new source files |
| `target/mips/system/cp0_timer.c` | Read-only reference — CP0 timer already works |
| `hw/mips/mips_int.c` | Read-only reference — CPU IRQ wiring |

**Reference implementations to study:**
| Reference | Pattern used |
|---|---|
| `hw/intc/mips_gic.c` | Vectored interrupt controller closest to EVIC |
| `hw/char/stm32f2xx_usart.c` | UART + CharFrontend + IRQ |
| `hw/timer/arm_mptimer.c` | Periodic timer + IRQ |
| `hw/i2c/` | I2C bus infrastructure |

---

## Verification Plan

### After EVIC (Task 1):
```bash
# Firmware that enables CP0 timer interrupt via EVIC and handles it:
./build/qemu-system-mipsel -M pic32mk -bios tests/firmware/evic_test.bin \
    -serial stdio -nographic -monitor none -d guest_errors
# Expected: timer ISR increments counter, UART prints "tick N" every second
```

### After FreeRTOS port (Task 2):
```bash
./build/qemu-system-mipsel -M pic32mk -bios tests/freertos/hello_freertos.bin \
    -serial stdio -nographic -monitor none
# Expected: "Hello from FreeRTOS!" printed every ~1 second
```

### After each Phase 2B peripheral:
Run `-d unimp,guest_errors` — the LOG_UNIMP output should shrink as each peripheral is implemented.

---

## User Decisions
- **FreeRTOS port**: Official Microchip/MIPS port for PIC32 (port.c + portmacro.h from FreeRTOS-Kernel portable/MPLAB)
- **EVIC mode**: Single-vector first (MVEC=0), then add multi-vector (MVEC=1)
- **Scope for FreeRTOS Hello World**: EVIC + UART + Timers 1–9 + GPIO A–G (SPI/I2C/DMA deferred)

---

## Implementation Order

```
Week 1–2:  EVIC single-vector mode + CP0 Core Timer routing (Task 1)
Week 2–3:  Official Microchip FreeRTOS port adaptation + Hello World task
           → MILESTONE: FreeRTOS multitasking + UART Hello World confirmed
Week 3–4:  EVIC multi-vector mode (OFFx registers, INTSTAT, EIC mode)
Week 4–5:  Full UART×6 refactor (RX buffering + EVIC interrupt, move to pic32mk_uart.c)
Week 5–6:  Timers 1–9 (pic32mk_timer.c, EVIC integration)
Week 6–7:  GPIO A–G (pic32mk_gpio.c, PPS stub)
Week 8+:   SPI, I2C, DMA — Phase 2B continuation (separate plan when ready)
```
