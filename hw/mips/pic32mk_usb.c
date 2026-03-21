/*
 * Microchip PIC32MK USB OTG Full-Speed controller emulation
 * Datasheet: DS60001519E §25
 * Register addresses verified against p32mk1024mcm100.h (XC32 v4.60 pack).
 *
 * Phase 4A — register-file stub (complete)
 * Phase 4B — USB enumeration simulation + CDC TX chardev output:
 *   • QEMUTimer drives the EP0 state machine (USB Reset → enumerate)
 *   • SETUP packet injection via BDT write + TRNIF interrupt
 *   • CDC TX polling: EP1-IN BDT entry harvested → chardev write
 *   • chardev property "chardev" exposes CDC output as host PTY/socket
 *
 * BDT layout (PIC32MK device mode, ping-pong off / PPBRST):
 *   Each endpoint has 4 BDT entries (RX-even, RX-odd, TX-even, TX-odd).
 *   Entry size = 8 bytes (4-byte ctrl word + 4-byte buffer address).
 *   EP0-OUT-even = offset 0x00, EP0-IN-even = offset 0x10,
 *   EP1-OUT-even = offset 0x20, EP1-IN-even = offset 0x30.
 *
 * Copyright (c) 2026 QEMU contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "hw/mips/pic32mk.h"
#include "hw/mips/pic32mk_usb.h"
#include "exec/cpu-common.h"   /* cpu_physical_memory_read/write */
#include "chardev/char-fe.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/*
 * Apply SET / CLR / INV operation.
 * sub = addr & 0xF: 0 → write, 4 → CLR, 8 → SET, C → INV
 */
static void apply_sci(uint32_t *reg, uint32_t val, int sub)
{
    switch (sub) {
    case 0x0: *reg  = val;  break;
    case 0x4: *reg &= ~val; break;
    case 0x8: *reg |= val;  break;
    case 0xC: *reg ^= val;  break;
    }
}

/*
 * Apply write-1-clear (W1C) operation.
 */
static void apply_w1c(uint32_t *reg, uint32_t val, int sub)
{
    if (sub == 0x0 || sub == 0x4) {
        *reg &= ~val;
    }
}

static void usb_update_irq(PIC32MKUSBState *s)
{
    bool fire = ((s->uir  & s->uie)  != 0)
             || ((s->ueir & s->ueie) != 0)
             || ((s->otgir & s->otgie) != 0);
    qemu_set_irq(s->irq, fire ? 1 : 0);
}

/* -----------------------------------------------------------------------
 * BDT helpers
 * ----------------------------------------------------------------------- */

static hwaddr usb_bdt_phys(PIC32MKUSBState *s)
{
    /* BDT base = (BDTP3[7:0] << 24) | (BDTP2[7:0] << 16) | (BDTP1[7:1] << 8)
     * BDTP1 bits [7:1] map to BDT address bits [15:9]. */
    return ((hwaddr)(s->bdtp3 & 0xFFu) << 24)
         | ((hwaddr)(s->bdtp2 & 0xFFu) << 16)
         | ((hwaddr)(s->bdtp1 & 0xFEu) << 8);
}

/*
 * Inject an 8-byte SETUP packet into the EP0-OUT-even BDT buffer.
 * The BDT entry at offset 0x00 from BDT base must be UOWN=1 (firmware armed).
 * On success: writes the SETUP data to the buffer, clears UOWN, sets
 * TOK_PID=SETUP (0xD) in byte[0] bits[5:2]=0x34, bc=8 in shortWord[1]
 * (bits[31:16]), sets ustat=EP0/OUT/EVEN, fires TRNIF.
 *
 * BDT 32-bit control word layout (Harmony DRV_USBFS_BDT_ENTRY union):
 *   bits[ 7: 0] = byte[0]:  UOWN(7), DATA01(6), DTSEN(4), BSTALL(2), TOK_PID[5:2]
 *   bits[15: 8] = byte[1]:  (reserved / padding)
 *   bits[31:16] = shortWord[1]: byte count (BC)
 *   word[1]                  = buffer physical address
 *
 * Harmony TRNIF switch uses (byte[0] & 0x3C):
 *   0x34 → SETUP (TOK_PID=0xD)
 *   0x04 → OUT   (TOK_PID=0x1)
 *   0x24 → IN    (TOK_PID=0x9)
 */
