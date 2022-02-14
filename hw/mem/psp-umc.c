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

/* BFDctAddlOffsetReg */
REG32(CTRL_INDIRECT_ADDR,         0x1800)
/* BFDctAddlDataReg */
REG32(CTRL_INDIRECT_DATA,         0x1804)
/* BFDctAddlDataReg */
REG32(CTRL_INDIRECT_STATUS,       0x1808)


/*
Special control registers:
0x1800: BFDctAddlOffsetReg
0x1804: BFDctAddlDataReg

We first write (possibly several ?) data to BFDctAddlDataReg
Then write the (indirect address | DCT_ACCESS_WRITE) in OffsetReg

DCT_ACCESS_WRITE = 0x80000000
*/

typedef enum {
    EMU_SIMULATION = 0xb1aab1aa,
    EMU_EMULATION1 = 0x4e554e55,
    EMU_EMULATION2 = 0x51e051e0,
} emu_type_e;


/*
These function control direct R/W access to the "PHY"
*/
static uint32_t psp_umc_indirect_read(uint32_t addr)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%x)\n",
                __func__, addr);

    return 0;
}

static void psp_umc_indirect_write(uint32_t addr, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%x, value 0x%x)\n",
                  __func__, addr, data);
    return;
}

static uint64_t psp_umc_ch_read(PspUmcState *s, hwaddr offset)
{
    switch(offset) {
    case 0x0:
    case 0x4:
    case 0x8:
    case 0xc:
    case 0x30:
    case 0x40:
    case 0x50:
    case 0x54:
    case 0x70:
    case 0x74:
    case 0x80:
    case 0x84:
    case 0xc8:
    case 0xcc:
    case 0xd0:
    case 0xd4:
    case 0xe8:
    case 0xec:
    case 0x100:
    case 0x104:
    case 0x108:
    case 0x10c:
    case 0x110:
    case 0x114:
    case 0x11c:
    case 0x120:
    case 0x124:
    case 0x12c:
    case 0x138:
    case 0x14c:
    case 0x150:
    case 0x1e0:
    case 0x200:
    case 0x22c:
    case 0xd6c:
    case 0xdf0:
    case 0xdf4:
    case 0xf28:
    case 0xf2c:
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static uint64_t psp_umc_ctrl_read(PspUmcState *s, hwaddr offset)
{
    switch(offset) {
    case 0x1000:
    case 0x1050:
    case 0x11ac:
    case 0x11b0:
    case 0x11b4:
    case 0x11b8:
    case 0x11bc:
    case 0x11e4:
        break;
    case A_CTRL_INDIRECT_DATA:
        return s->indirect_data;
    case A_CTRL_INDIRECT_STATUS:
        return 1; /* Ready */
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0xffffffff;
}

static void psp_umc_ch_write(PspUmcState *s, hwaddr offset, uint64_t data)
{
    switch(offset) {
    case 0x0:
    case 0x4:
    case 0x20:
    case 0x30:
    case 0x40:
    case 0x50:
    case 0x54:
    case 0x70:
    case 0x74:
    case 0x80:
    case 0x84:
    case 0xc8:
    case 0xcc:
    case 0xd0:
    case 0xd4:
    case 0xe8:
    case 0xec:
    case 0xf0:
    case 0xf4:
    case 0x100:
    case 0x104:
    case 0x108:
    case 0x10c:
    case 0x110:
    case 0x114:
    case 0x11c:
    case 0x120:
    case 0x124:
    case 0x12c:
    case 0x138:
    case 0x144:
    case 0x148:
    case 0x150:
    case 0x1e0:
    case 0x200:
    case 0x22c:
    case 0xa00:
    case 0xa04:
    case 0xa08:
    case 0xa0c:
    case 0xf28:
    case 0xf2c:
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

static void psp_umc_ctrl_write(PspUmcState *s, hwaddr offset, uint64_t data)
{
    switch(offset) {
    case 0x1000:
    case 0x11b0:
    case 0x11b4:
    case 0x11b8:
    case 0x11bc:
    case 0x11e4:
        break;
    case A_CTRL_INDIRECT_ADDR:
        s->indirect_addr = data & 0x7fffffff;
        if(data & 0x80000000) {
            psp_umc_indirect_write(s->indirect_addr, s->indirect_data);
        } else {
            s->indirect_data = psp_umc_indirect_read(s->indirect_addr);
        }
        return;
    case A_CTRL_INDIRECT_DATA:
        s->indirect_data = data;
        return;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

static uint64_t psp_umc_read(void *opaque, hwaddr offset, unsigned size)
{
    PspUmcState *s = PSP_UMC(opaque);
    
    if (offset < 0x1000) {
        return psp_umc_ch_read(s, offset);
    } else {
        return psp_umc_ctrl_read(s, offset);
    }
}

static void psp_umc_write(void *opaque, hwaddr offset,
                          uint64_t data, unsigned size)
{
    PspUmcState *s = PSP_UMC(opaque);

    if (offset < 0x1000) {
        psp_umc_ch_write(s, offset, data);
    } else {
        psp_umc_ctrl_write(s, offset, data);
    }
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
    PspUmcState *s = PSP_UMC(obj);

    memory_region_init_io(&s->regs_region, OBJECT(s), &psp_umc_ops,
                          s, "psp-umc", 0x2000);
    sysbus_init_mmio(sbd, &s->regs_region);
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
