#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/fch-lpc-bridge.h"

OBJECT_DECLARE_SIMPLE_TYPE(FchLpcBridgeState, FCH_LPC_BRIDGE)

struct FchLpcBridgeState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
};


static uint64_t fch_lpc_bridge_io_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);
    return 0;
}

static void fch_lpc_bridge_io_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, addr, size, value);

}

static const MemoryRegionOps fch_lpc_bridge_io_ops = {
    .read = fch_lpc_bridge_io_read,
    .write = fch_lpc_bridge_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fch_lpc_bridge_init(Object *obj)
{
}

static void fch_lpc_bridge_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    FchLpcBridgeState *s = FCH_LPC_BRIDGE(dev);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_lpc_bridge_io_ops, s,
                          "fch-lpc-bridge-regs",
                          0x1000);
    sysbus_init_mmio(sbd, &s->regs_region);
}

static void fch_lpc_bridge_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_lpc_bridge_realize;
    dc->desc = "FCH LPC bridge";
}

static const TypeInfo fch_lpc_bridge_type_info = {
    .name = TYPE_FCH_LPC_BRIDGE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchLpcBridgeState),
    .instance_init = fch_lpc_bridge_init,
    .class_init = fch_lpc_bridge_class_init,
};

static void fch_lpc_bridge_register_types(void)
{
    type_register_static(&fch_lpc_bridge_type_info);
}

type_init(fch_lpc_bridge_register_types)
