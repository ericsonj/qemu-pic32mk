/*
 * Microchip PIC32MK GPK/MCM with CAN FD — board emulation
 * Datasheet: DS60001519E
 *
 * Phase 2: CPU core, EVIC, UART×6, Timers×9, GPIO A-G, SPI×6, I2C×4, DMA×8
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/log.h"
#include "qemu/datadir.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/core/boards.h"
#include "hw/core/loader.h"
#include "hw/core/clock.h"
#include "hw/core/qdev-properties.h"
#include "hw/mips/mips.h"
#include "hw/mips/pic32mk.h"
#include "hw/mips/pic32mk_evic.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "cpu.h"

/* Device type strings from our peripheral files */
#define TYPE_PIC32MK_UART   "pic32mk-uart"
#define TYPE_PIC32MK_TIMER  "pic32mk-timer"
#define TYPE_PIC32MK_GPIO   "pic32mk-gpio"
#define TYPE_PIC32MK_SPI    "pic32mk-spi"
#define TYPE_PIC32MK_I2C    "pic32mk-i2c"
#define TYPE_PIC32MK_DMA    "pic32mk-dma"

/*
 * Board state.
 */
typedef struct {
    MIPSCPU        *cpu;
    MemoryRegion    boot_rom;
    MemoryRegion    pflash;
    MemoryRegion    bflash1;
    MemoryRegion    bflash2;
    MemoryRegion    sfr;
    MemoryRegion    sfr_rcon;
    MemoryRegion    sfr_unimpl;

    DeviceState    *evic;
} PIC32MKState;

/* -----------------------------------------------------------------------
 * SFR catch-all stub — logs every unimplemented register access.
 * ----------------------------------------------------------------------- */

static uint64_t sfr_unimpl_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP,
                  "pic32mk: unimplemented SFR read  @ 0x%08" HWADDR_PRIx
                  " (size %u)\n",
                  (hwaddr)(PIC32MK_SFR_BASE + addr), size);
    return 0;
}

static void sfr_unimpl_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size)
{
    qemu_log_mask(LOG_UNIMP,
                  "pic32mk: unimplemented SFR write @ 0x%08" HWADDR_PRIx
                  " = 0x%08" PRIx64 " (size %u)\n",
                  (hwaddr)(PIC32MK_SFR_BASE + addr), val, size);
}

static const MemoryRegionOps sfr_unimpl_ops = {
    .read       = sfr_unimpl_read,
    .write      = sfr_unimpl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* -----------------------------------------------------------------------
 * RCON stub — returns POR|BOR on read so firmware sees a cold power-on reset.
 * ----------------------------------------------------------------------- */

static uint64_t rcon_read(void *opaque, hwaddr addr, unsigned size)
{
    return PIC32MK_RCON_POR | PIC32MK_RCON_BOR;
}

static void rcon_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    /* RSWRST trigger and RCON clear — stubbed */
}

static const MemoryRegionOps rcon_ops = {
    .read       = rcon_read,
    .write      = rcon_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* -----------------------------------------------------------------------
 * Helper: create a peripheral SysBusDevice, map its MMIO into the SFR
 * window at the given offset (overriding the catch-all at priority 1).
 * Returns the DeviceState for further property/IRQ wiring.
 * ----------------------------------------------------------------------- */

static DeviceState *sfr_device_create(MemoryRegion *sfr, const char *type,
                                      hwaddr sfr_offset, Error **errp)
{
    DeviceState *dev = qdev_new(type);
    if (!sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), errp)) {
        return NULL;
    }

    MemoryRegion *mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_add_subregion_overlap(sfr, sfr_offset, mr, 1);
    return dev;
}

/* -----------------------------------------------------------------------
 * Memory map initialisation
 * ----------------------------------------------------------------------- */

