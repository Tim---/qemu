#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/smu.h"
#include "hw/zen/zen-cpuid.h"

OBJECT_DECLARE_SIMPLE_TYPE(SmuState, SMU)

struct SmuState {
    SysBusDevice parent_obj;
    zen_codename codename;
    MemoryRegion regs;
};

static uint64_t smu_read(void *opaque, hwaddr addr, unsigned size)
{
    SmuState *s = SMU(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);

    if(addr == 0x704) {
        return 1;
    }

    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        if(addr == 0x34) {
            // Mp1FWFlagsReg
            return 1;
        }
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        if(addr == 0x28) {
            // Mp1FWFlagsReg
            return 1;
        }
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        if(addr == 0x24) {
            // Mp1FWFlagsReg
            return 1;
        }
        break;
    default:
        g_assert_not_reached();
    }
    return 0;
}

static void smu_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, addr, value);
}

static const MemoryRegionOps smu_data_ops = {
    .read = smu_read,
    .write = smu_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void smu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SmuState *s = SMU(obj);

    /* Is this region an alias of 0x03010000 ? */
    memory_region_init_io(&s->regs, OBJECT(s), &smu_data_ops, s,
                          "smu",
                          0x1000);
    sysbus_init_mmio(sbd, &s->regs);
}

static void smu_realize(DeviceState *dev, Error **errp)
{
}

static Property psp_soc_props[] = {
    DEFINE_PROP_UINT32("codename", SmuState, codename, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void smu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = smu_realize;
    dc->desc = "Zen SMU";
    device_class_set_props(dc, psp_soc_props);
}

static const TypeInfo smu_type_info = {
    .name = TYPE_SMU,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SmuState),
    .instance_init = smu_init,
    .class_init = smu_class_init,
};

static void smu_register_types(void)
{
    type_register_static(&smu_type_info);
}

type_init(smu_register_types)
