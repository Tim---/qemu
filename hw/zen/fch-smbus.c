#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/fch-smbus.h"

#define REGS_SIZE 0x20

OBJECT_DECLARE_SIMPLE_TYPE(FchSmbusState, FCH_SMBUS)

struct FchSmbusState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    uint8_t storage[REGS_SIZE];
};

static uint64_t fch_smbus_io_read(void *opaque, hwaddr addr, unsigned size)
{
    FchSmbusState *s = FCH_SMBUS(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);

    return ldn_le_p(s->storage + addr, size);
}

static void fch_smbus_io_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    FchSmbusState *s = FCH_SMBUS(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, addr, size, value);

    stn_le_p(s->storage + addr, size, value);
}

static const MemoryRegionOps fch_smbus_io_ops = {
    .read = fch_smbus_io_read,
    .write = fch_smbus_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fch_smbus_init(Object *obj)
{
}

static void fch_smbus_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    FchSmbusState *s = FCH_SMBUS(dev);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_smbus_io_ops, s,
                          "fch-smbus-regs", REGS_SIZE);

    sysbus_add_io(sbd, 0xb00, &s->regs_region);
    sysbus_init_ioports(sbd, 0xb00, REGS_SIZE);
}

static void fch_smbus_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_smbus_realize;
    dc->desc = "FCH SMBus";
}

static const TypeInfo fch_smbus_type_info = {
    .name = TYPE_FCH_SMBUS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchSmbusState),
    .instance_init = fch_smbus_init,
    .class_init = fch_smbus_class_init,
};

static void fch_smbus_register_types(void)
{
    type_register_static(&fch_smbus_type_info);
}

type_init(fch_smbus_register_types)
