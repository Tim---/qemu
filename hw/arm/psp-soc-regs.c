/*
 * PSP "SoC" registers.
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

#include "hw/timer/psp-timer.h"
#include "hw/arm/psp-soc-regs.h"
#include "psp-soc-regs-internal.h"


static uint64_t psp_soc_regs_public_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    PspSocRegsState *ps = PSP_SOC_REGS(opaque);
    PspSocRegsClass *pc = PSP_SOC_REGS_GET_CLASS(ps);
    return pc->public_read(ps, offset);
}

static void psp_soc_regs_public_write(void *opaque, hwaddr offset,
                                      uint64_t data, unsigned size)
{
    PspSocRegsState *ps = PSP_SOC_REGS(opaque);
    PspSocRegsClass *pc = PSP_SOC_REGS_GET_CLASS(ps);
    pc->public_write(ps, offset, data);
}

static uint64_t psp_soc_regs_private_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    PspSocRegsState *ps = PSP_SOC_REGS(opaque);
    PspSocRegsClass *pc = PSP_SOC_REGS_GET_CLASS(ps);
    return pc->private_read(ps, offset);
}

static void psp_soc_regs_private_write(void *opaque, hwaddr offset,
                                      uint64_t data, unsigned size)
{
    PspSocRegsState *ps = PSP_SOC_REGS(opaque);
    PspSocRegsClass *pc = PSP_SOC_REGS_GET_CLASS(ps);
    pc->private_write(ps, offset, data);
}

const MemoryRegionOps psp_soc_regs_public_ops = {
    .read = psp_soc_regs_public_read,
    .write = psp_soc_regs_public_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

const MemoryRegionOps psp_soc_regs_private_ops = {
    .read = psp_soc_regs_private_read,
    .write = psp_soc_regs_private_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_soc_regs_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspSocRegsState *s = PSP_SOC_REGS(obj);

    memory_region_init_io(&s->regs_region_public, OBJECT(s),
                          &psp_soc_regs_public_ops, s,
                          "soc-regs-public", 0x10000);
    sysbus_init_mmio(sbd, &s->regs_region_public);

    memory_region_init_io(&s->regs_region_private, OBJECT(s),
                          &psp_soc_regs_private_ops, s,
                          "soc-regs-private", 0x10000);
    sysbus_init_mmio(sbd, &s->regs_region_private);
}

static void psp_soc_regs_realize(DeviceState *dev, Error **errp)
{
    PspSocRegsState *s = PSP_SOC_REGS(dev);
    s->serial_enabled = 1;
}

static void psp_soc_regs_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_soc_regs_realize;
    dc->desc = "PSP SoC registers";
}

static void psp_soc_regs_0a_00_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PspSocRegsClass *pc = PSP_SOC_REGS_CLASS(oc);

    dc->realize = psp_soc_regs_realize;
    dc->desc = "PSP SoC registers (version 0A.00)";
    pc->public_read = psp_soc_regs_0a_00_public_read;
    pc->public_write = psp_soc_regs_0a_00_public_write;
    pc->private_read = psp_soc_regs_0a_00_private_read;
    pc->private_write = psp_soc_regs_0a_00_private_write;
}

static void psp_soc_regs_0b_05_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PspSocRegsClass *pc = PSP_SOC_REGS_CLASS(oc);

    dc->realize = psp_soc_regs_realize;
    dc->desc = "PSP SoC registers (version 0B.05)";
    pc->public_read = psp_soc_regs_0b_05_public_read;
    pc->public_write = psp_soc_regs_0b_05_public_write;
    pc->private_read = psp_soc_regs_0b_05_private_read;
    pc->private_write = psp_soc_regs_0b_05_private_write;
}

static const TypeInfo psp_soc_regs_type_info = {
    .name = TYPE_PSP_SOC_REGS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .abstract = true,
    .instance_size = sizeof(PspSocRegsState),
    .instance_init = psp_soc_regs_init,
    .class_size = sizeof(PspSocRegsClass),
    .class_init = psp_soc_regs_class_init,
};

static const TypeInfo psp_soc_regs_0a_00_type_info = {
    .name = TYPE_PSP_SOC_REGS_0A_00,
    .parent = TYPE_PSP_SOC_REGS,
    .class_init = psp_soc_regs_0a_00_class_init,
};

static const TypeInfo psp_soc_regs_0b_05_type_info = {
    .name = TYPE_PSP_SOC_REGS_0B_05,
    .parent = TYPE_PSP_SOC_REGS,
    .class_init = psp_soc_regs_0b_05_class_init,
};

static void psp_soc_regs_register_types(void)
{
    type_register_static(&psp_soc_regs_type_info);
    type_register_static(&psp_soc_regs_0a_00_type_info);
    type_register_static(&psp_soc_regs_0b_05_type_info);
}

type_init(psp_soc_regs_register_types)
