/*
 * SMN unknown registers for PSP.
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
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/arm/smn-misc.h"

static uint64_t smn_misc1_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;
    qemu_log_mask(LOG_UNIMP, "%s:  unimplemented device read  "
                  "(offset [%02lx:%03lx])\n",
                  __func__, offset >> 12, offset & 0xfff);
    return res;
}

static void smn_misc1_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset [%02lx:%03lx], value 0x%lx)\n",
                  __func__, offset >> 12, offset & 0xfff, value);
}

static const MemoryRegionOps smn_misc1_ops = {
    .read = smn_misc1_read,
    .write = smn_misc1_write,
};

static uint64_t smn_misc_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(offset) {
    case 0x5a86c:
        /* ???. Used in conjunction to 0x5d5bc. */
        res = 0x00810f00;
        break;
    case 0x5a870:
        /* bitmap of cores present */
        res = 1;
        break;
    case 0x03e10024:
        /* MP2_RSMU_FUSESTRAPS */
        res = 1;
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return res;
}

static void smn_misc_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, value);
}

static const MemoryRegionOps smn_misc_ops = {
    .read = smn_misc_read,
    .write = smn_misc_write,
};

static MemoryRegion *create_ram(const char *name, MemoryRegion *mem,
                                hwaddr addr, uint64_t size)
{
    MemoryRegion *region = g_new0(MemoryRegion, 1);
    memory_region_init_ram(region, NULL, name, size, &error_fatal);
    memory_region_add_subregion(mem, addr, region);
    return region;
}

static void smn_misc_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SmnMiscState *s = SMN_MISC(obj);

    /* Init the registers access */
    memory_region_init_io(&s->region, OBJECT(s), &smn_misc_ops, s,
                          "smn-misc", 0x100000000);
    sysbus_init_mmio(sbd, &s->region);

    /* some fuses */
    create_ram("fuses",         &s->region,   0x0005d000, 0x200);

    /* 0x03000000 -> 0x04000000 looks full of SMU */
    create_ram("smu-ram",       &s->region,   0x03c00000, 0x00040000);
    create_ram("smn-more-apob", &s->region,   0x03f40000, 0x00020000);
    create_ram("mp2-sram",      &s->region,   0x03f50000, 0x00000800);

    /* Looks similar to FICA in the data fabric */
    memory_region_init_io(&s->region1, OBJECT(s), &smn_misc1_ops, s,
                          "smn-misc1", 0x100000);
    memory_region_add_subregion(&s->region, 0x1000000, &s->region1);

}

static void smn_misc_realize(DeviceState *dev, Error **errp)
{
}

static void smn_misc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = smn_misc_realize;
    dc->desc = "PSP SMN misc";
}

static const TypeInfo smn_misc_type_info = {
    .name = TYPE_SMN_MISC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SmnMiscState),
    .instance_init = smn_misc_init,
    .class_init = smn_misc_class_init,
};

static void smn_misc_register_types(void)
{
    type_register_static(&smn_misc_type_info);
}

type_init(smn_misc_register_types)
