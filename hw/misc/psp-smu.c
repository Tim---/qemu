/*
 * AMD SMU.
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

#include "hw/misc/psp-smu.h"

REG32(INTERRUPT_READY,  0x28)
REG32(MSG_DATA,         0x700)
REG32(MSG_STATUS,       0x704)
REG32(MSG_CMD,          0x714)

static uint64_t psp_smu_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case A_INTERRUPT_READY:
        return 1;
    case A_MSG_STATUS:
        return 1;
    case A_MSG_DATA:
        return 0;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static void psp_smu_write(void *opaque, hwaddr offset,
                          uint64_t data, unsigned size)
{
    switch (offset) {
    case A_MSG_DATA:
    case A_MSG_CMD:
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}


const MemoryRegionOps psp_smu_ops = {
    .read = psp_smu_read,
    .write = psp_smu_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};


static void psp_smu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspSmuState *s = PSP_SMU(obj);

    memory_region_init_io(&s->regs_region, OBJECT(s), &psp_smu_ops, s,
                          "smu-regs", 0x10000);
    sysbus_init_mmio(sbd, &s->regs_region);
}

static void psp_smu_realize(DeviceState *dev, Error **errp)
{
}

static void psp_smu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_smu_realize;
    dc->desc = "PSP SMU registers";
}

static const TypeInfo psp_smu_type_info = {
    .name = TYPE_PSP_SMU,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspSmuState),
    .instance_init = psp_smu_init,
    .class_init = psp_smu_class_init,
};

static void psp_smu_register_types(void)
{
    type_register_static(&psp_smu_type_info);
}

type_init(psp_smu_register_types)
