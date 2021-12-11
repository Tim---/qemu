/*
 * PSP timer device.
 *
 * Copyright (C) 2021 Timothée Cocault <timothee.cocault@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/registerfields.h"

#include "qemu/timer.h"
#include "hw/irq.h"
#include "hw/timer/psp-timer.h"
#include "qemu/qemu-print.h"

REG32(INT_CTRL0,        0x00)
REG32(INT_CTRL1,        0x04)
REG32(INT_ENABLE,       0x0C)
REG32(INT_FREQ,         0x10)

REG32(TICKS_CFG,        0x24)
REG32(TICKS_REG0,       0x28)
REG32(TICKS_REG1,       0x2C)
REG32(TICKS_REG2,       0x30)
REG32(TICKS_REG3,       0x34)
REG32(TICKS_REG4,       0x38)
REG32(TICKS_REG5,       0x3C)
REG32(TICKS_REG6,       0x40)
REG32(TICKS_VALUE,      0x44)


static uint64_t psp_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    PspTimerState *pts = PSP_TIMER(opaque);
    size_t index = offset / 4;

    switch(index) {
    case R_TICKS_VALUE:
        pts->ticks += 10000; // TODO: dirty hack !
        return pts->ticks;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __FUNCTION__, offset);

    return 0;
}

static void psp_timer_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    PspTimerState *pts = PSP_TIMER(opaque);
    size_t index = offset / 4;

    switch(index) {
    case R_INT_CTRL0:
        assert(data == 0x10000 || data == 0x10100 || data == 0x10001);
        return;
    case R_INT_CTRL1:
        assert(data == 0x100);
        return;
    case R_INT_ENABLE:
        assert(data == 0 || data == 1);
        pts->int_enable = (bool)data;
        if(pts->int_enable)
            timer_mod(pts->qemu_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 1000);
        else
            timer_del(pts->qemu_timer);
        return;
    case R_INT_FREQ:
        //uint32_t freq = data / 0x19;
        return;
    case R_TICKS_CFG:
        assert(data == 0 || data == 0x101);
        return;
    case R_TICKS_REG0 ... R_TICKS_REG6:
    case R_TICKS_VALUE:
        assert(data == 0);
        return; // Unknown
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __FUNCTION__, offset, data);
}


MemoryRegionOps psp_timer_ops = {
    .read = psp_timer_read,
    .write = psp_timer_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_timer_tick_expired(void *opaque)
{
    PspTimerState *pts = PSP_TIMER(opaque);

    qemu_irq_raise(pts->irq);
    timer_mod(pts->qemu_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 1000);
}

static void psp_timer_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspTimerState *pts = PSP_TIMER(obj);

    // Init the registers access
    memory_region_init_io(&pts->regs_region, OBJECT(pts), &psp_timer_ops, pts, "psp-timer", 0x100);
    sysbus_init_mmio(sbd, &pts->regs_region);

    pts->qemu_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL, psp_timer_tick_expired, pts);
    sysbus_init_irq(sbd, &pts->irq);
}

static void psp_timer_realize(DeviceState *dev, Error **errp)
{
}

static void psp_timer_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_timer_realize;
    dc->desc = "PSP timer";
}

static const TypeInfo psp_timer_type_info = {
    .name = TYPE_PSP_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspTimerState),
    .instance_init = psp_timer_init,
    .class_init = psp_timer_class_init,
};

static void psp_timer_register_types(void)
{
    type_register_static(&psp_timer_type_info);
}

type_init(psp_timer_register_types)
