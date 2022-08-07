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

    uint32_t smu_ok_offset;

    uint32_t mb_cmd_offset;
    uint32_t mb_status_offset;
    uint32_t mb_data_offset;

    uint32_t cmd;
    uint32_t status;
    uint32_t data[6];
};

static void smu_execute(SmuState *s)
{
    qemu_log_mask(LOG_UNIMP, "%s: execute\n", __func__);
    s->status = 1;
}

static uint64_t smu_read(void *opaque, hwaddr addr, unsigned size)
{
    SmuState *s = SMU(opaque);

    if(addr == 0x704) {
        return 1;
    } else if(addr == s->smu_ok_offset) {
        return 1;
    } else if(addr == s->mb_status_offset) {
        return s->status;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);

    return 0;
}

static void smu_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    SmuState *s = SMU(opaque);

    if(addr == s->mb_cmd_offset) {
        s->cmd = value;
        smu_execute(s);
    } else if(addr >= s->mb_data_offset && addr < s->mb_data_offset + 6 * 4) {
        s->data[(addr - s->mb_data_offset) / 4] = value;
    }

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
    SmuState *s = SMU(dev);

    /* Mp1FWFlagsReg */
    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        s->smu_ok_offset = 0x34;
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        s->smu_ok_offset = 0x28;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        s->smu_ok_offset = 0x24;
        break;
    default:
        g_assert_not_reached();
    }

    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        /* SmuV9 */
        s->mb_cmd_offset    = 0x528;
        s->mb_status_offset = 0x564;
        s->mb_data_offset   = 0x598;
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        /* SmuV10 */
        s->mb_cmd_offset    = 0x528;
        s->mb_status_offset = 0x564;
        s->mb_data_offset   = 0x998;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        /* SmuV11 */
        s->mb_cmd_offset    = 0x530;
        s->mb_status_offset = 0x57c;
        s->mb_data_offset   = 0x9c4;
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        /* SmuV12 */
        s->mb_cmd_offset    = 0x528;
        s->mb_status_offset = 0x564;
        s->mb_data_offset   = 0x998;
        break;
    default:
        g_assert_not_reached();
    }

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
