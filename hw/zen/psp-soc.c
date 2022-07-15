#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "hw/zen/psp-soc.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/psp-regs.h"
#include "hw/zen/psp-smn-bridge.h"
#include "hw/zen/psp-ht-bridge.h"
#include "hw/zen/psp-fuses.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-timer.h"
#include "hw/zen/ccp.h"

OBJECT_DECLARE_SIMPLE_TYPE(PspSocState, PSP_SOC)

struct PspSocState {
    /*< private >*/
    SysBusDevice parent_obj;

    char *cpu_type;
    MemoryRegion ram;
    zen_codename codename;
    MemoryRegion *smn_region;
    MemoryRegion *ht_region;

    MemoryRegion container;
};

static void create_memory(PspSocState *s)
{
    /* Create the memory container for the instance of this SoC */
    memory_region_init(&s->container, OBJECT(s), "psp-mem", 0x100000000ULL);

    /* Create RAM at the start */
    memory_region_init_ram(&s->ram, OBJECT(s), "psp-soc-ram", zen_get_ram_size(s->codename), &error_fatal);
    memory_region_add_subregion(&s->container, 0, &s->ram);
}

static void create_cpu(PspSocState *s, const char *cpu_type)
{
    /* Create CPU */
    Object *cpuobj;
    cpuobj = object_new(cpu_type);
    object_property_set_link(cpuobj, "memory", OBJECT(&s->container),
                             &error_abort);
    qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
}

static void psp_mmio_map(PspSocState *s, SysBusDevice *sbd, int n, hwaddr addr)
{
    MemoryRegion *region = sysbus_mmio_get_region(sbd, n);
    memory_region_add_subregion(&s->container, addr, region);
}

static void create_psp_regs(PspSocState *s, zen_codename codename)
{
    /* Create PSP registers */
    DeviceState *dev = qdev_new(TYPE_PSP_REGS);
    qdev_prop_set_uint32(dev, "codename", codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 0, 0x03010000);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 1, 0x03200000);
}

static void create_smn(PspSocState *s)
{
    DeviceState *dev = qdev_new(TYPE_SMN_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(s->smn_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 0, 0x03220000);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 1, 0x01000000);
}

static void create_ht(PspSocState *s)
{
    DeviceState *dev = qdev_new(TYPE_HT_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(s->ht_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 0, 0x03230000);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 1, 0x04000000);
}

static void create_fuses(PspSocState *s, zen_codename codename)
{
    DeviceState *dev = qdev_new(TYPE_PSP_FUSES);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    MemoryRegion *region = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_add_subregion(s->smn_region, 0x5d000, region);

    psp_dirty_fuses(codename, dev);
}

static void create_timer(PspSocState *s, int i)
{
    DeviceState *dev = qdev_new(TYPE_PSP_TIMER);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 0, 0x03010400 + i * 0x24);
}

static void create_ccp(PspSocState *s, zen_codename codename)
{
    DeviceState *dev = qdev_new(TYPE_CCP);
    object_property_set_link(OBJECT(dev), "system-memory",
                             OBJECT(&s->container), &error_fatal);
    qdev_prop_set_uint32(dev, "num-queues", zen_get_num_ccp_queues(codename));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    psp_mmio_map(s, SYS_BUS_DEVICE(dev), 0, 0x03000000);
}

static void psp_soc_init(Object *obj)
{
}

static void psp_soc_realize(DeviceState *dev, Error **errp)
{
    PspSocState *s = PSP_SOC(dev);
    create_memory(s);
    create_cpu(s, s->cpu_type);
    create_psp_regs(s, s->codename);
    create_smn(s);
    create_ht(s);
    create_fuses(s, s->codename);
    create_ccp(s, s->codename);

    for(int i = 0; i < 2; i++) {
        create_timer(s, i);
    }
}

static Property psp_soc_props[] = {
    DEFINE_PROP_STRING("cpu-type", PspSocState, cpu_type),
    DEFINE_PROP_UINT32("codename", PspSocState, codename, 0),
    DEFINE_PROP_LINK("smn-memory", PspSocState, smn_region, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_LINK("ht-memory", PspSocState, ht_region, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
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
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspSocState),
    .instance_init = psp_soc_init,
    .class_init = psp_soc_class_init,
};

static void psp_soc_register_types(void)
{
    type_register_static(&psp_soc_type_info);
}

type_init(psp_soc_register_types)
