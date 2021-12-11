/*
 * PSP-specific bridge to SysHub address space.
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
#include "hw/misc/unimp.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"

#include "hw/arm/x86-bridge.h"

/*
 * (perhaps this should be named "syshub_bridge" according to strings)
 * The bridge contains 0x3e slots, but only 0xf are mapped
 */


#define A_SLOTS_START   0x0000

#define A_SLOT_ADDR     0x00
#define A_SLOT_STATUS   0x04
#define A_SLOT_TYPE1    0x08
#define A_SLOT_TYPE2    0x0C

#define A_SLOTS_END     (A_SLOTS_START + X86_BRIDGE_MAX_MAPPINGS * 0x10)

#define A_ARRAY_A_BASE  A_SLOTS_END
#define A_ARRAY_A_END   (A_ARRAY_A_BASE + X86_BRIDGE_MAX_MAPPINGS * 4)

#define A_ARRAY_B_BASE  A_ARRAY_A_END
#define A_ARRAY_B_END   (A_ARRAY_B_BASE + X86_BRIDGE_MAX_MAPPINGS * 4)

#define A_DOIT          0x05F0

#define A_KEY_BASE      0x8000
#define A_KEY_RELATED   0x8040

static uint64_t x86_bridge_io_read(void *opaque, hwaddr addr, unsigned size)
{
    X86BridgeState *s = X86_BRIDGE(opaque);

    if (addr < A_SLOTS_END) {
        int index = addr / 0x10;
        x86_bridge_slot_t *slot = &s->slots[index];
        int reg = addr % 0x10;
        switch (reg) {
        case A_SLOT_ADDR:
            return slot->addr;
        case A_SLOT_STATUS:
            return slot->status;
        case A_SLOT_TYPE1:
            return slot->type1;
        case A_SLOT_TYPE2:
            return slot->type2;
        }
    } else if (addr < A_ARRAY_A_END) {
        int index = (addr - A_ARRAY_A_BASE) / 4;
        return s->A[index];
    } else if (addr < A_ARRAY_B_END) {
        int index = (addr - A_ARRAY_B_BASE) / 4;
        return s->B[index];
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
            "%s: Bad read offset 0x%"HWADDR_PRIx"\n",
            __func__, addr);
    }
    return 0;
}

static void x86_bridge_io_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    X86BridgeState *s = X86_BRIDGE(opaque);

    if (addr < A_SLOTS_END) {
        int index = addr / 0x10;
        x86_bridge_slot_t *slot = &s->slots[index];
        int reg = addr % 0x10;
        switch (reg) {
        case A_SLOT_ADDR:
            if (value != slot->addr) {
                hwaddr target_addr = value << 0x1a;
                memory_region_set_alias_offset(&s->aliases[index], target_addr);
            }
            slot->addr = value;
            return;
        case A_SLOT_STATUS:
            if (value != slot->status) {
                memory_region_set_enabled(&s->aliases[index], value != 0);
            }
            slot->status = value;
            return;
        case A_SLOT_TYPE1:
            slot->type1 = value;
            return;
        case A_SLOT_TYPE2:
            slot->type2 = value;
            return;
        }
    } else if (addr < A_ARRAY_A_END) {
        int index = (addr - A_ARRAY_A_BASE) / 4;
        s->A[index] = value;
        return;
    } else if (addr < A_ARRAY_B_END) {
        int index = (addr - A_ARRAY_B_BASE) / 4;
        s->B[index] = value;
        return;
    } else {
        switch (addr) {
        case A_DOIT:
            break;
        case A_KEY_RELATED:
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                "%s: Bad write offset 0x%"HWADDR_PRIx"\n",
                __func__, addr);
        }
    }
}

static const MemoryRegionOps x86_bridge_io_ops = {
    .read = x86_bridge_io_read,
    .write = x86_bridge_io_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void x86_bridge_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    X86BridgeState *s = X86_BRIDGE(obj);

    // Init the registers access
    memory_region_init_io(&s->regs_region, OBJECT(s), &x86_bridge_io_ops,
                          s, "x86-bridge-regs", 0x10000);
    sysbus_init_mmio(sbd, &s->regs_region);

    // Init the mapped memory
    memory_region_init(&s->mapped_region, OBJECT(s), "x86-bridge-mapped-space",
                       X86_BRIDGE_MAX_MAPPINGS * X86_BRIDGE_MAPPING_SIZE);

    // Only exposed the num_regions
    memory_region_init_alias(&s->exposed_region, OBJECT(s),
                             "x86-bridge-exposed-space", &s->mapped_region, 0,
                             X86_BRIDGE_NUM_MAPPINGS * X86_BRIDGE_MAPPING_SIZE);
    sysbus_init_mmio(sbd, &s->exposed_region);

}

static void x86_bridge_realize(DeviceState *dev, Error **errp)
{
    X86BridgeState *s = X86_BRIDGE(dev);

    for (int idx = 0; idx < X86_BRIDGE_MAX_MAPPINGS; idx++) {
        /* We first create an alias object */
        memory_region_init_alias(&s->aliases[idx], OBJECT(s), "alias[*]",
                                 s->source_memory, 0, X86_BRIDGE_MAPPING_SIZE);
        /* We then add this object in the mapped space */
        memory_region_add_subregion(&s->mapped_region,
                                    idx * X86_BRIDGE_MAPPING_SIZE,
                                    &s->aliases[idx]);
        memory_region_set_enabled(&s->aliases[idx], false);
    }
}

static Property x86_bridge_props[] = {
    DEFINE_PROP_LINK("source-memory", X86BridgeState, source_memory,
                     TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static void x86_bridge_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = x86_bridge_realize;
    dc->desc = "PSP X86 bridge";
    device_class_set_props(dc, x86_bridge_props);
}

static const TypeInfo x86_bridge_type_info = {
    .name = TYPE_X86_BRIDGE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(X86BridgeState),
    .instance_init = x86_bridge_init,
    .class_init = x86_bridge_class_init,
};

static void x86_bridge_register_types(void)
{
    type_register_static(&x86_bridge_type_info);
}

type_init(x86_bridge_register_types)