static bool usb_inject_setup(PIC32MKUSBState *s, const uint8_t setup[8])
{
    hwaddr bdt = usb_bdt_phys(s);
    if (!bdt) {
        return false;
    }

    /* Check both ping-pong entries for EP0-OUT (even=BDT+0, odd=BDT+8). */
    for (int ppbi = 0; ppbi < 2; ppbi++) {
        hwaddr out_entry = bdt + (ppbi ? 8u : 0u);
        uint8_t entry[8];
        cpu_physical_memory_read(out_entry, entry, 8);

        uint32_t ctrl = le32_to_cpu(*(uint32_t *)entry);
        uint32_t addr = le32_to_cpu(*(uint32_t *)(entry + 4));

        if (!(ctrl & BDT_UOWN) || !addr) {
            continue;   /* not armed — try other ping-pong */
        }

        /* Write SETUP packet into the EP0-OUT buffer */
        cpu_physical_memory_write(addr, setup, 8);

        /*
         * Return BDT entry to firmware:
         *   byte[0] = 0x34: TOK_PID=SETUP(0xD) in bits[5:2], UOWN=0
         *   shortWord[1] = 8: byte count
         */
        ctrl = 0x00000034u | (8u << 16);
        *(uint32_t *)entry = cpu_to_le32(ctrl);
        cpu_physical_memory_write(out_entry, entry, 8);

        /* UxSTAT: EP=0, DIR=OUT(0), PPBI=ppbi */
        s->ustat = (uint32_t)(ppbi ? 0x04u : 0x00u);
        s->uir  |= USB_IR_TRNIF;
        usb_update_irq(s);
        return true;
    }
    return false;   /* neither entry armed yet */
}

/*
 * Accept an EP0-IN response from the firmware: read the data the firmware
 * placed in EP0-IN-even (BDT offset 0x10), set TOK_PID=IN in byte[0],
 * clear UOWN, preserve bc in shortWord[1], fire TRNIF.
 * Returns true if an IN entry was accepted, false if not armed yet.
 */
static bool usb_accept_ep0_in(PIC32MKUSBState *s)
{
    hwaddr bdt = usb_bdt_phys(s);
    if (!bdt) {
        return false;
    }

    /* Check both ping-pong entries for EP0-IN (even=BDT+0x10, odd=BDT+0x18). */
    for (int ppbi = 0; ppbi < 2; ppbi++) {
        hwaddr in_entry = bdt + 0x10u + (ppbi ? 8u : 0u);
        uint8_t entry[8];
        cpu_physical_memory_read(in_entry, entry, 8);

        uint32_t ctrl = le32_to_cpu(*(uint32_t *)entry);
        if (!(ctrl & BDT_UOWN)) {
            continue;   /* not armed */
        }

        /*
         * Return BDT entry to firmware:
         *   byte[0] = 0x24: TOK_PID=IN(0x9), UOWN=0
         *   shortWord[1]: preserve bc
         */
        ctrl = (ctrl & 0xFFFF0000u) | 0x24u;
        *(uint32_t *)entry = cpu_to_le32(ctrl);
        cpu_physical_memory_write(in_entry, entry, 8);

        /* UxSTAT: EP=0, DIR=IN(bit3), PPBI=ppbi(bit2) */
        s->ustat = 0x08u | (uint32_t)(ppbi ? 0x04u : 0x00u);
        s->uir  |= USB_IR_TRNIF;
        usb_update_irq(s);
        return true;
    }
    return false;
}

/*
 * Simulate the STATUS phase OUT for a control read (host→device ZLP).
 * Write EP0-OUT-even BDT entry: TOK_PID=OUT(0x1)→byte[0]=0x04, UOWN=0,
 * bc=0 in shortWord[1].  Then fire TRNIF so the Harmony TRNIF handler
 * hits case 0x04, sees shortWord[1]=0 < maxPacketSize, marks the IRP
 * complete, invokes the callback, and re-arms EP0-OUT for the next SETUP.
 */
