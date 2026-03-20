# Bootloader with Program Flash Write + Software Reset — Plan

## Yes, it's fully possible. Here's what's needed:

### The Problem (2 things missing today)

1. **Program Flash is read-only** — `pflash` is created with `memory_region_init_rom()`, so any store from firmware to `0xBD000000` will silently fail
2. **RSWRST (software reset) is a no-op** — the `rcon_write()` stub ignores writes to `RSWRST` at `0xBF801250`

### The Plan (3 QEMU changes + 1 firmware)

**Change 1 — Make Program Flash writable**
- In `pic32mk_memory_init()`, change `memory_region_init_rom(&s->pflash, ...)` to `memory_region_init_ram(&s->pflash, ...)`
- This lets firmware write directly to physical `0x1D000000` (KSEG1 `0xBD000000`)
- Approximates what the real NVM controller does (without the NVMCON sequence)

**Change 2 — Implement RSWRST software reset**
- Add `bool rswrst_armed` to `PIC32MKState`
- In `rcon_write()`: when firmware writes `1` to RSWRST offset, set `rswrst_armed = true`
- In `rcon_read()`: when firmware reads RSWRST and `rswrst_armed` is true, call `qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET)` — this is the standard QEMU API for guest-initiated resets (same as what MAX78000, PPC40x, S390x use)
- This matches the real PIC32MK reset sequence: write RSWRST=1, then read RSWRST

**Change 3 — Support loading an app image into Program Flash**
- Use QEMU's built-in generic loader: `-device loader,file=app.bin,addr=0x1D000000`
- No code change needed — QEMU already supports this
- OR the bootloader can write the app into Program Flash via SRAM copy (since pflash is now writable)

**Firmware — Bootloader demo** (`tests/firmware/bootloader.c`)

The bootloader would run from Boot Flash 1 (`0xBFC40000`) and:
1. Init UART1, print "Bootloader starting..."
2. Check if a valid app signature exists at `0xBD000000` (Program Flash)
3. If valid: print "Jumping to application", jump to the app entry point
4. If not: write a small test app into Program Flash (memcpy to `0xBD000000`), print "App installed"
5. Trigger software reset: write `RSWRST = 1`, read `RSWRST` → system resets
6. On reboot: bootloader runs again, finds the app, jumps to it
7. App prints "Hello from application!" via UART1

### How it would run

```bash
# Boot 1: bootloader writes app to pflash, triggers RSWRST reset
# Boot 2: bootloader finds app, jumps to it, app prints message
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/firmware/bootloader.bin \
    -serial stdio -nographic -d unimp,guest_errors
```

### Scope

- 3 small edits to `hw/mips/pic32mk.c` (~40 lines net)
- 1 new test firmware (bootloader.c + linker script, ~80 lines)
- No changes to any other QEMU files
- Stays within Phase 2 scope — no new peripherals needed
