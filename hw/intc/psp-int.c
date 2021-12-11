/*
 * PSP interrupt controller.
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
#include "hw/intc/psp-int.h"
#include "qemu/qemu-print.h"
#include "trace.h"

REG32(INT_ENABLE0,      0x00)
REG32(INT_ENABLE1,      0x04)
REG32(INT_ENABLE2,      0x08)
REG32(INT_ENABLE3,      0x0c)

REG32(INT_ACK0,         0xb0)
REG32(INT_ACK1,         0xb4)
REG32(INT_ACK2,         0xb8)
REG32(INT_ACK3,         0xbc)

REG32(NO_MORE_INT,      0xc0)
REG32(INT_NUM,          0xc4)

static void psp_int_irq_update(PspIntState *pis)
{
    int reg, bit;

    pis->current_int = 0xffffffff;

    for(reg = 0; reg < PSP_INT_ARRAY_SIZE; reg++) {
        uint32_t valid = pis->active[reg] & pis->enabled[reg];
        if(valid) {
            for(bit = 0; bit < 32; bit++) {
                if((valid >> bit) & 1) {
                    pis->current_int = reg * 32 + bit;
                }
            }
        }
    }

    qemu_set_irq(pis->irq, pis->current_int != 0xffffffff);
}

static void set_int_enable(PspIntState *pis, size_t index, uint64_t data)
{
    uint32_t new_enabled = data & ~pis->enabled[index];
    uint32_t new_disabled = ~data & pis->enabled[index];

    for(int i = 0; i < 32; i++) {
        if((new_enabled >> i) & 1)
            trace_psp_int_enable(index * 32 + i);
        if((new_disabled >> i) & 1)
            trace_psp_int_disable(index * 32 + i);
    }
    pis->enabled[index] = data;

    psp_int_irq_update(pis);
}

static void set_int_ack(PspIntState *pis, size_t index, uint64_t data)
{
    for(int i = 0; i < 32; i++) {
        if((data >> i) & 1)
            trace_psp_int_ack(index * 32 + i);
    }
    pis->active[index] &= ~data;

    psp_int_irq_update(pis);
}

static uint64_t psp_int_public_read(void *opaque, hwaddr offset, unsigned size)
{
    PspIntState *pis = PSP_INT(opaque);
    size_t index = offset / 4;

    switch(index) {
    case R_INT_ENABLE0 ... R_INT_ENABLE3:
        return pis->enabled[index - R_INT_ENABLE0];
    case R_NO_MORE_INT:
        return pis->current_int == 0xffffffff;
    case R_INT_NUM:
        return pis->current_int;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __FUNCTION__, offset);

    return 0;
}

static void psp_int_public_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    PspIntState *pis = PSP_INT(opaque);
    size_t index = offset / 4;

    switch(index) {
    case R_INT_ENABLE0 ... R_INT_ENABLE3:
        set_int_enable(pis, index - R_INT_ENABLE0, data);
        return;
    case R_INT_ACK0 ... R_INT_ACK3:
        set_int_ack(pis, index - R_INT_ACK0, data);
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __FUNCTION__, offset, data);
}


MemoryRegionOps psp_int_public_ops = {
    .read = psp_int_public_read,
    .write = psp_int_public_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static uint64_t psp_int_private_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __FUNCTION__, offset);

    return 0;
}

static void psp_int_private_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __FUNCTION__, offset, data);
}


MemoryRegionOps psp_int_private_ops = {
    .read = psp_int_private_read,
    .write = psp_int_private_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_int_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspIntState *pis = PSP_INT(obj);

    // Init the registers access
    memory_region_init_io(&pis->regs_region_public, OBJECT(pis), &psp_int_public_ops, pis, "psp-int-public", 0x100);
    sysbus_init_mmio(sbd, &pis->regs_region_public);
    memory_region_init_io(&pis->regs_region_private, OBJECT(pis), &psp_int_private_ops, pis, "psp-int-private", 0x100);
    sysbus_init_mmio(sbd, &pis->regs_region_private);

    sysbus_init_irq(sbd, &pis->irq);
}

static void psp_int_set_irq(void *opaque, int n_IRQ, int level)
{
    PspIntState *pis = PSP_INT(opaque);

    pis->active[n_IRQ >> 5] = deposit32(pis->active[n_IRQ >> 5], n_IRQ & 0x1f, 1, level);
    psp_int_irq_update(pis);
}

static void psp_int_realize(DeviceState *dev, Error **errp)
{
    PspIntState *pis = PSP_INT(dev);

    qdev_init_gpio_in(DEVICE(dev), psp_int_set_irq, PSP_INT_NUM_IRQS);
    psp_int_irq_update(pis);
}

static void psp_int_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_int_realize;
    dc->desc = "PSP interrupt controller";
}

static const TypeInfo psp_int_type_info = {
    .name = TYPE_PSP_INT,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspIntState),
    .instance_init = psp_int_init,
    .class_init = psp_int_class_init,
};

static void psp_int_register_types(void)
{
    type_register_static(&psp_int_type_info);
}

type_init(psp_int_register_types)
