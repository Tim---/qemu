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
#include "hw/arm/psp-loader.h"
#include "hw/misc/psp-smu.h"
#include "hw/isa/fch-lpc.h"
#include "hw/pci-host/zen-pci-root.h"
#include "hw/mem/psp-umc.h"

/* #define BOOT_SECURED_OS */

#define TYPE_PSP_MACHINE MACHINE_TYPE_NAME("psp")
OBJECT_DECLARE_SIMPLE_TYPE(PspMachineState, PSP_MACHINE)

struct PspMachineState {
    MachineState parent;

    PspSocState soc;
    FchAcpiState fch_acpi;
    FchSpiState fch_spi;
    FchLpcState fch_lpc;
    PspSmuState smu;
    PspUmcState umc0;
    PspUmcState umc1;
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

static MemoryRegion *create_rom(const char *name, MemoryRegion *mem,
                                hwaddr addr, uint64_t size)
{
    MemoryRegion *region = g_new0(MemoryRegion, 1);
    memory_region_init_rom(region, NULL, name, size, &error_fatal);
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

static void create_smn_dirt(PspMachineState *pms)
{
    /* PCI devices in SMN space */
    create_unimplemented_device_custom("D18F0x", &pms->smn_region, 0x1c000, 0x400, false);
    create_unimplemented_device_custom("D18F1x", &pms->smn_region, 0x1c400, 0x400, false);
    create_unimplemented_device_custom("D18F2x", &pms->smn_region, 0x1c800, 0x400, false);
    create_unimplemented_device_custom("D18F3x", &pms->smn_region, 0x1cc00, 0x400, false);
    create_unimplemented_device_custom("D18F4x", &pms->smn_region, 0x1d000, 0x400, false);
    create_unimplemented_device_custom("D18F5x", &pms->smn_region, 0x1d400, 0x400, false);
    create_unimplemented_device_custom("D18F6x", &pms->smn_region, 0x1d800, 0x400, false);

    /* ??? */
    stub_create("stub",         &pms->smn_region,   0x0005a86c, 4, 0x00810f00);
    /* bitmap of cores present */
    stub_create("cores-bitmap", &pms->smn_region,   0x0005a870, 4, 0x00000001);

    /* some fuses */
    stub_create("fuses0",       &pms->smn_region,   0x0005d1f8, 4, 0);
    /* more fuses */
    stub_create("fuses1",       &pms->smn_region,   0x0005d1fc, 4, 0);

    /* 0x03000000 -> 0x04000000 looks full of SMU */
    create_ram("smu-ram",       &pms->smn_region,   0x03c00000, 0x00040000);
    /* MP2_RSMU_FUSESTRAPS */
    stub_create("fusestraps",   &pms->smn_region,   0x03e10024, 4, 1);
    create_ram("smn-more-apob", &pms->smn_region,   0x03f40000, 0x00010000);
    create_ram("mp2-sram",      &pms->smn_region,   0x03f50000, 0x00000800);
}

static void create_x86_dirt(PspMachineState *pms)
{
    /* Well, maybe it's just... RAM ? */
    create_ram("x86-apob",      &pms->x86_region_0000, 0x04000000, 0x00010000);
    create_ram("x86-bin",       &pms->x86_region_0000, 0x09d80000, 0x00400000);
    create_ram("write-pattern", &pms->x86_region_fffd, 0xf7000000, 0x00001000);
    create_ram("more-ram1",     &pms->x86_region_fffd, 0xfb000000, 0x01000000);
}

static void create_dirt(PspMachineState *pms)
{
    create_smn_dirt(pms);
    create_x86_dirt(pms);
#ifdef BOOT_SECURED_OS
    create_ram("secured-os-ram", get_system_memory(), 0x04000000, 0x01000000);
#endif
}

static void psp_machine_init(MachineState *machine)
{
    PspMachineState *pms = PSP_MACHINE(machine);

    memory_region_init(&pms->smn_region, OBJECT(pms),
                       "smn-region", 0x100000000);
    create_unimplemented_device_custom("smn-unimpl", &pms->smn_region,
                                       0, 0x100000000, true);

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
    object_initialize_child(OBJECT(machine), "soc", &pms->soc, TYPE_PSP_SOC);
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

    /* SPI ROM */
    MemoryRegion *rom_region = create_rom("rom", &pms->smn_region,
                                          0x0A000000, 0x01000000);
    int res = load_image_mr(machine->firmware, rom_region);
    assert(res == 0x01000000);

    /* SMU io */
    object_initialize_child(OBJECT(machine), "smu", &pms->smu, TYPE_PSP_SMU);
    if (!sysbus_realize(SYS_BUS_DEVICE(&pms->smu), &error_fatal)) {
        return;
    }
    memory_region_add_subregion(&pms->smn_region, 0x03b10000,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(&pms->smu), 0));

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

    /* PCI host bridge */
    PCIHostState *phb = PCI_HOST_BRIDGE(create_pcie_root(pms));

    /* FCH LPC (including serial port) */
    PCIDevice *lpc = pci_create_simple_multifunction(phb->bus, PCI_DEVFN(0x14, 0x3),
                                                     true, TYPE_FCH_LPC);
    assert(lpc);

    /* Is the LPC the owner of the IO space ? */
    /* TODO: we don't get unimplemented RW */
    memory_region_add_subregion(&pms->x86_region_fffd, X86_IO_BASE, get_system_io());

    create_dirt(pms);

    /* Load bootloader */
    assert(machine->firmware);
#ifdef BOOT_SECURED_OS
    psp_load(machine->firmware, PSP_LOAD_SECURED_OS, 0xbc0a0000);
    cpu_set_pc(CPU(&pms->soc.cpu), 0);
#else
    psp_load(machine->firmware, PSP_LOAD_BOOTLOADER, 0xbc0a0000);
    cpu_set_pc(CPU(&pms->soc.cpu), 0x100);
#endif
}

static void psp_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "AMD PSP";
    mc->init = psp_machine_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a7");
    mc->default_ram_size = 0x40000;
    mc->default_ram_id = "psp.ram";
}

static const TypeInfo psp_machine_typeinfo = {
    .name       = TYPE_PSP_MACHINE,
    .parent     = TYPE_MACHINE,
    .class_init = psp_machine_class_init,
    .instance_size = sizeof(PspMachineState),
};

static void psp_machine_register_types(void)
{
    type_register_static(&psp_machine_typeinfo);
}

type_init(psp_machine_register_types);
