/*
 * PIC32MK SPI × 6 (SPI1–SPI6)
 * Datasheet: DS60001519E, §23
 *
 * Each SPI instance is a SysBusDevice providing a stub register file.
 * Full transfer emulation (loopback, slave select, IRQs) is Phase 2B.
 * Firmware doing register-level init (SPIxCON, SPIxBRG) will succeed;
 * actual data transfers log LOG_UNIMP.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "hw/mips/pic32mk.h"

#define TYPE_PIC32MK_SPI    "pic32mk-spi"
OBJECT_DECLARE_SIMPLE_TYPE(PIC32MKSpiState, PIC32MK_SPI)

struct PIC32MKSpiState {
    SysBusDevice parent_obj;
    MemoryRegion mr;

    uint32_t con;   /* SPIxCON */
    uint32_t stat;  /* SPIxSTAT */
    uint32_t buf;   /* SPIxBUF — TX write / RX read */
    uint32_t brg;   /* SPIxBRG */
    uint32_t con2;  /* SPIxCON2 */

    qemu_irq irq;   /* connect to EVIC */
};

/* STAT bits */
#define SPITBE  (1u << 3)   /* TX buffer empty */
#define SPIRBF  (1u << 0)   /* RX buffer full */

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

static uint32_t *spi_find_reg(PIC32MKSpiState *s, hwaddr base)
{
    switch (base) {
    case PIC32MK_SPIxCON:  return &s->con;
    case PIC32MK_SPIxSTAT: return &s->stat;
    case PIC32MK_SPIxBRG:  return &s->brg;
    case PIC32MK_SPIxCON2: return &s->con2;
    default:               return NULL;
    }
}

static uint64_t spi_read(void *opaque, hwaddr addr, unsigned size)
{
    PIC32MKSpiState *s = opaque;
    hwaddr base = addr & ~(hwaddr)0xF;

    /* BUF read: loopback — return last written byte, clear SPIRBF */
    if (base == PIC32MK_SPIxBUF) {
        uint32_t val = s->buf;
        s->stat &= ~SPIRBF;
        s->stat |= SPITBE;
        return val;
    }

    uint32_t *reg = spi_find_reg(s, base);
    if (reg) {
        return *reg;
    }

    qemu_log_mask(LOG_UNIMP,
                  "pic32mk_spi: unimplemented read @ 0x%04" HWADDR_PRIx "\n",
                  addr);
    return 0;
}

static void spi_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    PIC32MKSpiState *s = opaque;
    int sub     = (int)(addr & 0xF);
    hwaddr base = addr & ~(hwaddr)0xF;

    /* BUF write: loopback — store TX data as RX data, set SPIRBF */
    if (base == PIC32MK_SPIxBUF) {
        s->buf   = (uint32_t)val;
        s->stat |= SPIRBF;
        s->stat &= ~SPITBE;
        qemu_log_mask(LOG_UNIMP,
                      "pic32mk_spi: TX write 0x%08" PRIx64
                      " (loopback only)\n", val);
        return;
    }

    uint32_t *reg = spi_find_reg(s, base);
    if (reg) {
        apply_sci(reg, (uint32_t)val, sub);
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "pic32mk_spi: unimplemented write @ 0x%04"
                  HWADDR_PRIx " = 0x%08" PRIx64 "\n",
                  addr, val);
}

static const MemoryRegionOps spi_ops = {
    .read       = spi_read,
    .write      = spi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void pic32mk_spi_reset(DeviceState *dev)
{
    PIC32MKSpiState *s = PIC32MK_SPI(dev);
    s->con  = 0;
    s->stat = SPITBE;   /* TX buffer empty on reset */
    s->buf  = 0;
    s->brg  = 0;
    s->con2 = 0;
    qemu_irq_lower(s->irq);
}

static void pic32mk_spi_init(Object *obj)
{
    PIC32MKSpiState *s = PIC32MK_SPI(obj);

    memory_region_init_io(&s->mr, obj, &spi_ops, s,
                          TYPE_PIC32MK_SPI, PIC32MK_SPI_BLOCK_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mr);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
}

static void pic32mk_spi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_legacy_reset(dc, pic32mk_spi_reset);
}

static const TypeInfo pic32mk_spi_info = {
    .name          = TYPE_PIC32MK_SPI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PIC32MKSpiState),
    .instance_init = pic32mk_spi_init,
    .class_init    = pic32mk_spi_class_init,
};

static void pic32mk_spi_register_types(void)
{
    type_register_static(&pic32mk_spi_info);
}

type_init(pic32mk_spi_register_types)
