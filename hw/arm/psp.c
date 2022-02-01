/*
 * PSP "board".
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
#include "hw/boards.h"
#include "qapi/error.h"
#include "hw/loader.h"
#include "hw/arm/psp-soc.h"
#include "hw/arm/psp-utils.h"
#include "hw/isa/fch-lpc.h"
#include "hw/pci-host/zen-pci-root.h"
#include "hw/mem/psp-umc.h"
#include "hw/misc/amd-df.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/ssi.h"
#include "hw/arm/smn-misc.h"
#include "elf.h"

#define TYPE_PSP_MACHINE "psp"
#define TYPE_PSP_MACHINE_0A_00 MACHINE_TYPE_NAME("psp-0a-00")
#define TYPE_PSP_MACHINE_0B_05 MACHINE_TYPE_NAME("psp-0b-05")

struct PspMachineClass {
    MachineClass parent;
    uint32_t version;
    uint32_t rom_addr;
    const char *soc_type;
};

OBJECT_DECLARE_TYPE(PspMachineState, PspMachineClass, PSP_MACHINE)

struct PspMachineState {
    MachineState parent;

    PspSocState soc;
    FchAcpiState fch_acpi;
    FchSpiState fch_spi;
    FchLpcState fch_lpc;
    PspUmcState umc0;
    PspUmcState umc1;
    SmnMiscState smn_misc;
    AmdDfState df;
    MemoryRegion smn_region;
    MemoryRegion x86_full_region;
    MemoryRegion x86_region_fffd;
    MemoryRegion x86_region_fffe;
    MemoryRegion x86_region_0000;
};

#define X86_IO_BASE 0xfc000000
/* See coreboot/src/soc/amd/common/block/include/amdblocks/aoac.h */
#define ACPI_MMIO_ADDR 0xfed80000


static MemoryRegion *create_ram(const char *name, MemoryRegion *mem,
                                hwaddr addr, uint64_t size)
{
    MemoryRegion *region = g_new0(MemoryRegion, 1);
    memory_region_init_ram(region, NULL, name, size, &error_fatal);
    memory_region_add_subregion(mem, addr, region);
    return region;
}

static void pcie_x86_mmcfg_map(PspMachineState *pms, PCIExpressHost *e,
                               hwaddr addr, uint32_t size)
{
    pcie_host_mmcfg_init(e, size);
    e->base_addr = addr;
    memory_region_add_subregion(&pms->x86_region_fffe, e->base_addr, &e->mmio);
}


static DeviceState *create_pcie_root(PspMachineState *pms)
{
    DeviceState *dev;

    dev = qdev_new(TYPE_ZEN_HOST);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    /* Add mmcfg region */
    PCIExpressHost *peh = PCIE_HOST_BRIDGE(dev);
    pcie_x86_mmcfg_map(pms, peh, 0, PCIE_MMCFG_SIZE_MIN);

    return dev;
}

static void create_x86_dirt(PspMachineState *pms)
{
    /* Well, maybe it's just... RAM ? */
    create_ram("alt-apob",      &pms->x86_region_0000, 0x0a200000, 0x00010000);
    create_ram("x86-apob",      &pms->x86_region_0000, 0x04000000, 0x00010000);
    create_ram("x86-bin",       &pms->x86_region_0000, 0x09d80000, 0x00400000);
    create_ram("write-pattern", &pms->x86_region_fffd, 0xf7000000, 0x00001000);
    create_ram("more-ram1",     &pms->x86_region_fffd, 0xfb000000, 0x01000000);
}

static void create_dirt(PspMachineState *pms)
{
    create_x86_dirt(pms);
}

static void fch_spi_create_flash(PspMachineState *pms, Error **errp)
{
    qemu_irq cs_line;
    DeviceState *dev;

    DriveInfo *dinfo = drive_get_next(IF_MTD);
    assert(dinfo);
    BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
    assert(blk);

    dev = qdev_new("mx25l12805d");
    qdev_prop_set_drive(dev, "drive", blk);
    qdev_realize_and_unref(dev, qdev_get_child_bus(DEVICE(&pms->fch_spi), "spi"), errp);

    cs_line = qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0);
    sysbus_connect_irq(SYS_BUS_DEVICE(&pms->fch_spi), 0, cs_line);
}