static void pic32mk_memory_init(PIC32MKState *s, MachineState *machine)
{
    MemoryRegion *sys_mem = get_system_memory();

    /* 256 KB SRAM */
    memory_region_add_subregion(sys_mem, PIC32MK_RAM_BASE, machine->ram);

    /* 1 MB Program Flash */
    memory_region_init_rom(&s->pflash, NULL, "pic32mk.pflash",
                           PIC32MK_PFLASH_SIZE, &error_fatal);
    memory_region_add_subregion(sys_mem, PIC32MK_PFLASH_BASE, &s->pflash);

    /* Boot Flash 1 — firmware loaded here via -bios */
    memory_region_init_rom(&s->bflash1, NULL, "pic32mk.bflash1",
                           PIC32MK_BFLASH1_SIZE, &error_fatal);
    memory_region_add_subregion(sys_mem, PIC32MK_BFLASH1_BASE, &s->bflash1);

    /* Boot Flash 2 */
    memory_region_init_rom(&s->bflash2, NULL, "pic32mk.bflash2",
                           PIC32MK_BFLASH2_SIZE, &error_fatal);
    memory_region_add_subregion(sys_mem, PIC32MK_BFLASH2_BASE, &s->bflash2);

    /*
     * Boot vector ROM — physical 0x1FC00000 to 0x1FC3FFFF.
     * Contains a two-instruction trampoline:
     *   j  0xBFC40000   (Boot Flash 1, KSEG1)
     *   nop             (branch delay slot)
     *
     * j-encoding when PC = 0xBFC00000:
     *   instr_index = (0xBFC40000 >> 2) & 0x3FFFFFF = 0x03F10000
     *   word = (2 << 26) | 0x03F10000 = 0x0BF10000
     */
    memory_region_init_rom(&s->boot_rom, NULL, "pic32mk.boot-rom",
                           PIC32MK_BOOTVEC_SIZE, &error_fatal);
    memory_region_add_subregion(sys_mem, PIC32MK_BOOTVEC_BASE, &s->boot_rom);
    {
        uint32_t *p = memory_region_get_ram_ptr(&s->boot_rom);
        p[0] = 0x0BF10000;  /* j 0xBFC40000 */
        p[1] = 0x00000000;  /* nop (delay slot) */
    }

    /* SFR window: 1 MB container */
    memory_region_init(&s->sfr, NULL, "pic32mk.sfr", PIC32MK_SFR_SIZE);
    memory_region_add_subregion(sys_mem, PIC32MK_SFR_BASE, &s->sfr);

    /* RCON stub at priority 1 (overrides catch-all) */
    memory_region_init_io(&s->sfr_rcon, NULL, &rcon_ops, s,
                          "pic32mk.rcon", 0x40);
    memory_region_add_subregion_overlap(&s->sfr, PIC32MK_RCON_OFFSET,
                                        &s->sfr_rcon, 1);

    /* Catch-all at priority 0 */
    memory_region_init_io(&s->sfr_unimpl, NULL, &sfr_unimpl_ops, s,
                          "pic32mk.sfr-unimpl", PIC32MK_SFR_SIZE);
    memory_region_add_subregion_overlap(&s->sfr, 0, &s->sfr_unimpl, 0);
}

/* -----------------------------------------------------------------------
 * CPU initialisation
 * ----------------------------------------------------------------------- */

static void pic32mk_cpu_init(PIC32MKState *s, MachineState *machine)
{
    Clock *cpuclk;

    cpuclk = clock_new(OBJECT(machine), "cpu-refclk");
    clock_set_hz(cpuclk, PIC32MK_CPU_HZ);

    s->cpu = mips_cpu_create_with_clock(machine->cpu_type, cpuclk, false);
    if (!s->cpu) {
        error_report("pic32mk: failed to create CPU '%s'", machine->cpu_type);
        exit(1);
    }

    /*
     * Allocate the 8 CPU interrupt lines (env->irq[0..7]).
     * Must be called before wiring EVIC to CPU pins.
     */
    cpu_mips_irq_init_cpu(s->cpu);
    cpu_mips_clock_init(s->cpu);
}

/* -----------------------------------------------------------------------
 * EVIC initialisation — create device, map MMIO, wire CPU pins
 * ----------------------------------------------------------------------- */