static void usb_send_status_out(PIC32MKUSBState *s)
{
    hwaddr bdt = usb_bdt_phys(s);
    if (!bdt) {
        return;
    }

    /*
     * Find the armed EP0-OUT ping-pong entry (even=BDT+0, odd=BDT+8).
     * Write: byte[0]=0x04 (TOK_PID=OUT), UOWN=0, shortWord[1]=0 (ZLP).
     */
    for (int ppbi = 0; ppbi < 2; ppbi++) {
        hwaddr out_entry = bdt + (ppbi ? 8u : 0u);
        uint8_t entry[8];
        cpu_physical_memory_read(out_entry, entry, 8);
        uint32_t ctrl = le32_to_cpu(*(uint32_t *)entry);

        if (ctrl & BDT_UOWN) {
            ctrl = 0x00000004u;   /* TOK_PID=OUT, UOWN=0, bc=0 */
            *(uint32_t *)entry = cpu_to_le32(ctrl);
            cpu_physical_memory_write(out_entry, entry, 8);
            s->ustat = (uint32_t)(ppbi ? 0x04u : 0x00u);  /* PPBI in bit 2 */
            s->uir  |= USB_IR_TRNIF;
            usb_update_irq(s);
            return;
        }
    }

    /* Neither armed — fire even anyway (fallback; should not happen) */
    uint8_t entry[8];
    cpu_physical_memory_read(bdt, entry, 8);
    *(uint32_t *)entry = cpu_to_le32(0x00000004u);
    cpu_physical_memory_write(bdt, entry, 8);
    s->ustat = USB_STAT_EP0_OUT_EVEN;
    s->uir  |= USB_IR_TRNIF;
    usb_update_irq(s);
}

/*
 * Poll the CDC TX endpoint (EP1-IN-even, BDT offset 0x30).
 * If the firmware has queued data (UOWN=1), drain it to the chardev,
 * release the BDT entry, and fire TRNIF so the firmware can refill.
 */
static void usb_check_cdc_bdt(PIC32MKUSBState *s)
{
    hwaddr bdt = usb_bdt_phys(s);
    if (!bdt) {
        return;
    }

    /*
     * CDC TX = EP2-IN (bulk).  BDT layout: EP n occupies 4 × 8-byte entries
     * starting at BDT + n × 32.  EP2 base = BDT + 0x40.
     *   EP2-IN-even = BDT+0x50, EP2-IN-odd = BDT+0x58.
     * Check both ping-pong entries.
     */
    for (int ppbi = 0; ppbi < 2; ppbi++) {
        hwaddr ep2_in = bdt + 0x50u + (ppbi ? 8u : 0u);
        uint8_t entry[8];
        cpu_physical_memory_read(ep2_in, entry, 8);

        uint32_t ctrl = le32_to_cpu(*(uint32_t *)entry);
        if (!(ctrl & BDT_UOWN)) {
            continue;
        }

        uint32_t bc   = ctrl >> 16;
        uint32_t addr = le32_to_cpu(*(uint32_t *)(entry + 4));

        if (bc > 0 && addr) {
            uint8_t buf[64];
            bc = MIN(bc, sizeof(buf));
            cpu_physical_memory_read(addr, buf, bc);
            if (qemu_chr_fe_backend_connected(&s->chr)) {
                qemu_chr_fe_write_all(&s->chr, buf, bc);
            }
        }

        /* Return BDT entry: TOK_PID=IN, UOWN=0, preserve bc */
        ctrl = (ctrl & 0xFFFF0000u) | 0x24u;
        *(uint32_t *)entry = cpu_to_le32(ctrl);
        cpu_physical_memory_write(ep2_in, entry, 8);

        /* UxSTAT: EP=2(bits[7:4]=0x20), DIR=IN(bit3=0x08), PPBI=ppbi(bit2) */
        s->ustat = 0x28u | (uint32_t)(ppbi ? 0x04u : 0x00u);
        s->uir  |= USB_IR_TRNIF;
        usb_update_irq(s);
        return;   /* only drain one packet per 5 ms tick */
    }
}

/* -----------------------------------------------------------------------
 * EP0 enumeration timer
 * ----------------------------------------------------------------------- */