static void psp_load_bootloader(PspMachineState *pms)
{
    PspMachineClass *pmc = PSP_MACHINE_GET_CLASS(pms);

    /* Load bootloader */
    create_ram("on-chip-bootloader", get_system_memory(), 0x0f000000, 0x00010000);
    uint64_t elf_entry = 0;
    load_elf("pc-bios/psp/loader.elf", NULL, NULL, NULL, &elf_entry, NULL, NULL, NULL, false, EM_ARM, 1, 0);
    cpu_set_pc(CPU(&pms->soc.cpu), elf_entry);
    CPUARMState *env = &pms->soc.cpu.env;
    env->regs[13] = 0x3f000;
    env->regs[0] = pmc->version;
}

static void psp_machine_init(MachineState *machine)
{
    PspMachineState *pms = PSP_MACHINE(machine);
    PspMachineClass *pmc = PSP_MACHINE_GET_CLASS(pms);

    /* SMN region */
    memory_region_init(&pms->smn_region, OBJECT(pms),
                       "smn-region", 0x100000000);

    /* SMN misc registers */
    object_initialize_child(OBJECT(machine), "smn-misc", &pms->smn_misc, TYPE_SMN_MISC);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->smn_misc), &error_fatal)) {
        return;
    }
    memory_region_add_subregion_overlap(&pms->smn_region, 0,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->smn_misc), 0), -1000);

    /* X86 regions ! */
    memory_region_init(&pms->x86_full_region, OBJECT(pms),
                       "x86-full-region", 0x1000000000000UL);
    create_unimplemented_device_custom("x86-full-unimpl", &pms->x86_full_region,
                                       0, 0x1000000000000UL, false);

    memory_region_init(&pms->x86_region_fffd, OBJECT(pms),
                       "x86-fffd-region", 0x100000000UL);
    create_unimplemented_device_custom("x86-fffd-unimpl", &pms->x86_region_fffd,
                                       0, 0x100000000UL, false);
    memory_region_add_subregion(&pms->x86_full_region, 0xfffd00000000UL,
                                &pms->x86_region_fffd);

    memory_region_init(&pms->x86_region_fffe, OBJECT(pms),
                       "x86-fffe-region", 0x100000000UL);
    create_unimplemented_device_custom("x86-fffe-unimpl", &pms->x86_region_fffe,
                                       0, 0x100000000UL, false);
    memory_region_add_subregion(&pms->x86_full_region, 0xfffe00000000UL,
                                &pms->x86_region_fffe);

    memory_region_init(&pms->x86_region_0000, OBJECT(pms),
                       "x86-0000-region", 0x100000000UL);
    create_unimplemented_device_custom("x86-0000-unimpl", &pms->x86_region_0000,
                                       0, 0x100000000UL, false);
    memory_region_add_subregion(&pms->x86_full_region, 0x000000000000UL,
                                &pms->x86_region_0000);

    /* PSP SOC */
    object_initialize_child(OBJECT(machine), "soc", &pms->soc, pmc->soc_type);
    object_property_set_link(OBJECT(&pms->soc), "ram",
                             OBJECT(machine->ram), &error_abort);
    object_property_set_link(OBJECT(&pms->soc), "smn-memory",
                             OBJECT(&pms->smn_region), &error_abort);
    object_property_set_link(OBJECT(&pms->soc), "x86-memory",
                             OBJECT(&pms->x86_full_region), &error_abort);
    object_property_set_str(OBJECT(&pms->soc), "cpu-type",
                            machine->cpu_type, &error_abort);
    qdev_realize(DEVICE(&pms->soc), NULL, &error_abort);

    /* FCH ACPI */
    object_initialize_child(OBJECT(machine), "fch-acpi",
                            &pms->fch_acpi, TYPE_FCH_ACPI);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->fch_acpi), &error_fatal)) {
        return;
    }
    memory_region_add_subregion(&pms->x86_region_0000, ACPI_MMIO_ADDR,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->fch_acpi), 0));
    memory_region_add_subregion(&pms->smn_region, 0x02d01000,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->fch_acpi), 1));

    /* FCH SPI */
    object_initialize_child(OBJECT(machine), "fch-spi", &pms->fch_spi, TYPE_FCH_SPI);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->fch_spi), &error_fatal)) {
        return;
    }
    memory_region_add_subregion(&pms->smn_region, 0x02dc4000,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->fch_spi), 0));
    memory_region_add_subregion(&pms->smn_region, pmc->rom_addr,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->fch_spi), 1));

    fch_spi_create_flash(pms, &error_fatal);

    /* SMU io, alias to PSP public regs */
    MemoryRegion *smu_alias = g_new(MemoryRegion, 1);
    memory_region_init_alias(smu_alias, OBJECT(machine), "smu-alias",
                             get_system_memory(), 0x03000000, 0x100000);
    memory_region_add_subregion(&pms->smn_region, 0x3b00000, smu_alias);

    /* UMC */
    object_initialize_child(OBJECT(machine), "umc0", &pms->umc0, TYPE_PSP_UMC);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->umc0), &error_fatal)) {
        return;
    }
    memory_region_add_subregion(&pms->smn_region, 0x050000,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->umc0), 0));

    object_initialize_child(OBJECT(machine), "umc1", &pms->umc1, TYPE_PSP_UMC);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->umc1), &error_fatal)) {
        return;
    }
    memory_region_add_subregion(&pms->smn_region, 0x150000,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->umc1), 0));

    /* PCI Data Fabric */
    object_initialize_child(OBJECT(machine), "amd-df", &pms->df, TYPE_AMD_DF);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->df), &error_fatal)) {
        return;
    }
    memory_region_add_subregion(&pms->smn_region, 0x1c000,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->df), 0));


    /* PCI host bridge */
    PCIHostState *phb = PCI_HOST_BRIDGE(create_pcie_root(pms));

    /* FCH LPC (including serial port) */
    PCIDevice *lpc = pci_create_simple_multifunction(phb->bus, PCI_DEVFN(0x14, 0x3),
                                                     true, TYPE_FCH_LPC);
    assert(lpc);

    /* Is the LPC the owner of the IO space ? */
    memory_region_add_subregion(&pms->x86_region_fffd, X86_IO_BASE, get_system_io());
    create_unimplemented_device_custom("io-unimpl", get_system_io(),
                                       0, 0x10000, true);

    create_dirt(pms);

    psp_load_bootloader(pms);
}

