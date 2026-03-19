# Analog Simulation Engine — Mocking Analog I/O from Host Software

While QEMU has no analog simulation engine, you can **inject and read analog values from host software**. The "fundamental limitation" is that there's no physics — but you can replace physics with a software mock.

## The Key Insight

From the firmware's perspective, an ADC is just **a register that returns a number** after a conversion-complete interrupt fires. The firmware never sees an actual voltage — it sees a 12-bit integer in `ADCDATAx`. So all you need is a way to **set that number from the host**.

## Architecture Options

### Option 1: QMP/Monitor Commands (simplest)

QEMU's QMP (QEMU Machine Protocol) lets host software send JSON commands over a Unix socket:

```
Host Python script                    QEMU
┌──────────────┐    QMP socket    ┌──────────────────┐
│              │ ──────────────→  │                  │
│  adc_inject  │  {"execute":     │  pic32mk_adc.c   │
│  (Python)    │   "pic32mk-adc", │  ADCDATA0 = val  │
│              │   "channel": 0,  │  → trigger IRQ    │
│              │   "value": 2048} │                  │
│              │ ←──────────────  │  ack              │
└──────────────┘                  └──────────────────┘
```

You'd register a custom QMP command in your ADC peripheral model. Host scripts inject values on demand.

### Option 2: CharDev Socket (streaming)

For continuous analog stimulus (e.g., simulating a sine wave on an ADC input):

```
Host simulator                        QEMU
┌──────────────────┐   TCP/Unix   ┌───────────────────┐
│                  │   socket     │                   │
│  Writes binary   │ ──────────→  │  pic32mk_adc.c    │
│  packets:        │              │                   │
│  {ch, value}     │              │  Reads packets,   │
│  every N ms      │              │  stores in shadow │
│                  │              │  register per ch  │
│  Python/C/Rust   │              │  Triggers conv    │
│  signal gen      │              │  complete IRQ     │
└──────────────────┘              └───────────────────┘
```

Launch QEMU with:

```bash
qemu-system-mipsel -M pic32mk \
    -chardev socket,id=adc0,host=localhost,port=5555,server=on,wait=off \
    ...
```

Your ADC model reads `{channel, value}` tuples from the chardev. A host Python script generates the stimulus:

```python
import socket, struct, time, math

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("localhost", 5555))

t = 0
while True:
    # Simulate 1 kHz sine on ADC channel 0, 12-bit range
    value = int(2048 + 2047 * math.sin(2 * math.pi * 1000 * t))
    sock.send(struct.pack("<BH", 0, value))  # channel=0, value=uint16
    t += 0.001
    time.sleep(0.001)
```

### Option 3: Shared Memory (lowest latency)

For high-rate stimulus (motor control simulation with external Simulink/Python model):

```
┌─────────────────┐                  ┌───────────────┐
│  Host Model     │   mmap'd file    │  QEMU         │
│  (Simulink,     │ ←──────────────→ │               │
│   Python,       │  adc_values[32]  │  ADC reads    │
│   C model)      │  dac_values[3]   │  DAC writes   │
│                 │  pwm_state[12]   │  PWM outputs  │
│  Updates at     │                  │               │
│  model rate     │  Shared memory   │  Samples on   │
│                 │  region          │  conversion   │
└─────────────────┘                  └───────────────┘
```

QEMU's `-object memory-backend-file` or a custom device with `mmap` makes this possible.

### Option 4: GPIO/DAC Output Capture

The same pattern works in reverse — DAC output registers and PWM duty-cycle registers are just numbers the firmware writes:

```c
// In pic32mk_dac.c write handler:
static void pic32mk_dac_write(void *opaque, hwaddr addr, uint64_t val, unsigned size) {
    // ... register update ...
    if (addr == DACxDAT_OFFSET) {
        // Forward to host via chardev/QMP/shared-mem
        qemu_chr_fe_write(&s->chr, buf, len);
    }
}
```

Host software can then **plot, log, or feed into a plant model** what the firmware is outputting.

## What This Enables

| Use Case | How |
|---|---|
| **Unit test ADC driver** | QMP: inject known value → verify firmware reads correct result |
| **Test ADC interrupt flow** | Socket: inject value → verify ISR fires → verify DMA transfer |
| **Simulate sensor input** | Socket/mmap: Python generates temperature ramp → firmware reacts |
| **Closed-loop mock** | Mmap: host reads PWM duty → runs motor model → writes back-EMF to ADC → repeat |
| **Capture DAC output** | Chardev: firmware writes DAC → host logs/plots the waveform |
| **Regression testing** | Script injects known stimulus → assert firmware outputs match expected |

## What You Still Can't Do

- **Real-time fidelity**: The loop `ADC sample → firmware processes → DAC output` won't run at 120 MHz wall-clock speed. The timing is functional, not temporal.
- **Analog signal integrity**: No noise, no settling time, no bandwidth limits — it's perfect math.
- **Mixed-signal interaction**: Comparator output won't change dynamically based on DAC output unless you explicitly wire that logic in the host model.

## Summary

The "no analog" limitation means **no built-in physics engine** — but it does **not** mean you can't inject and capture analog values. Every analog peripheral reduces to register reads/writes, and QEMU provides multiple mechanisms (QMP, chardev sockets, shared memory) to bridge those registers to host software. You can mock anything from a simple fixed value to a full plant model running alongside QEMU.
