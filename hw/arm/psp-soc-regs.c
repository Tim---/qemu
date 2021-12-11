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

/*
 * Bits 8:15 of "SOC_REGS_TYPE" register seem to define the registers layout
 * The bootloader assigns type 2 if it is zero, and type 3 otherwise
 */
#define PSP_SOC_MAGIC   0x00

#if PSP_SOC_MAGIC == 0x00
#define PSP_SOC_TYPE 2
#else
#define PSP_SOC_TYPE 3
#endif

#if PSP_SOC_TYPE == 2
REG32(PUB_SERIAL_PORT_ENABLE, 0x3C)
REG32(PUB_BL_VERSION,         0x44)
#define PRIV_GENERIC_REG_BASE  0xA0
#else
REG32(PUB_SERIAL_PORT_ENABLE, 0x40)
REG32(PUB_BL_VERSION,         0x48)
#define PRIV_GENERIC_REG_BASE  0xA0
#endif

REG32(PRIV_SOC_REGS_TYPE,    0x4c)
REG32(PRIV_POSTCODE,         PRIV_GENERIC_REG_BASE + 0x14 * 4)
REG32(PUB_CLOCK0,           0x108)
REG32(PUB_CLOCK1,           0x10C)

REG32(PUB_BL_VERSION2,      0x9EC)

static uint64_t psp_soc_regs_public_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    PspSocRegsState *s = PSP_SOC_REGS(opaque);

    switch (offset) {
    case A_PUB_SERIAL_PORT_ENABLE:
        return s->serial_enabled;
    case A_PUB_CLOCK0:
    case A_PUB_CLOCK1:
        return 0;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, offset);

    return 0;
}

static void psp_soc_regs_public_write(void *opaque, hwaddr offset,
                                      uint64_t data, unsigned size)
{
    PspSocRegsState *s = PSP_SOC_REGS(opaque);

    switch (offset) {
    case A_PUB_SERIAL_PORT_ENABLE:
        s->serial_enabled = (data != 0);
        return;
    case A_PUB_BL_VERSION2:
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}


const MemoryRegionOps psp_soc_regs_public_ops = {
    .read = psp_soc_regs_public_read,
    .write = psp_soc_regs_public_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};


static uint64_t psp_soc_regs_private_read(void *opaque, hwaddr offset,
                                          unsigned size)
{
    switch (offset) {
    case A_PRIV_SOC_REGS_TYPE:
        return PSP_SOC_MAGIC << 8;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                 "(offset 0x%lx)\n",
                 __func__, offset);

    return 0;
}

static void psp_soc_regs_private_write(void *opaque, hwaddr offset,
                                       uint64_t data, unsigned size)
{
    switch (offset) {
    case A_PRIV_POSTCODE:
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}


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
}

static void psp_soc_regs_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_soc_regs_realize;
    dc->desc = "PSP SoC registers";
}

static const TypeInfo psp_soc_regs_type_info = {
    .name = TYPE_PSP_SOC_REGS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspSocRegsState),
    .instance_init = psp_soc_regs_init,
    .class_init = psp_soc_regs_class_init,
};

static void psp_soc_regs_register_types(void)
{
    type_register_static(&psp_soc_regs_type_info);
}

type_init(psp_soc_regs_register_types)