static void psp_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "AMD PSP";
    mc->init = psp_machine_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a7");
    mc->default_ram_id = "psp.ram";
}

static void psp_machine_0a_00_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);

    mc->desc = "AMD PSP (version 0A.00)";
    mc->default_ram_size = 0x40000;
    pmc->version = 0xbc0a0000;
    pmc->rom_addr = 0x0a000000;
    pmc->soc_type = TYPE_PSP_SOC_0A_00;
}

static void psp_machine_0b_05_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);

    mc->desc = "AMD PSP (version 0B.05)";
    mc->default_ram_size = 0x50000;
    pmc->version = 0xbc0b0500;
    pmc->rom_addr = 0x44000000;
    pmc->soc_type = TYPE_PSP_SOC_0B_05;
}

static const TypeInfo psp_machine_typeinfo = {
    .name       = TYPE_PSP_MACHINE,
    .parent     = TYPE_MACHINE,
    .abstract   = true,
    .class_init = psp_machine_class_init,
    .class_size = sizeof(PspMachineClass),
    .instance_size = sizeof(PspMachineState),
};
static const TypeInfo psp_0a_00_machine_typeinfo = {
    .name       = TYPE_PSP_MACHINE_0A_00,
    .parent     = TYPE_PSP_MACHINE,
    .class_init = psp_machine_0a_00_class_init,
};
static const TypeInfo psp_0b_05_machine_typeinfo = {
    .name       = TYPE_PSP_MACHINE_0B_05,
    .parent     = TYPE_PSP_MACHINE,
    .class_init = psp_machine_0b_05_class_init,
};


static void psp_machine_register_types(void)
{
    type_register_static(&psp_machine_typeinfo);
    type_register_static(&psp_0a_00_machine_typeinfo);
    type_register_static(&psp_0b_05_machine_typeinfo);
}

type_init(psp_machine_register_types);
