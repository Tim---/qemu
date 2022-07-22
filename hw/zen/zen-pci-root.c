#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie_host.h"
#include "hw/sysbus.h"
#include "hw/zen/zen-pci-root.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenHost, ZEN_HOST)

struct ZenHost {
    PCIExpressHost parent_obj;
    bool allow_unmapped_accesses;
};

static void zen_host_realize(DeviceState *dev, Error **errp)
{
    PCIHostState *pci = PCI_HOST_BRIDGE(dev);
    ZenHost *s = ZEN_HOST(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    PCIExpressHost *pex = PCIE_HOST_BRIDGE(dev);

    pcie_host_mmcfg_init(pex, PCIE_MMCFG_SIZE_MAX);
    sysbus_init_mmio(sbd, &pex->mmio);

    pci->bus = pci_register_root_bus(dev, "pcie.0", NULL,
                                     pci_swizzle_map_irq_fn, s,
                                     get_system_memory(), get_system_io(),
                                     0, 4, TYPE_PCIE_BUS);

    memory_region_init_io(&pci->conf_mem, OBJECT(dev), &pci_host_conf_le_ops, s,
                          "pci-conf-idx", 4);
    memory_region_init_io(&pci->data_mem, OBJECT(dev), &pci_host_data_le_ops, s,
                          "pci-conf-data", 4);

    sysbus_add_io(sbd, 0xcf8, &pci->conf_mem);
    sysbus_init_ioports(sbd, 0xcf8, 4);

    sysbus_add_io(sbd, 0xcfc, &pci->data_mem);
    sysbus_init_ioports(sbd, 0xcfc, 4);
}

static const char *zen_host_root_bus_path(PCIHostState *host_bridge,
                                          PCIBus *rootbus)
{
    return "0000:00";
}

static void zen_host_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(klass);

    hc->root_bus_path = zen_host_root_bus_path;
    dc->realize = zen_host_realize;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->fw_name = "pci";
}

static void zen_host_initfn(Object *obj)
{
}

static const TypeInfo zen_pci_host_info = {
    .name       = TYPE_ZEN_HOST,
    .parent     = TYPE_PCIE_HOST_BRIDGE,
    .instance_size = sizeof(ZenHost),
    .instance_init = zen_host_initfn,
    .class_init = zen_host_class_init,
};

static void zen_pci_register(void)
{
    type_register_static(&zen_pci_host_info);
}

type_init(zen_pci_register)