static void usb_timer_cb(void *opaque)
{
    PIC32MKUSBState *s = opaque;

    /* Standard USB SETUP packets for enumeration sequence */
    static const uint8_t setup_get_dev_desc[8] = {
        0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 18, 0x00
    };  /* GET_DESCRIPTOR(Device, length=18) */

    static const uint8_t setup_set_address[8] = {
        0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
    };  /* SET_ADDRESS(1) */

    static const uint8_t setup_get_cfg_desc[8] = {
        0x80, 0x06, 0x00, 0x02, 0x00, 0x00, 67, 0x00
    };  /* GET_DESCRIPTOR(Configuration, length=67) */

    static const uint8_t setup_set_config[8] = {
        0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
    };  /* SET_CONFIGURATION(1) */

    switch (s->ep0_sim) {

    case EP0_SIM_IDLE:
        /* Do nothing until USBPWR is set */
        break;

    case EP0_SIM_RESET:
        /* Simulate VBUS present: set UxOTGSTAT.SESVD (bit 3) so that
         * PLIB_USB_OTG_SessionValid() returns true.  Fire the SESSION_VALID
         * OTG interrupt (USB_OTG_INT_SESSION_VALID = 0x08) so the Harmony
         * ISR sets hDriver->vbusIsValid = true before it processes URSTIF.
         * Without this the ISR takes the early exit at line:
         *   if (!vbusIsValid || !isAttached) { clearAllFlags(); return; }
         * and never reaches the Reset handler. */
        s->otgstat |= 0x08u;   /* SESVD: VBUS > V_A_SESS_VLD (session valid) */
        s->otgir   |= 0x08u;   /* USB_OTG_INT_SESSION_VALID */
        /* Fire USB Reset — firmware handles URSTIF */
        s->uir |= USB_IR_URSTIF;
        usb_update_irq(s);
        s->ep0_sim = EP0_SIM_GET_DEV_DESC;
        timer_mod(s->usb_timer,
                  qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 10 * SCALE_MS);
        return;

    case EP0_SIM_GET_DEV_DESC:
        if (usb_inject_setup(s, setup_get_dev_desc)) {
            s->ep0_sim = EP0_SIM_WAIT_DEV_DESC;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        } else {
            /* Retry every 2 ms until BDT is armed */
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        }
        return;

    case EP0_SIM_WAIT_DEV_DESC:
        /* Consume the EP0-IN response(s) — may be multi-packet (64+3 bytes) */
        if (usb_accept_ep0_in(s)) {
            /* Give firmware 2ms to re-arm IN or finish */
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        } else {
            /* IN response not ready yet, or we just consumed last packet */
            usb_send_status_out(s);  /* status phase complete */
            s->ep0_sim = EP0_SIM_SET_ADDRESS;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        }
        return;

    case EP0_SIM_SET_ADDRESS:
        if (usb_inject_setup(s, setup_set_address)) {
            s->uaddr = 1;   /* pretend host acknowledged new address */
            s->ep0_sim = EP0_SIM_WAIT_ADDRESS;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        } else {
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        }
        return;

    case EP0_SIM_WAIT_ADDRESS:
        /* Consume status-phase ZLP IN from SET_ADDRESS */
        if (usb_accept_ep0_in(s)) {
            s->ep0_sim = EP0_SIM_GET_CFG_DESC;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        } else {
            /* Retry */
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        }
        return;

    case EP0_SIM_GET_CFG_DESC:
        if (usb_inject_setup(s, setup_get_cfg_desc)) {
            s->ep0_sim = EP0_SIM_WAIT_CFG_DESC;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        } else {
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        }
        return;

    case EP0_SIM_WAIT_CFG_DESC:
        /* May be multi-packet; keep accepting until no more IN pending */
        if (usb_accept_ep0_in(s)) {
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        } else {
            usb_send_status_out(s);  /* status phase */
            s->ep0_sim = EP0_SIM_SET_CONFIG;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        }
        return;

    case EP0_SIM_SET_CONFIG:
        if (usb_inject_setup(s, setup_set_config)) {
            s->ep0_sim = EP0_SIM_WAIT_CONFIG;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        } else {
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        }
        return;

    case EP0_SIM_WAIT_CONFIG:
        /* Consume status-phase ZLP IN from SET_CONFIGURATION */
        if (usb_accept_ep0_in(s)) {
            s->configured = true;
            s->ep0_sim = EP0_SIM_DONE;
            qemu_log_mask(LOG_UNIMP, "pic32mk_usb: USB enumeration DONE\n");
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        } else {
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 2 * SCALE_MS);
        }
        return;

    case EP0_SIM_DONE:
        /* Poll CDC TX every 5 ms */
        usb_check_cdc_bdt(s);
        /* Advance the frame counter (5 frames per poll) */
        {
            uint32_t frm = ((s->ufrmh & 0x07u) << 8) | s->ufrml;
            frm = (frm + 5) & 0x7FFu;
            s->ufrml = frm & 0xFFu;
            s->ufrmh = (frm >> 8) & 0x07u;
        }
        timer_mod(s->usb_timer,
                  qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 5 * SCALE_MS);
        return;
    }
}

