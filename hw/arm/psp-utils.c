/*
 * PSP-specific util functions.
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
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/misc/unimp.h"

#include "hw/arm/psp-utils.h"

static uint64_t stub_read(void *opaque, hwaddr offset,
                          unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return (uint64_t)opaque;
}

static void stub_write(void *opaque, hwaddr offset,
                       uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps stub_ops = {
    .read = stub_read,
    .write = stub_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

void stub_create(const char *name, MemoryRegion *parent_region,
                 hwaddr offset, uint32_t size, uint64_t value)
{
    MemoryRegion *region = g_new0(MemoryRegion, 1);
    memory_region_init_io(region, NULL, &stub_ops, (void *)value, name, size);
    memory_region_add_subregion(parent_region, offset, region);
}

void create_unimplemented_device_custom(const char *name, MemoryRegion *region,
                                        hwaddr base, hwaddr size, bool ones)
{
    DeviceState *dev = qdev_new(TYPE_UNIMPLEMENTED_DEVICE);

    qdev_prop_set_string(dev, "name", name);
    qdev_prop_set_uint64(dev, "size", size);
    qdev_prop_set_bit(dev, "ones", ones);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    memory_region_add_subregion_overlap(region, base, sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0), -1000);
}