static void pic32mk_evic_init(PIC32MKState *s)
{
    DeviceState *evic = qdev_new(TYPE_PIC32MK_EVIC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(evic), &error_fatal);

    MemoryRegion *evic_mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(evic), 0);
    memory_region_add_subregion_overlap(&s->sfr, PIC32MK_EVIC_OFFSET,
                                        evic_mr, 1);
    s->evic = evic;

    /*
     * Wire EVIC output pins to CPU interrupt inputs.
     * The EVIC device stores these handles and calls qemu_set_irq()
     * when a pending+enabled interrupt is found at a given priority level.
     */
    PIC32MKEVICState *evic_s = PIC32MK_EVIC(evic);
    CPUMIPSState *env = &s->cpu->env;
    for (int i = 0; i < 8; i++) {
        evic_s->cpu_irq[i] = env->irq[i];
    }

    /*
     * Intercept the CP0 Core Timer's interrupt output (normally fires on
     * env->irq[7]) and redirect it through EVIC source 0 (CT = Core Timer).
     *
     * This makes CP0 timer interrupts subject to IEC0[0] (CTIE) and
     * IPC0 priority configuration, as on real hardware.
     *
     * cp0_timer.c calls: qemu_irq_raise(env->irq[IPTI & 7])
     * With IPTI=7 (M14K default, CP0_IntCtl = 0xe0000000), that is irq[7].
     * We replace irq[7] with the EVIC's irq_in[PIC32MK_IRQ_CT].
     */
    env->irq[7] = qdev_get_gpio_in(evic, PIC32MK_IRQ_CT);

    /*
     * Intercept CP0 Software Interrupts 0 and 1 (env->irq[0..1]) and
     * redirect them through the EVIC (sources CS0=1, CS1=2).
     *
     * On real PIC32MK hardware, writing Cause.IP0 triggers EVIC source
     * "Core Software Interrupt 0" (CS0) at whatever priority is configured
     * in IPC0.  The EVIC then delivers the interrupt through the normal
     * priority comparison, which prevents same-priority nesting.
     *
     * In QEMU VEIC mode the pending-vs-status comparison is
     *   (Cause & 0xFF00) > (Status & 0xFF00)
     * If IP0 (bit 8) is left in Cause while the tick ISR sets IPL=1
     * (bit 10), 0x0500 > 0x0400 causes an unwanted nested interrupt.
     *
     * The custom handler below:
     *   1) routes SW0/SW1 through the EVIC input lines, and
     *   2) clears the direct Cause.IP0/IP1 bit so the VEIC comparison
     *      only sees the EVIC-asserted priority pin (bit 10+).
     */
    env->irq[0] = qdev_get_gpio_in(evic, PIC32MK_IRQ_CS0);
    env->irq[1] = qdev_get_gpio_in(evic, PIC32MK_IRQ_CS1);

    /*
     * Store a reference to the CPU env in the EVIC state so the
     * evic_set_irq handler can clear the direct Cause.IP bits for
     * software interrupt sources routed through the EVIC.
     */
    evic_s->cpu = s->cpu;
}

/* -----------------------------------------------------------------------
 * Peripheral initialisation helpers
 * ----------------------------------------------------------------------- */

/*
 * Create a UART instance, attach a chardev, map into the SFR window,
 * and connect its RX/TX/error IRQ outputs to the EVIC input lines.
 */
static void pic32mk_uart_create(PIC32MKState *s, int index,
                                hwaddr sfr_offset,
                                int irq_rx, int irq_tx, int irq_err,
                                Chardev *chr)
{
    DeviceState *dev = qdev_new(TYPE_PIC32MK_UART);
    if (chr) {
        qdev_prop_set_chr(dev, "chardev", chr);
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    MemoryRegion *mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_add_subregion_overlap(&s->sfr, sfr_offset, mr, 1);

    /* Connect UART IRQ outputs → EVIC inputs */
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(s->evic, irq_rx));
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1,
                       qdev_get_gpio_in(s->evic, irq_tx));
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 2,
                       qdev_get_gpio_in(s->evic, irq_err));
}

/*
 * Create a Timer instance, map into the SFR window, connect IRQ to EVIC.
 */
