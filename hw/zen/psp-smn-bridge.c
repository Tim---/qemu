#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/misc/unimp.h"
#include "hw/zen/psp-smn-bridge.h"

OBJECT_DECLARE_SIMPLE_TYPE(SmnBridgeState, SMN_BRIDGE)

/* Number of low bits */
#define SMN_BRIDGE_MAPPING_BITS 0x14
#define SMN_BRIDGE_MAPPING_SIZE (1 << SMN_BRIDGE_MAPPING_BITS)
#define SMN_BRIDGE_NUM_MAPPINGS 0x20

struct SmnBridgeState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion mapped_region;
    MemoryRegion aliases[SMN_BRIDGE_NUM_MAPPINGS];

    MemoryRegion *source_memory;
};


static uint64_t smn_bridge_io_read(void *opaque, hwaddr addr, unsigned size)
{
    SmnBridgeState *s = SMN_BRIDGE(opaque);
    return s->aliases[addr >> 1].alias_offset >> SMN_BRIDGE_MAPPING_BITS;
}

static void smn_bridge_io_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    SmnBridgeState *s = SMN_BRIDGE(opaque);
    memory_region_set_alias_offset(&s->aliases[addr >> 1],
                                   value << SMN_BRIDGE_MAPPING_BITS);
}

static const MemoryRegionOps smn_bridge_io_ops = {
    .read = smn_bridge_io_read,
    .write = smn_bridge_io_write,
    .valid = {
        .min_access_size = 2,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 2,
        .max_access_size = 2,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void smn_bridge_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SmnBridgeState *s = SMN_BRIDGE(obj);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &smn_bridge_io_ops, s,
                          "smn-bridge-regs",
                          sizeof(uint16_t) * SMN_BRIDGE_NUM_MAPPINGS);
    sysbus_init_mmio(sbd, &s->regs_region);

    /* Init the mapped memory */
    memory_region_init(&s->mapped_region, OBJECT(s), "smn-bridge-mapped-space",
                       SMN_BRIDGE_NUM_MAPPINGS * SMN_BRIDGE_MAPPING_SIZE);
    sysbus_init_mmio(sbd, &s->mapped_region);

}

static void smn_bridge_realize(DeviceState *dev, Error **errp)
{
    SmnBridgeState *s = SMN_BRIDGE(dev);

    for (int idx = 0; idx < SMN_BRIDGE_NUM_MAPPINGS; idx++) {
        /* We first create an alias object */
        memory_region_init_alias(&s->aliases[idx], OBJECT(s), "alias[*]",
                                 s->source_memory, 0, SMN_BRIDGE_MAPPING_SIZE);
        /* We then add this object in the mapped space */
        memory_region_add_subregion(&s->mapped_region, idx * SMN_BRIDGE_MAPPING_SIZE, &s->aliases[idx]);
    }
}

static Property smn_bridge_props[] = {
    DEFINE_PROP_LINK("source-memory", SmnBridgeState, source_memory,
                     TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static void smn_bridge_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = smn_bridge_realize;
    dc->desc = "PSP SMN bridge";
    device_class_set_props(dc, smn_bridge_props);
}

static const TypeInfo smn_bridge_type_info = {
    .name = TYPE_SMN_BRIDGE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SmnBridgeState),
    .instance_init = smn_bridge_init,
    .class_init = smn_bridge_class_init,
};

static void smn_bridge_register_types(void)
{
    type_register_static(&smn_bridge_type_info);
}

type_init(smn_bridge_register_types)
