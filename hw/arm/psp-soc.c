/*
 * PSP "SoC".
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
#include "hw/misc/unimp.h"
#include "exec/address-spaces.h"

#include "hw/arm/psp-soc.h"
#include "hw/arm/psp-utils.h"


static void psp_soc_init(Object *obj)
{
    PspSocState *pss = PSP_SOC(obj);

    object_initialize_child(obj, "soc-regs", &pss->soc_regs, TYPE_PSP_SOC_REGS);
    object_initialize_child(obj, "smn-bridge", &pss->smn_bridge, TYPE_SMN_BRIDGE);
    object_initialize_child(obj, "x86-bridge", &pss->x86_bridge, TYPE_X86_BRIDGE);
    object_initialize_child(obj, "ccp", &pss->ccp, TYPE_CCP);
    object_initialize_child(obj, "timer", &pss->timer, TYPE_PSP_TIMER);
    object_initialize_child(obj, "int", &pss->interrupt_controller, TYPE_PSP_INT);

}

static void psp_soc_realize(DeviceState *dev, Error **errp)
{
    PspSocState *pss = PSP_SOC(dev);

    // CPU
    object_initialize_child(OBJECT(dev), "cpu", &pss->cpu, pss->cpu_type);
    if (!qdev_realize(DEVICE(&pss->cpu), NULL, errp)) {
        return;
    }

    // RAM
    memory_region_add_subregion(get_system_memory(), 0, pss->ram);

    // SOC regs
    {
        if (!sysbus_realize(SYS_BUS_DEVICE(&pss->soc_regs), errp))
            return;

        // create regs region
        memory_region_init(&pss->soc_regs_region_public, OBJECT(dev), "soc-regs-public", 0x10000);
        memory_region_init(&pss->soc_regs_region_private, OBJECT(dev), "soc-regs-private", 0x10000);
        // map the default registers
        memory_region_add_subregion_overlap(&pss->soc_regs_region_public, 0, sysbus_mmio_get_region(SYS_BUS_DEVICE(&pss->soc_regs), 0), -500);
        memory_region_add_subregion_overlap(&pss->soc_regs_region_private, 0, sysbus_mmio_get_region(SYS_BUS_DEVICE(&pss->soc_regs), 1), -500);
        // map registers
        memory_region_add_subregion(get_system_memory(), 0x03010000, &pss->soc_regs_region_public);
        memory_region_add_subregion(get_system_memory(), 0x03200000, &pss->soc_regs_region_private);
    }

    // Interrupt controller
    if (!sysbus_realize(SYS_BUS_DEVICE(&pss->interrupt_controller), errp))
        return;
    memory_region_add_subregion(&pss->soc_regs_region_public, 0x200, sysbus_mmio_get_region(SYS_BUS_DEVICE(&pss->interrupt_controller), 0));
    memory_region_add_subregion(&pss->soc_regs_region_private, 0x200, sysbus_mmio_get_region(SYS_BUS_DEVICE(&pss->interrupt_controller), 1));
    sysbus_connect_irq(SYS_BUS_DEVICE(&pss->interrupt_controller), 0, qdev_get_gpio_in(DEVICE(&pss->cpu), ARM_CPU_FIQ));

    // Timer
    if (!sysbus_realize(SYS_BUS_DEVICE(&pss->timer), errp))
        return;
    memory_region_add_subregion(&pss->soc_regs_region_public, 0x400, sysbus_mmio_get_region(SYS_BUS_DEVICE(&pss->timer), 0));
    sysbus_connect_irq(SYS_BUS_DEVICE(&pss->timer), 0, qdev_get_gpio_in(DEVICE(&pss->interrupt_controller), 0x60));

    // SMN
    object_property_set_link(OBJECT(&pss->smn_bridge), "source-memory", OBJECT(pss->smn_region), &error_fatal);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pss->smn_bridge), errp))
        return;
    sysbus_mmio_map(SYS_BUS_DEVICE(&pss->smn_bridge), 0, 0x03220000);
    sysbus_mmio_map(SYS_BUS_DEVICE(&pss->smn_bridge), 1, 0x01000000);

    // X86
    object_property_set_link(OBJECT(&pss->x86_bridge), "source-memory", OBJECT(pss->x86_region), &error_fatal);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pss->x86_bridge), errp))
        return;
    sysbus_mmio_map(SYS_BUS_DEVICE(&pss->x86_bridge), 0, 0x03230000);
    sysbus_mmio_map(SYS_BUS_DEVICE(&pss->x86_bridge), 1, 0x04000000);

    // CCP
    if (!sysbus_realize(SYS_BUS_DEVICE(&pss->ccp), errp))
        return;
    sysbus_mmio_map(SYS_BUS_DEVICE(&pss->ccp), 0, 0x03000000);
    sysbus_connect_irq(SYS_BUS_DEVICE(&pss->ccp), 0, qdev_get_gpio_in(DEVICE(&pss->interrupt_controller), 0x15));

    // ???
    create_unimplemented_device("dev0321", 0x03210000, 0x10000);

}

static Property psp_soc_props[] = {
    DEFINE_PROP_LINK("ram", PspSocState, ram, TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_LINK("smn-memory", PspSocState, smn_region, TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_LINK("x86-memory", PspSocState, x86_region, TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_STRING("cpu-type", PspSocState, cpu_type),
    DEFINE_PROP_END_OF_LIST()
};

static void psp_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_soc_realize;
    dc->desc = "PSP SoC";
    device_class_set_props(dc, psp_soc_props);
}

static const TypeInfo psp_soc_type_info = {
    .name = TYPE_PSP_SOC,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(PspSocState),
    .instance_init = psp_soc_init,
    .class_init = psp_soc_class_init,
};

static void psp_soc_register_types(void)
{
    type_register_static(&psp_soc_type_info);
}

type_init(psp_soc_register_types)