/* -----------------------------------------------------------------------
 * MMIO read
 * ----------------------------------------------------------------------- */

static uint64_t usb_read(void *opaque, hwaddr addr, unsigned size)
{
    PIC32MKUSBState *s = opaque;
    hwaddr base = addr & ~(hwaddr)0xFu;   /* strip sub-register bits */

    switch (base) {
    /* OTG block */
    case PIC32MK_UxOTGIR:   return s->otgir;
    case PIC32MK_UxOTGIE:   return s->otgie;
    case PIC32MK_UxOTGSTAT: return s->otgstat;
    case PIC32MK_UxOTGCON:  return s->otgcon;
    case PIC32MK_UxPWRC:    return s->pwrc;

    /* Core registers */
    case PIC32MK_UxIR:      return s->uir;
    case PIC32MK_UxIE:      return s->uie;
    case PIC32MK_UxEIR:     return s->ueir;
    case PIC32MK_UxEIE:     return s->ueie;
    case PIC32MK_UxSTAT:    return s->ustat;
    case PIC32MK_UxCON:     return s->ucon;
    case PIC32MK_UxADDR:    return s->uaddr;
    case PIC32MK_UxBDTP1:   return s->bdtp1;
    case PIC32MK_UxFRML:    return s->ufrml;
    case PIC32MK_UxFRMH:    return s->ufrmh;
    case PIC32MK_UxTOK:     return s->utok;
    case PIC32MK_UxSOF:     return s->usof;
    case PIC32MK_UxBDTP2:   return s->bdtp2;
    case PIC32MK_UxBDTP3:   return s->bdtp3;
    case PIC32MK_UxCNFG1:   return s->cnfg1;

    default:
        break;
    }

    /* Endpoint control registers UxEP0–UxEP15 */
    if (base >= PIC32MK_UxEP_BASE &&
        base < PIC32MK_UxEP_BASE + PIC32MK_USB_NEPS * PIC32MK_UxEP_STRIDE) {
        unsigned ep = (base - PIC32MK_UxEP_BASE) / PIC32MK_UxEP_STRIDE;
        return s->uep[ep];
    }

    qemu_log_mask(LOG_UNIMP,
                  "pic32mk_usb: unimplemented read @ 0x%04" HWADDR_PRIx "\n",
                  addr);
    return 0;
}

/* -----------------------------------------------------------------------
 * MMIO write
 * ----------------------------------------------------------------------- */

static void usb_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    PIC32MKUSBState *s = opaque;
    hwaddr base = addr & ~(hwaddr)0xFu;
    int    sub  = (int)(addr & 0xFu);
    uint32_t v  = (uint32_t)val;

    switch (base) {

    /* ----- OTG block ----- */
    case PIC32MK_UxOTGIR:
        apply_w1c(&s->otgir, v, sub);
        usb_update_irq(s);
        return;

    case PIC32MK_UxOTGIE:
        apply_sci(&s->otgie, v, sub);
        usb_update_irq(s);
        return;

    case PIC32MK_UxOTGSTAT:
        return;   /* read-only */

    case PIC32MK_UxOTGCON:
        apply_sci(&s->otgcon, v, sub);
        return;

    case PIC32MK_UxPWRC: {
        uint32_t old_pwrc = s->pwrc;
        apply_sci(&s->pwrc, v, sub);
        /* When USBPWR first asserted: schedule USB Reset after 50 ms */
        if ((s->pwrc & USB_PWRC_USBPWR) && !(old_pwrc & USB_PWRC_USBPWR)
            && s->ep0_sim == EP0_SIM_IDLE) {
            s->ep0_sim = EP0_SIM_RESET;
            timer_mod(s->usb_timer,
                      qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 50 * SCALE_MS);
        }
        return;
    }

    /* ----- Core interrupt registers ----- */
    case PIC32MK_UxIR:
        apply_w1c(&s->uir, v, sub);
        usb_update_irq(s);
        return;

    case PIC32MK_UxIE:
        apply_sci(&s->uie, v, sub);
        usb_update_irq(s);
        return;

    case PIC32MK_UxEIR:
        apply_w1c(&s->ueir, v, sub);
        usb_update_irq(s);
        return;

    case PIC32MK_UxEIE:
        apply_sci(&s->ueie, v, sub);
        usb_update_irq(s);
        return;

    case PIC32MK_UxSTAT:
        return;   /* read-only */

    case PIC32MK_UxCON:
        apply_sci(&s->ucon, v, sub);
        return;

    case PIC32MK_UxADDR:
        apply_sci(&s->uaddr, v, sub);
        return;

    case PIC32MK_UxBDTP1:
        apply_sci(&s->bdtp1, v, sub);
        return;

    case PIC32MK_UxFRML:
    case PIC32MK_UxFRMH:
        return;   /* read-only */

    case PIC32MK_UxTOK:
        apply_sci(&s->utok, v, sub);
        return;

    case PIC32MK_UxSOF:
        apply_sci(&s->usof, v, sub);
        return;

    case PIC32MK_UxBDTP2:
        apply_sci(&s->bdtp2, v, sub);
        return;

    case PIC32MK_UxBDTP3:
        apply_sci(&s->bdtp3, v, sub);
        return;

    case PIC32MK_UxCNFG1:
        apply_sci(&s->cnfg1, v, sub);
        return;

    default:
        break;
    }

    /* Endpoint control registers */
    if (base >= PIC32MK_UxEP_BASE &&
        base < PIC32MK_UxEP_BASE + PIC32MK_USB_NEPS * PIC32MK_UxEP_STRIDE) {
        unsigned ep = (base - PIC32MK_UxEP_BASE) / PIC32MK_UxEP_STRIDE;
        apply_sci(&s->uep[ep], v, sub);
        return;
    }

    qemu_log_mask(LOG_UNIMP,
                  "pic32mk_usb: unimplemented write @ 0x%04" HWADDR_PRIx
                  " = 0x%08x\n", addr, v);
}

