/*
 * AMD UMC (Universal Memory Controller).
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

#include "hw/mem/psp-umc.h"

/*
 * UMC channels
 * See linux/drivers/edac/amd64_edac.h
 * UMCCH_* registers
 */

REG32(EMU_TYPE,         0x1050)
REG32(CTRL_FLAGS,       0x01ec)

typedef enum {
    EMU_SIMULATION = 0xb1aab1aa,
    EMU_EMULATION1 = 0x4e554e55,
    EMU_EMULATION2 = 0x51e051e0,
} emu_type_e;

static uint64_t psp_umc_read(void *opaque, hwaddr offset, unsigned size)
{

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    if (offset < 0x1000) {
        return 0;
    } else {
        return 0xffffffff;
    }
}

static void psp_umc_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    switch (offset) {
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}


const MemoryRegionOps psp_umc_ops = {
    .read = psp_umc_read,
    .write = psp_umc_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_umc_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspUmcState *pis = PSP_UMC(obj);

    memory_region_init_io(&pis->regs_region, OBJECT(pis), &psp_umc_ops, pis, "psp-umc", 0x2000);
    sysbus_init_mmio(sbd, &pis->regs_region);
}

static void psp_umc_realize(DeviceState *dev, Error **errp)
{
}

static void psp_umc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_umc_realize;
    dc->desc = "PSP emulated UMC";
}

static const TypeInfo psp_umc_type_info = {
    .name = TYPE_PSP_UMC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspUmcState),
    .instance_init = psp_umc_init,
    .class_init = psp_umc_class_init,
};

static void psp_umc_register_types(void)
{
    type_register_static(&psp_umc_type_info);
}

type_init(psp_umc_register_types)