static void pic32mk_timer_create(PIC32MKState *s,
                                 hwaddr sfr_offset, int irq_src, bool type_a)
{
    DeviceState *dev = qdev_new(TYPE_PIC32MK_TIMER);
    qdev_prop_set_bit(dev, "type-a", type_a);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    memory_region_add_subregion_overlap(&s->sfr, sfr_offset,
                                        sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0), 1);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(s->evic, irq_src));
}

/*
 * Create a GPIO port instance, map into the SFR window.
 * (No EVIC connection — CN interrupts are Phase 2B.)
 */
static void pic32mk_gpio_create(PIC32MKState *s, hwaddr sfr_offset)
{
    sfr_device_create(&s->sfr, TYPE_PIC32MK_GPIO, sfr_offset, &error_fatal);
}

/*
 * Create a SPI instance, map into the SFR window.
 */
static void pic32mk_spi_create(PIC32MKState *s, hwaddr sfr_offset)
{
    sfr_device_create(&s->sfr, TYPE_PIC32MK_SPI, sfr_offset, &error_fatal);
}

/*
 * Create an I2C instance, map into the SFR window.
 */
static void pic32mk_i2c_create(PIC32MKState *s, hwaddr sfr_offset)
{
    sfr_device_create(&s->sfr, TYPE_PIC32MK_I2C, sfr_offset, &error_fatal);
}

/* -----------------------------------------------------------------------
 * Firmware loading
 * ----------------------------------------------------------------------- */

static void pic32mk_load_firmware(MachineState *machine)
{
    if (!machine->firmware) {
        return;
    }

    char *filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->firmware);
    if (!filename) {
        error_report("pic32mk: could not find firmware '%s'",
                     machine->firmware);
        exit(1);
    }

    ssize_t bios_size = load_image_targphys(filename,
                                            PIC32MK_BFLASH1_BASE,
                                            PIC32MK_BFLASH1_SIZE,
                                            NULL);
    g_free(filename);

    if (bios_size < 0) {
        error_report("pic32mk: could not load firmware '%s'",
                     machine->firmware);
        exit(1);
    }
}

/* -----------------------------------------------------------------------
 * Machine entry point
 * ----------------------------------------------------------------------- */

