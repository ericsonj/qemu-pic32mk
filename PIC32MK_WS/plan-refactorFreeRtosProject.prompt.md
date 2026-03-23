# Plan: Refactor FreeRTOS Project to tests/pic32mk_rtos_demo with Pymaketool

Migrate `tests/freertos/` into a new `tests/pic32mk_rtos_demo/` using pymaketool build automation, matching the PRG-IDU-0003 structure. Uses `mipsel-linux-gnu-gcc`, the proven QEMU startup code (`crt0.S`), adapted Harmony files from tests/freertos, and a physical copy of the FreeRTOS kernel. Exercises UART1/2, CAN1/2, USB CDC, and Timer1.
Not remove `tests/freertos/` — both projects coexist, same functionality, different build systems.

**Decisions**: mipsel-linux-gnu-gcc toolchain | adapted Harmony files from tests/freertos | full peripheral scope (UART+CAN+USB) | physical FreeRTOS copy | reuse crt0.S | fix `thrid_party` → `third_party` typo

---

### Phase 1: Project Scaffolding *(no dependencies)*

**Step 1.** Create the full directory tree under `tests/pic32mk_rtos_demo/`:
- `firmware/src/`, `firmware/src/config/default/` with subdirs: `peripheral/evic/`, `system/debug/`, `system/int/`, `system/reset/`, `sys/`, `gnu/`
- `firmware/third_party/rtos/FreeRTOS/Source/include/`, `.../Source/portable/MemMang/`, `.../Source/portable/MPLAB/PIC32MK/`
- `firmware/APP_Firmware/`
- `Release/Objects/`

**Step 2.** Create `pyproject.toml` — project metadata with pymaketool dependency (model after PRG-IDU-0003, name `pic32mk-rtos-demo`)

---

### Phase 2: FreeRTOS Kernel Copy *(depends on Phase 1)*

**Step 3.** Copy FreeRTOS kernel from `/home/ericson/PROJECTS/VOLTU/.../FreeRTOS/Source/` → `firmware/third_party/rtos/FreeRTOS/Source/`:
- Top-level `.c` files: `FreeRTOS_tasks.c`, `croutine.c`, `event_groups.c`, `list.c`, `queue.c`, `stream_buffer.c`, `timers.c`
- `include/` directory (all FreeRTOS headers — full copy)
- `portable/MemMang/heap_1.c`
- `portable/MPLAB/PIC32MK/`: `ISR_Support.h`, `port.c`, `portmacro.h` (**not** `port_asm.S` — we use `port_asm_patched.S` from tests/freertos instead)

---

### Phase 3: Config/Default Layer *(depends on Phase 1, parallel with Phase 2)*

**Step 4.** Copy adapted config files from `tests/freertos/` → `firmware/src/config/default/`:
- **Startup/linker**: `crt0.S`, `link.ld`, `port_asm_patched.S`
- **C stubs**: `libc_stubs.c`, `osal_freertos.c`
- **Headers**: `FreeRTOSConfig.h`, `configuration.h`, `definitions.h`, `device.h`, `xc.h`, `interrupts.h`
- **Subdirectory files**: `peripheral/evic/plib_evic.h`, `system/debug/sys_debug.h`, `system/int/sys_int.h`, `system/int/sys_int_mapping.h`, `system/reset/sys_reset.h`, `sys/kmem.h`, `gnu/lib-names-o32_soft.h`, `gnu/stubs-o32_soft.h`

---

### Phase 4: APP_Firmware Layer *(depends on Phase 1, parallel with Phases 2–3)*

**Step 5.** Copy peripheral drivers from `tests/freertos/` → `firmware/APP_Firmware/`:
- **UART**: `plib_uart1.c/.h`, `plib_uart2.c/.h`, `plib_uart_common.h`
- **CAN FD**: `plib_canfd1.c/.h`, `plib_canfd2.c/.h`, `plib_canfd_common.h`
- **USB**: `usb_init.c/.h`, `usb_device.c`, `usb_device_cdc.c`, `usb_device_cdc_acm.c`, `usb_device_init_data.c`, `drv_usbfs.c`, `drv_usbfs_device.c`

---

### Phase 5: Application Source *(depends on Phase 1, parallel with Phases 2–4)*

**Step 6.** Copy `tests/freertos/main.c` → `firmware/src/main.c`. Review `#include` paths — peripheral driver includes (`plib_uart1.h`, `plib_canfd1.h`, `usb_init.h`) must resolve via the APP_Firmware include path.

---

### Phase 6: Pymaketool Build Files *(depends on Phases 2–5)*

**Step 7.** Create `Makefile.py` — root pymaketool entry. Adapt from PRG-IDU-0003/Makefile.py `Project` class with:
- `getCompilerSet()`: `CC=mipsel-linux-gnu-gcc`, `LD=mipsel-linux-gnu-ld`, `OBJCOPY=mipsel-linux-gnu-objcopy` etc.
- `getCompilerOpts()`: `MACHINE-OPTS` = `-march=mips32r2 -mdspr2 -msoft-float -mno-micromips -mabi=32 -EL -G 0 -mno-abicalls`; `OPTIMIZE-OPTS` = `-O1 -g`; `OPTIONS` = `-ffreestanding -fno-builtin -nostdlib -nostdinc -ffunction-sections -fdata-sections`; `INCLUDES` with GCC internal headers path (resolve via `subprocess` calling `gcc -print-file-name=include`)
- `getLinkerOpts()`: flags `-m elf32ltsmip --gc-sections -Map=...`; linker script `firmware/src/config/default/link.ld`
- `getTargetsScript()`: `LD` (not CC) for linking, then `OBJCOPY -O binary` for BIN target
- **PROJECT_NAME**: `pic32mk_rtos_demo`, **FOLDER_OUT**: `Release/Objects/`, **DIST_DIR**: `Release/`

