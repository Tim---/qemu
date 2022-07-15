#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/fch.h"

struct FchState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
};

OBJECT_DECLARE_SIMPLE_TYPE(FchState, FCH)

static uint64_t fch_io_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);
    return 0;
}

static void fch_io_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, addr, size, value);
}


static const MemoryRegionOps fch_io_ops = {
    .read = fch_io_read,
    .write = fch_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fch_init(Object *obj)
{
}

static void fch_realize(DeviceState *dev, Error **errp)
{
    FchState *s = FCH(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->regs_region, OBJECT(s),
                          &fch_io_ops, s,
                          "fch", 0x2000);
    sysbus_init_mmio(sbd, &s->regs_region);
}

static void fch_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_realize;
    dc->desc = "Zen FCH";
}

static const TypeInfo fch_type_info = {
    .name = TYPE_FCH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchState),
    .instance_init = fch_init,
    .class_init = fch_class_init,
};

static void fch_register_types(void)
{
    type_register_static(&fch_type_info);
}

type_init(fch_register_types)