static void pic32mk_machine_init(MachineState *machine)
{
    PIC32MKState *s = g_new0(PIC32MKState, 1);

    pic32mk_cpu_init(s, machine);
    pic32mk_memory_init(s, machine);

    /* EVIC — must come before peripherals so we can wire IRQs */
    pic32mk_evic_init(s);

    /* DMA — mapped into the EVIC's 4 KB page (DMA_OFFSET = EVIC+0x1000) */
    {
        DeviceState *dma = qdev_new(TYPE_PIC32MK_DMA);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dma), &error_fatal);
        MemoryRegion *mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dma), 0);
        memory_region_add_subregion_overlap(&s->sfr, PIC32MK_DMA_OFFSET, mr, 1);
        for (int i = 0; i < PIC32MK_DMA_NCHANNELS; i++) {
            sysbus_connect_irq(SYS_BUS_DEVICE(dma), i,
                               qdev_get_gpio_in(s->evic,
                                                PIC32MK_IRQ_DMA0 + i));
        }
    }

    /* UART1 on serial_hd(0), UART2–6 without chardev (stub only) */
    pic32mk_uart_create(s, 1, PIC32MK_UART1_OFFSET,
                        PIC32MK_IRQ_U1RX, PIC32MK_IRQ_U1TX, PIC32MK_IRQ_U1E,
                        serial_hd(0));
    pic32mk_uart_create(s, 2, PIC32MK_UART2_OFFSET,
                        PIC32MK_IRQ_U2RX, PIC32MK_IRQ_U2TX, PIC32MK_IRQ_U2E,
                        serial_hd(1));
    pic32mk_uart_create(s, 3, PIC32MK_UART3_OFFSET,
                        PIC32MK_IRQ_U3RX, PIC32MK_IRQ_U3TX, PIC32MK_IRQ_U3E,
                        serial_hd(2));
    pic32mk_uart_create(s, 4, PIC32MK_UART4_OFFSET,
                        PIC32MK_IRQ_U4RX, PIC32MK_IRQ_U4TX, PIC32MK_IRQ_U4E,
                        NULL);
    pic32mk_uart_create(s, 5, PIC32MK_UART5_OFFSET,
                        PIC32MK_IRQ_U5RX, PIC32MK_IRQ_U5TX, PIC32MK_IRQ_U5E,
                        NULL);
    pic32mk_uart_create(s, 6, PIC32MK_UART6_OFFSET,
                        PIC32MK_IRQ_U6RX, PIC32MK_IRQ_U6TX, PIC32MK_IRQ_U6E,
                        NULL);

    /* Timers 1–9: Timer1 is Type A (2-bit TCKPS {1,8,64,256}); 2–9 are Type B/C */
    pic32mk_timer_create(s, PIC32MK_T1_OFFSET, PIC32MK_IRQ_T1, true);
    pic32mk_timer_create(s, PIC32MK_T2_OFFSET, PIC32MK_IRQ_T2, false);
    pic32mk_timer_create(s, PIC32MK_T3_OFFSET, PIC32MK_IRQ_T3, false);
    pic32mk_timer_create(s, PIC32MK_T4_OFFSET, PIC32MK_IRQ_T4, false);
    pic32mk_timer_create(s, PIC32MK_T5_OFFSET, PIC32MK_IRQ_T5, false);
    pic32mk_timer_create(s, PIC32MK_T6_OFFSET, PIC32MK_IRQ_T6, false);
    pic32mk_timer_create(s, PIC32MK_T7_OFFSET, PIC32MK_IRQ_T7, false);
    pic32mk_timer_create(s, PIC32MK_T8_OFFSET, PIC32MK_IRQ_T8, false);
    pic32mk_timer_create(s, PIC32MK_T9_OFFSET, PIC32MK_IRQ_T9, false);

    /* GPIO ports A–G */
    for (int port = 0; port < PIC32MK_GPIO_NPORTS; port++) {
        pic32mk_gpio_create(s, PIC32MK_GPIO_OFFSET
                              + (hwaddr)port * PIC32MK_GPIO_PORT_SIZE);
    }

    /* SPI 1–6 */
    pic32mk_spi_create(s, PIC32MK_SPI1_OFFSET);
    pic32mk_spi_create(s, PIC32MK_SPI2_OFFSET);
    pic32mk_spi_create(s, PIC32MK_SPI3_OFFSET);
    pic32mk_spi_create(s, PIC32MK_SPI4_OFFSET);
    pic32mk_spi_create(s, PIC32MK_SPI5_OFFSET);
    pic32mk_spi_create(s, PIC32MK_SPI6_OFFSET);

    /* I2C 1–4 */
    pic32mk_i2c_create(s, PIC32MK_I2C1_OFFSET);
    pic32mk_i2c_create(s, PIC32MK_I2C2_OFFSET);
    pic32mk_i2c_create(s, PIC32MK_I2C3_OFFSET);
    pic32mk_i2c_create(s, PIC32MK_I2C4_OFFSET);

    pic32mk_load_firmware(machine);
}

/* -----------------------------------------------------------------------
 * MachineClass registration
 * ----------------------------------------------------------------------- */

static void pic32mk_machine_class_init(MachineClass *mc)
{
    mc->desc           = "Microchip PIC32MK GPK/MCM with CAN FD";
    mc->init           = pic32mk_machine_init;
    mc->max_cpus       = 1;
    /*
     * microAptiv: MIPS32r2 + FPU + DSP R2 + microMIPS + MCU ASE,
     * fixed-mapping MMU, VEIC — matches PIC32MK DS60001519E §3.
     */
    mc->default_cpu_type = MIPS_CPU_TYPE_NAME("microAptiv");
    mc->default_ram_id   = "pic32mk.ram";
    mc->default_ram_size = PIC32MK_RAM_SIZE;
    mc->no_parallel      = 1;
    mc->no_floppy        = 1;
    mc->no_cdrom         = 1;
}

DEFINE_MACHINE("pic32mk", pic32mk_machine_class_init)
