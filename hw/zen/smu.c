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

    /* Mp1FWFlagsReg */
    uint32_t smu_ok_offset;

    /* Mailbox 1 */
    uint32_t mb1_cmd_offset;
    uint32_t mb1_status_offset;
    uint32_t mb1_data_offset;

    uint32_t mb1_cmd;
    uint32_t mb1_status;
    uint32_t mb1_data[6];

    /* Mailbox 2 */
    uint32_t mb2_cmd_offset;
    uint32_t mb2_status_offset;
    uint32_t mb2_data_offset;

    uint32_t mb2_cmd;
    uint32_t mb2_status;
    uint32_t mb2_data;
};

static void smu_mb1_execute(SmuState *s)
{
    qemu_log_mask(LOG_UNIMP, "%s: execute\n", __func__);
    s->mb1_status = 1;
}

static void smu_mb2_execute(SmuState *s)
{
    qemu_log_mask(LOG_UNIMP, "%s: execute (cmd=0x%x, data=0x%x)\n", __func__, s->mb2_cmd, s->mb2_data);
    s->mb2_status = 1;
    s->mb2_data = 0;
}

static uint64_t smu_read(void *opaque, hwaddr addr, unsigned size)
{
    SmuState *s = SMU(opaque);

    if(addr == s->smu_ok_offset) {
        return 1;
    } else if(addr == s->mb1_status_offset) {
        return s->mb1_status;
    } else if(addr == s->mb2_status_offset) {
        return s->mb2_status;
    } else if(addr == s->mb2_data_offset) {
        return s->mb2_data;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);

    return 0;
}

static void smu_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    SmuState *s = SMU(opaque);

    if(addr == s->mb1_cmd_offset) {
        s->mb1_cmd = value;
        smu_mb1_execute(s);
    } else if(addr >= s->mb1_data_offset && addr < s->mb1_data_offset + 6 * 4) {
        s->mb1_data[(addr - s->mb1_data_offset) / 4] = value;
    } else if(addr == s->mb2_cmd_offset) {
        s->mb2_cmd = value;
        smu_mb2_execute(s);
        return;
    } else if(addr == s->mb2_status_offset) {
        s->mb2_status = value;
        return;
    } else if(addr == s->mb2_data_offset) {
        s->mb2_data = value;
        return;
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

    s->mb2_data_offset = 0x700;
    s->mb2_status_offset = 0x704;

    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        /* SmuV9 */
        s->smu_ok_offset = 0x34;

        s->mb1_cmd_offset    = 0x528;
        s->mb1_status_offset = 0x564;
        s->mb1_data_offset   = 0x598;

        s->mb2_cmd_offset   = 0x714;
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        /* SmuV10 */
        s->smu_ok_offset = 0x28;

        s->mb1_cmd_offset    = 0x528;
        s->mb1_status_offset = 0x564;
        s->mb1_data_offset   = 0x998;

        s->mb2_cmd_offset   = 0x714;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        /* SmuV11 */
        s->smu_ok_offset = 0x24;

        s->mb1_cmd_offset    = 0x530;
        s->mb1_status_offset = 0x57c;
        s->mb1_data_offset   = 0x9c4;

        s->mb2_cmd_offset   = 0x718;
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        /* SmuV12 */
        s->smu_ok_offset = 0x24;

        s->mb1_cmd_offset    = 0x528;
        s->mb1_status_offset = 0x564;
        s->mb1_data_offset   = 0x998;

        s->mb2_cmd_offset   = 0x71c;
        break;
    default:
        g_assert_not_reached();
    }

    s->mb2_status = 1;
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
