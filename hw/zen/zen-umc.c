#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "hw/zen/zen-umc.h"
#include "hw/registerfields.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenUmcState, ZEN_UMC)

struct ZenUmcState {
    SysBusDevice parent_obj;

    MemoryRegion region;
};

REG32(CTRL_EMU_TYPE,         0x1050)
typedef enum {
    EMU_SIMULATION = 0xb1aab1aa,
    EMU_EMULATION1 = 0x4e554e55,
    EMU_EMULATION2 = 0x51e051e0,
    EMU_WITH_PORT80_DEBUG = 0x5a335a33,
} emu_type_e;

static uint64_t zen_umc_read(void *opaque, hwaddr offset, unsigned size)
{
    switch(offset) {
    case A_CTRL_EMU_TYPE:
        return 0;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return 0;
}

static void zen_umc_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, value);
}

static const MemoryRegionOps zen_umc_ops = {
    .read = zen_umc_read,
    .write = zen_umc_write,
};

static void zen_umc_init(Object *obj)
{
}

static void zen_umc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    ZenUmcState *s = ZEN_UMC(dev);

    /* Init the registers access */
    memory_region_init_io(&s->region, OBJECT(s), &zen_umc_ops, s,
                          "zen-umc", 0x2000);
    sysbus_init_mmio(sbd, &s->region);
}

static void zen_umc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = zen_umc_realize;
    dc->desc = "Zen UMC";
}

static const TypeInfo zen_umc_type_info = {
    .name = TYPE_ZEN_UMC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ZenUmcState),
    .instance_init = zen_umc_init,
    .class_init = zen_umc_class_init,
};

static void zen_umc_register_types(void)
{
    type_register_static(&zen_umc_type_info);
}

type_init(zen_umc_register_types)
