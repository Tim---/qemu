/*
 * AMD FCH (Fusion Controller Hub).
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
#include "hw/char/serial.h"
#include "sysemu/sysemu.h"
#include "qemu/qemu-print.h"

#include "hw/misc/fch.h"

#define X86_IO_BASE 0xfc000000
// See coreboot/src/soc/amd/common/block/include/amdblocks/aoac.h
#define AOACx0000   0xfed81e00

static uint64_t fch_read(void *opaque, hwaddr offset, unsigned size)
{
    switch(offset) {
    case AOACx0000 + 0x77:
        return 7;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __FUNCTION__, offset);

    return 0;
}

static void fch_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    switch(offset) {
    case X86_IO_BASE + 0x80: // Postcode
        qemu_printf("\033[0;31mPOSTCODE %08lx\033[0m\n", data);
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __FUNCTION__, offset, data);
}


MemoryRegionOps fch_ops = {
    .read = fch_read,
    .write = fch_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};


static void fch_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    FchState *s = FCH(obj);

    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_ops, s, "fch", 0x100000000);
    sysbus_init_mmio(sbd, &s->regs_region);

    serial_mm_init(&s->regs_region, X86_IO_BASE + 0x3f8, 0, NULL, 9600, serial_hd(0), DEVICE_NATIVE_ENDIAN);
}

static void fch_realize(DeviceState *dev, Error **errp)
{
}

static void fch_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_realize;
    dc->desc = "AMD FCH";
}

static const TypeInfo fch_type_info = {
    .name = TYPE_FCH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchState),
    .instance_init = fch_init,
    .class_init = fch_class_init,
};

static void fch_register_types(void)
{
    type_register_static(&fch_type_info);
}

type_init(fch_register_types)
