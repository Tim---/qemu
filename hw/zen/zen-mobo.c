#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "sysemu/sysemu.h"
#include "hw/zen/zen-mobo.h"
#include "hw/zen/zen-utils.h"
#include "qapi/error.h"
#include "hw/zen/fch.h"
#include "hw/zen/zen-pci-root.h"
#include "hw/isa/isa.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pcie_host.h"
#include "exec/address-spaces.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenMoboState, ZEN_MOBO)

struct ZenMoboState {
    SysBusDevice parent_obj;

    MemoryRegion smn;
    MemoryRegion *ht;
    MemoryRegion ht_io;
    MemoryRegion ht_mmconfig;

    ISABus *isa_bus;
    PCIBus *pci_bus;
};

static void create_smn(ZenMoboState *s)
{
    create_region_with_unimpl(&s->smn, OBJECT(s), "smn", 0x1000000000UL);
}

static void create_ht(ZenMoboState *s)
{
    s->ht = get_system_memory();
    create_unimplemented_device_generic(s->ht, "ht-unimpl", 0, 0x1000000000000ULL);

    memory_region_init_alias(&s->ht_io, OBJECT(s), "ht-io", get_system_io(), 0, 0x10000);
    create_unimplemented_device_generic(get_system_io(), "ht-io-unimpl", 0, 0x10000);

    memory_region_add_subregion(s->ht, 0xfffdfc000000, &s->ht_io);
    create_region_with_unimpl(&s->ht_mmconfig, OBJECT(s), "ht-mmconfig", 0x20000000ULL);
    memory_region_add_subregion(s->ht, 0xfffe00000000, &s->ht_mmconfig);
}

MemoryRegion *zen_mobo_get_ht(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return s->ht;
}

MemoryRegion *zen_mobo_get_smn(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return &s->smn;
}

ISABus *zen_mobo_get_isa(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return s->isa_bus;
}

PCIBus *zen_mobo_get_pci(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return s->pci_bus;
}

static void generic_map(MemoryRegion *container, SysBusDevice *sbd, int n, hwaddr addr, bool alias)
{
    MemoryRegion *region = sysbus_mmio_get_region(sbd, n);

    if(alias) {
        MemoryRegion *alias = g_new(MemoryRegion, 1);
        g_autofree char *alias_name = g_strdup_printf("alias-%s",
                                                    memory_region_name(region));

        memory_region_init_alias(alias, memory_region_owner(region), alias_name,
                                region, 0, memory_region_size(region));
        
        region = alias;
    }

    memory_region_add_subregion(container, addr, region);
}

void zen_mobo_smn_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias)
{
    generic_map(zen_mobo_get_smn(dev), sbd, n, addr, alias);
}

void zen_mobo_ht_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias)
{
    generic_map(zen_mobo_get_ht(dev), sbd, n, addr, alias);
}

static void zen_mobo_create_serial(ZenMoboState *s)
{
    serial_mm_init(get_system_io(), 0x3f8, 0, NULL, 9600, serial_hd(0), DEVICE_NATIVE_ENDIAN);
}

static void create_fch(DeviceState *s)
{
    DeviceState *dev = qdev_new(TYPE_FCH);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    zen_mobo_ht_map(s, SYS_BUS_DEVICE(dev), 0, 0xfed80000, false);
    zen_mobo_smn_map(s, SYS_BUS_DEVICE(dev), 0, 0x02d01000, true);
}

static void create_isa(ZenMoboState *s)
{
    s->isa_bus = isa_bus_new(DEVICE(s), s->ht, get_system_io(), &error_fatal);
}

static void create_pcie(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_ZEN_HOST);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    /* TODO: trace the unimplemented R/W */
    pcie_host_mmcfg_map(PCIE_HOST_BRIDGE(dev), 0xf0000000, 0x4000000);
    PCIHostState *phb = PCI_HOST_BRIDGE(dev);
    s->pci_bus = phb->bus;
}

static void zen_mobo_init(Object *obj)
{
}

static void zen_mobo_realize(DeviceState *dev, Error **errp)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    create_smn(s);
    create_ht(s);

    zen_mobo_create_serial(s);
    create_fch(DEVICE(s));
    create_pcie(s);
    create_isa(s);
}

static void zen_mobo_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = zen_mobo_realize;
    dc->desc = "Zen motherboard";
}

static const TypeInfo zen_mobo_type_info = {
    .name = TYPE_ZEN_MOBO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ZenMoboState),
    .instance_init = zen_mobo_init,
    .class_init = zen_mobo_class_init,
};

static void zen_mobo_register_types(void)
{
    type_register_static(&zen_mobo_type_info);
}

type_init(zen_mobo_register_types)
