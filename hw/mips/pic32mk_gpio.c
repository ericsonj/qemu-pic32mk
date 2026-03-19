/*
 * PIC32MK GPIO — PORTA through PORTG (7 ports)
 * Datasheet: DS60001519E, §12
 *
 * Each port is a SysBusDevice that exposes a PIC32MK_GPIO_PORT_SIZE (0x100)
 * byte MMIO region containing ANSEL, TRIS, PORT, LAT, ODC, CN* registers.
 *
 * All registers support SET/CLR/INV sub-registers (+4/+8/+C).
 * Output pins are not yet driven to a QEMU GPIO bus; this is a Phase 2B stub.
 * PPS (Peripheral Pin Select) writes are silently accepted.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/core/sysbus.h"
#include "hw/mips/pic32mk.h"

/* -----------------------------------------------------------------------
 * Device state — one instance per port (A–G)
 * ----------------------------------------------------------------------- */

#define TYPE_PIC32MK_GPIO   "pic32mk-gpio"
OBJECT_DECLARE_SIMPLE_TYPE(PIC32MKGpioState, PIC32MK_GPIO)

struct PIC32MKGpioState {
    SysBusDevice parent_obj;
    MemoryRegion mr;

    uint32_t ansel;
    uint32_t tris;    /* 1 = input (reset default) */
    uint32_t port;    /* read pin state */
    uint32_t lat;     /* latch (output) */
    uint32_t odc;
    uint32_t cnpu;
    uint32_t cnpd;
    uint32_t cncon;
    uint32_t cnen0;
    uint32_t cnstat;
    uint32_t cnen1;
    uint32_t cnf;
};

/* -----------------------------------------------------------------------
 * MMIO helpers
 * ----------------------------------------------------------------------- */

/* PIC32MK: base+0=REG, +4=CLR, +8=SET, +0xC=INV */
static void apply_sci(uint32_t *reg, uint32_t val, int sub)
{
    switch (sub) {
    case 0:  *reg  = val; break;
    case 4:  *reg &= ~val; break;
    case 8:  *reg |= val; break;
    case 12: *reg ^= val; break;
    }
}

static uint32_t *gpio_find_reg(PIC32MKGpioState *s, hwaddr base)
{
    switch (base) {
    case PIC32MK_ANSEL:  return &s->ansel;
    case PIC32MK_TRIS:   return &s->tris;
    case PIC32MK_PORT:   return &s->port;
    case PIC32MK_LAT:    return &s->lat;
    case PIC32MK_ODC:    return &s->odc;
    case PIC32MK_CNPU:   return &s->cnpu;
    case PIC32MK_CNPD:   return &s->cnpd;
    case PIC32MK_CNCON:  return &s->cncon;
    case PIC32MK_CNEN0:  return &s->cnen0;
    case PIC32MK_CNSTAT: return &s->cnstat;
    case PIC32MK_CNEN1:  return &s->cnen1;
    case PIC32MK_CNF:    return &s->cnf;
    default:             return NULL;
    }
}

/* -----------------------------------------------------------------------
 * MMIO read/write
 * ----------------------------------------------------------------------- */

static uint64_t gpio_read(void *opaque, hwaddr addr, unsigned size)
{
    PIC32MKGpioState *s = opaque;
    hwaddr base = addr & ~(hwaddr)0xF;
    uint32_t *reg = gpio_find_reg(s, base);

    if (reg) {
        /* PORT reads physical pin state; we return LAT since we have no
         * external GPIO model yet */
        if (base == PIC32MK_PORT) {
            return s->lat & ~s->tris;   /* output bits reflect LAT */
        }
        return *reg;
    }

    qemu_log_mask(LOG_UNIMP,
                  "pic32mk_gpio: unimplemented read @ 0x%04" HWADDR_PRIx "\n",
                  addr);
    return 0;
}

static void gpio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    PIC32MKGpioState *s = opaque;
    int sub       = (int)(addr & 0xF);
    hwaddr base   = addr & ~(hwaddr)0xF;
    uint32_t *reg = gpio_find_reg(s, base);

    if (reg) {
        apply_sci(reg, (uint32_t)val, sub);
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "pic32mk_gpio: unimplemented write @ 0x%04"
                  HWADDR_PRIx " = 0x%08" PRIx64 "\n",
                  addr, val);
}

static const MemoryRegionOps gpio_ops = {
    .read       = gpio_read,
    .write      = gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* -----------------------------------------------------------------------
 * Device lifecycle
 * ----------------------------------------------------------------------- */

static void pic32mk_gpio_reset(DeviceState *dev)
{
    PIC32MKGpioState *s = PIC32MK_GPIO(dev);

    s->ansel  = 0xFFFF;   /* all pins analog on reset */
    s->tris   = 0xFFFF;   /* all pins input on reset */
    s->port   = 0;
    s->lat    = 0;
    s->odc    = 0;
    s->cnpu   = 0;
    s->cnpd   = 0;
    s->cncon  = 0;
    s->cnen0  = 0;
    s->cnstat = 0;
    s->cnen1  = 0;
    s->cnf    = 0;
}

static void pic32mk_gpio_init(Object *obj)
{
    PIC32MKGpioState *s = PIC32MK_GPIO(obj);

    memory_region_init_io(&s->mr, obj, &gpio_ops, s,
                          TYPE_PIC32MK_GPIO, PIC32MK_GPIO_PORT_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mr);
}

static void pic32mk_gpio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_legacy_reset(dc, pic32mk_gpio_reset);
}

static const TypeInfo pic32mk_gpio_info = {
    .name          = TYPE_PIC32MK_GPIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PIC32MKGpioState),
    .instance_init = pic32mk_gpio_init,
    .class_init    = pic32mk_gpio_class_init,
};

static void pic32mk_gpio_register_types(void)
{
    type_register_static(&pic32mk_gpio_info);
}

type_init(pic32mk_gpio_register_types)