**Step 8.** Create `firmware/src/app_mk.py`:
- `getSrcs()` → `['firmware/src/main.c']`
- `getIncs()` → `['firmware/src', 'firmware/src/config/default', 'firmware/third_party/rtos/FreeRTOS/Source/include', 'firmware/third_party/rtos/FreeRTOS/Source/portable/MPLAB/PIC32MK', '/usr/mipsel-linux-gnu/include', <GCC_INTERNAL_INCLUDE>]`

**Step 9.** Create `firmware/src/config/config_mk.py`:
- `getSrcs()` → `self.getAllSrcsC() + self.findSrcs(module.SrcType.ASM)` — auto-discovers `crt0.S`, `port_asm_patched.S`, `libc_stubs.c`, `osal_freertos.c`

**Step 10.** Create `firmware/third_party/rtos/FreeRTOS/Source/freertos_mk.py`:
- `getSrcs()` → explicit list with paths updated to `firmware/third_party/...`: `FreeRTOS_tasks.c`, `list.c`, `queue.c`, `timers.c`, `event_groups.c`, `stream_buffer.c`, `portable/MemMang/heap_1.c`, `portable/MPLAB/PIC32MK/port.c`
- `getIncs()` → `['firmware/third_party/rtos/FreeRTOS/Source/include']`

**Step 11.** Create `firmware/APP_Firmware/APP_Firmware_mk.py`:
- `getSrcs()` → `self.getAllSrcsC()` — auto-discovers all `.c` files
- `getIncs()` → `['firmware/APP_Firmware']`

---

### Phase 7: Include Path Adjustments *(depends on Phase 6)*

**Step 12.** Audit `#include` paths across all copied files:
- Verify `main.c` includes resolve (`"plib_uart1.h"`, `"definitions.h"`, etc.)
- Verify `definitions.h` sub-includes work (`plib_evic.h`, `sys_int.h`, etc.)
- Verify `FreeRTOSConfig.h` constants match QEMU (`configPERIPHERAL_CLOCK_HZ=120000000`)
- Fix any broken include paths — **only adjust include directives**, never Harmony/FreeRTOS logic

---

### Phase 8: Build Validation *(depends on all above)*

**Step 13.** Run `cd tests/pic32mk_rtos_demo && pymake` — verify compilation succeeds
**Step 14.** Verify `Release/pic32mk_rtos_demo.elf` and `Release/pic32mk_rtos_demo.bin` exist
**Step 15.** Verify ELF sections: `mipsel-linux-gnu-objdump -h Release/pic32mk_rtos_demo.elf` — `.vectors` at 0xBFC40000, `.text` in flash, `.data`/`.bss` in RAM

---

### Phase 9: QEMU Runtime Validation *(depends on Phase 8)*

**Step 16.** Run on QEMU:
```
./build/qemu-system-mipsel -M pic32mk \
    -bios tests/pic32mk_rtos_demo/Release/pic32mk_rtos_demo.bin \
    -serial stdio -nographic -monitor none -d unimp,guest_errors
```
**Step 17.** Verify: "Hello from FreeRTOS!" output on UART1, Ping messages, Timer1 tick functional

---

### Relevant Files

| Role | Path | What to do |
|------|------|------------|
| **Source template** | `tests/freertos/Makefile` | Reference for compiler flags, file list |
| **Pymaketool template** | `PRG-IDU-0003/Makefile.py` | Adapt `Project` class pattern |
| **Pymaketool template** | `PRG-IDU-0003/firmware/src/app_mk.py` | Adapt `AppModule` pattern |
| **Pymaketool template** | `PRG-IDU-0003/firmware/src/config/config_mk.py` | Reuse `ConfigModule` auto-discovery |
| **Pymaketool template** | `PRG-IDU-0003/firmware/IDU_Firmware/IDU_Firmware_mk.py` | Adapt for `APP_Firmware_mk.py` |
| **Pymaketool template** | `PRG-IDU-0003/.../freertos_mk.py` | Adapt paths for `firmware/third_party/` |

---

### Verification

1. Every file from `tests/freertos/Makefile` `C_SRCS` + `S_SRCS` is covered by exactly one pymaketool module
2. `pymake` builds without errors
3. `objdump -h` shows correct section layout matching `hello_freertos.elf`
4. QEMU boot with `-d unimp,guest_errors` — no unexpected warnings
5. UART1 output matches existing demo behavior

---

### Further Considerations

1. **Linker invocation**: PRG-IDU-0003 uses `xc32-gcc` as linker. With GNU, we use `mipsel-linux-gnu-ld` directly. `getTargetsScript()` must call `LD` (not `CC`) — this differs from the reference template.

2. **GCC internal include path**: For `stdint.h`/`stddef.h` with `-nostdinc`, we need the GCC built-in include directory. Recommend resolving via `subprocess.check_output(['mipsel-linux-gnu-gcc', '-print-file-name=include'])` in `Makefile.py`.

3. **tests/freertos preservation**: The old project remains untouched — both coexist and produce equivalent binaries.