static const MemoryRegionOps usb_ops = {
    .read       = usb_read,
    .write      = usb_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

/* -----------------------------------------------------------------------
 * Device lifecycle
 * ----------------------------------------------------------------------- */

static void pic32mk_usb_reset(DeviceState *dev)
{
    PIC32MKUSBState *s = PIC32MK_USB(dev);

    /* Reset values: all zero unless specified in DS60001519E §25 */
    s->otgir   = 0;
    s->otgie   = 0;
    s->otgstat = 0;
    s->otgcon  = 0;
    s->pwrc    = 0;
    s->uir     = 0;
    s->uie     = 0;
    s->ueir    = 0;
    s->ueie    = 0;
    s->ustat   = 0;
    s->ucon    = 0;
    s->uaddr   = 0;
    s->bdtp1   = 0;
    s->bdtp2   = 0;
    s->bdtp3   = 0;
    s->ufrml   = 0;
    s->ufrmh   = 0;
    s->utok    = 0;
    s->usof    = 0x4Bu;
    s->cnfg1   = 0;
    memset(s->uep, 0, sizeof(s->uep));

    s->ep0_sim   = EP0_SIM_IDLE;
    s->configured = false;

    if (s->usb_timer) {
        timer_del(s->usb_timer);
    }

    qemu_set_irq(s->irq, 0);
}

static void pic32mk_usb_init(Object *obj)
{
    PIC32MKUSBState *s = PIC32MK_USB(obj);

    memory_region_init_io(&s->sfr_mmio, obj, &usb_ops, s,
                          TYPE_PIC32MK_USB, PIC32MK_USB_SFR_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->sfr_mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    s->usb_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, usb_timer_cb, s);
    s->ep0_sim   = EP0_SIM_IDLE;
    s->configured = false;
}

static void pic32mk_usb_realize(DeviceState *dev, Error **errp)
{
    PIC32MKUSBState *s = PIC32MK_USB(dev);
    if (!qemu_chr_fe_backend_connected(&s->chr)) {
        /* No chardev attached — CDC output goes nowhere (still functional) */
    }
    (void)s;
}

static const Property pic32mk_usb_properties[] = {
    DEFINE_PROP_CHR("chardev", PIC32MKUSBState, chr),
};

static void pic32mk_usb_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_legacy_reset(dc, pic32mk_usb_reset);
    device_class_set_props(dc, pic32mk_usb_properties);
    dc->realize = pic32mk_usb_realize;
    dc->desc = "PIC32MK USB OTG Full-Speed";
}

static const TypeInfo pic32mk_usb_info = {
    .name          = TYPE_PIC32MK_USB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PIC32MKUSBState),
    .instance_init = pic32mk_usb_init,
    .class_init    = pic32mk_usb_class_init,
};

static void pic32mk_usb_register_types(void)
{
    type_register_static(&pic32mk_usb_info);
}

type_init(pic32mk_usb_register_types)
