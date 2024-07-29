#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "qemu/log.h"
#include "hw/zen/fch-lpc-bridge.h"

OBJECT_DECLARE_SIMPLE_TYPE(FchLpcBridgeState, FCH_LPC_BRIDGE)

struct FchLpcBridgeState {
    /*< private >*/
    PCIDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    uint8_t storage[0x1000];
};


static uint32_t fch_lpc_bridge_config_read(PCIDevice *d, uint32_t addr, int size)
{
    FchLpcBridgeState *s = FCH_LPC_BRIDGE(d);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%x, size 0x%x)\n", __func__, addr, size);

    return ldn_le_p(s->storage + addr, size);
}

static void fch_lpc_bridge_config_write(PCIDevice *d, uint32_t addr,
                                    uint32_t value, int size)
{
    FchLpcBridgeState *s = FCH_LPC_BRIDGE(d);
    stn_le_p(s->storage + addr, size, value);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%x, size 0x%x, value 0x%x)\n", __func__, addr, size, value);

}

static void fch_lpc_bridge_init(Object *obj)
{
}

static void fch_lpc_bridge_realize(PCIDevice *dev, Error **errp)
{
}

static void fch_lpc_bridge_exit(PCIDevice *dev)
{
}

static void fch_lpc_bridge_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(oc);

    dc->desc = "FCH LPC bridge";
    k->realize      = fch_lpc_bridge_realize;
    k->exit         = fch_lpc_bridge_exit;
    k->vendor_id    = PCI_VENDOR_ID_AMD;
    k->device_id    = 0x790E;
    k->class_id     = PCI_CLASS_BRIDGE_ISA;
    k->config_write = fch_lpc_bridge_config_write;
    k->config_read  = fch_lpc_bridge_config_read;
}

static const TypeInfo fch_lpc_bridge_type_info = {
    .name = TYPE_FCH_LPC_BRIDGE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(FchLpcBridgeState),
    .instance_init = fch_lpc_bridge_init,
    .class_init = fch_lpc_bridge_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { }
    },
};

static void fch_lpc_bridge_register_types(void)
{
    type_register_static(&fch_lpc_bridge_type_info);
}

type_init(fch_lpc_bridge_register_types)
