#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/fch-rtc.h"

OBJECT_DECLARE_SIMPLE_TYPE(FchRtcState, FCH_RTC)

struct FchRtcState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
};


static uint64_t fch_rtc_io_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);
    return 0xffffffff;
}

static void fch_rtc_io_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, addr, size, value);

}

static const MemoryRegionOps fch_rtc_io_ops = {
    .read = fch_rtc_io_read,
    .write = fch_rtc_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fch_rtc_init(Object *obj)
{
}

static void fch_rtc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    FchRtcState *s = FCH_RTC(dev);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_rtc_io_ops, s,
                          "fch-rtc-regs", 4);
    sysbus_init_mmio(sbd, &s->regs_region);
}

static void fch_rtc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_rtc_realize;
    dc->desc = "FCH RTC";
}

static const TypeInfo fch_rtc_type_info = {
    .name = TYPE_FCH_RTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchRtcState),
    .instance_init = fch_rtc_init,
    .class_init = fch_rtc_class_init,
};

static void fch_rtc_register_types(void)
{
    type_register_static(&fch_rtc_type_info);
}

type_init(fch_rtc_register_types)
