#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "sysemu/sysemu.h"
#include "hw/zen/zen-mobo.h"
#include "hw/zen/zen-utils.h"
#include "qapi/error.h"
#include "hw/zen/fch.h"
#include "hw/isa/isa.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenMoboState, ZEN_MOBO)

struct ZenMoboState {
    SysBusDevice parent_obj;

    MemoryRegion smn;
    MemoryRegion ht;
    MemoryRegion ht_io;
    MemoryRegion ht_mmconfig;

    ISABus *isa_bus;
};

static void create_smn(ZenMoboState *s)
{
    create_region_with_unimpl(&s->smn, OBJECT(s), "smn", 0x1000000000UL);
}

static void create_ht(ZenMoboState *s)
{
    create_region_with_unimpl(&s->ht, OBJECT(s), "ht", 0x1000000000000ULL);
    create_region_with_unimpl(&s->ht_io, OBJECT(s), "ht-io", 0x2000000ULL);
    memory_region_add_subregion(&s->ht, 0xfffdfc000000, &s->ht_io);
    create_region_with_unimpl(&s->ht_mmconfig, OBJECT(s), "ht-mmconfig", 0x20000000ULL);
    memory_region_add_subregion(&s->ht, 0xfffe00000000, &s->ht_mmconfig);
}

MemoryRegion *zen_mobo_get_ht(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return &s->ht;
}

MemoryRegion *zen_mobo_get_smn(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return &s->smn;
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
    serial_mm_init(&s->ht_io, 0x3f8, 0, NULL, 9600, serial_hd(0), DEVICE_NATIVE_ENDIAN);
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
    s->isa_bus = isa_bus_new(DEVICE(s), &s->ht, &s->ht_io, &error_fatal);
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
