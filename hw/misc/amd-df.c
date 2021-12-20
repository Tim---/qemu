/*
 * AMD DataFabric PCI devices (D18F*).
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

#include "hw/misc/amd-df.h"

/*
 * Implements the PCI devices D18Fx configuration space.
 * This should be implemented as PCI devices but there's no reason to do it for
 * now.
 */


/* D18F0 */

static uint32_t amd_df_f0_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f0_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F1 */

static uint32_t amd_df_f1_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f1_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F2 */

static uint32_t amd_df_f2_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f2_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F3 */

static uint32_t amd_df_f3_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f3_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F4 */

REG32(FICA_ADDR, 0x50)
    FIELD(FICA_ADDR, RUN,    0, 2) /* must be one */
    FIELD(FICA_ADDR, REG,    2, 8) /* multiply by 4 */
    FIELD(FICA_ADDR, LARGE, 10, 1) /* 64 bits op */
    FIELD(FICA_ADDR, FUNC,  11, 3)
    FIELD(FICA_ADDR, NODE,  16, 8)
REG32(FICA_DATA0, 0x80)
REG32(FICA_DATA1, 0x84)

static uint32_t fica_read(AmdDfState *s, hwaddr addr)
{
    assert(FIELD_EX32(addr, FICA_ADDR, RUN) == 1);
    assert(FIELD_EX32(addr, FICA_ADDR, LARGE) == 0);
    uint32_t node = FIELD_EX32(addr, FICA_ADDR, NODE);
    uint32_t func = FIELD_EX32(addr, FICA_ADDR, FUNC);
    uint32_t reg = FIELD_EX32(addr, FICA_ADDR, REG) << 2;

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented read  "
                  "(addr [%02x:%01x:%03x])\n",
                  __func__, node, func, reg);
    return 0;
}

static void fica_write(AmdDfState *s, hwaddr addr, uint32_t data)
{
    assert(FIELD_EX32(addr, FICA_ADDR, RUN) == 1);
    assert(FIELD_EX32(addr, FICA_ADDR, LARGE) == 0);
    uint32_t node = FIELD_EX32(addr, FICA_ADDR, NODE);
    uint32_t func = FIELD_EX32(addr, FICA_ADDR, FUNC);
    uint32_t reg = FIELD_EX32(addr, FICA_ADDR, REG) << 2;

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented write "
                  "(addr [%02x:%01x:%03x], value 0x%x)\n",
                  __func__, node, func, reg, data);
}


static uint32_t amd_df_f4_read(AmdDfState *s, hwaddr offset)
{
    switch(offset) {
    case A_FICA_DATA0:
        return fica_read(s, s->fica_addr);
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f4_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    switch(offset) {
    case A_FICA_ADDR:
        s->fica_addr = data;
        return;
    case A_FICA_DATA0:
        fica_write(s, s->fica_addr, data);
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F5 */

static uint32_t amd_df_f5_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f5_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F6 */

static uint32_t amd_df_f6_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f6_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}

/* D18F7 */

static uint32_t amd_df_f7_read(AmdDfState *s, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void amd_df_f7_write(AmdDfState *s, hwaddr offset, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}


const struct {
    uint32_t (*read)(AmdDfState *s, hwaddr offset);
    void (*write)(AmdDfState *s, hwaddr offset, uint32_t data);
} funcs[] = {
    {
        .read = amd_df_f0_read,
        .write = amd_df_f0_write,
    }, {
        .read = amd_df_f1_read,
        .write = amd_df_f1_write,
    }, {
        .read = amd_df_f2_read,
        .write = amd_df_f2_write,
    }, {
        .read = amd_df_f3_read,
        .write = amd_df_f3_write,
    }, {
        .read = amd_df_f4_read,
        .write = amd_df_f4_write,
    }, {
        .read = amd_df_f4_read,
        .write = amd_df_f4_write,
    }, {
        .read = amd_df_f5_read,
        .write = amd_df_f5_write,
    }, {
        .read = amd_df_f6_read,
        .write = amd_df_f6_write,
    }, {
        .read = amd_df_f7_read,
        .write = amd_df_f7_write,
    }
};

static uint64_t amd_df_read(void *opaque, hwaddr offset, unsigned size)
{
    AmdDfState *s = AMD_DF(opaque);
    return funcs[offset >> 10].read(s, offset & 0x3ff);
}

static void amd_df_write(void *opaque, hwaddr offset,
                         uint64_t data, unsigned size)
{
    AmdDfState *s = AMD_DF(opaque);
    funcs[offset >> 10].write(s, offset & 0x3ff, data);
}


const MemoryRegionOps amd_df_ops = {
    .read = amd_df_read,
    .write = amd_df_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

static void amd_df_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    AmdDfState *s = AMD_DF(obj);

    memory_region_init_io(&s->regs, OBJECT(s), &amd_df_ops, s,
                          "amd-df", 8*0x400);
    sysbus_init_mmio(sbd, &s->regs);
}


static void amd_df_realize(DeviceState *dev, Error **errp)
{
}

static void amd_df_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = amd_df_realize;
    dc->desc = "AMD DataFabric";
}

static const TypeInfo amd_df_type_info = {
    .name = TYPE_AMD_DF,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AmdDfState),
    .instance_init = amd_df_init,
    .class_init = amd_df_class_init,
};

static void amd_df_register_types(void)
{
    type_register_static(&amd_df_type_info);
}

type_init(amd_df_register_types)
