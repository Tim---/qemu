#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie_host.h"
#include "hw/sysbus.h"
#include "hw/zen/zen-pci-root.h"
#include "hw/registerfields.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qapi/error.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenRootState, ZEN_ROOT_DEVICE)

struct ZenRootState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    uint32_t smu_index_addr;
    MemoryRegion *smn;
};

OBJECT_DECLARE_SIMPLE_TYPE(ZenHost, ZEN_HOST)

struct ZenHost {
    PCIExpressHost parent_obj;
    bool allow_unmapped_accesses;
};

/*
 * Pci Host
 */

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

/*
 * PCI root
 */

REG32(SMU_INDEX_ADDR,   0xB8)
REG32(SMU_INDEX_DATA,   0xBC)

static void zen_pci_root_config_write(PCIDevice *d, uint32_t addr,
                                 uint32_t val, int len)
{
    ZenRootState *s = ZEN_ROOT_DEVICE(d);

    if(addr < 0x40) {
        pci_default_write_config(d, addr, val, len);
        return;
    }

    switch(addr) {
    case A_SMU_INDEX_ADDR:
        s->smu_index_addr = val;
        return;
    case A_SMU_INDEX_DATA:
        memory_region_dispatch_write(s->smn, s->smu_index_addr, val,
                                     size_memop(len), MEMTXATTRS_UNSPECIFIED);
        return;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%x, value 0x%x)\n",
                  __func__, addr, val);
}

static uint32_t zen_pci_root_config_read(PCIDevice *d, uint32_t addr, int len)
{
    ZenRootState *s = ZEN_ROOT_DEVICE(d);
    uint64_t res = 0;

    if(addr < 0x40) {
        return pci_default_read_config(d, addr, len);
    }

    switch(addr) {
    case A_SMU_INDEX_ADDR:
        return s->smu_index_addr;
    case A_SMU_INDEX_DATA:
        memory_region_dispatch_read(s->smn, s->smu_index_addr, &res,
                                    size_memop(len), MEMTXATTRS_UNSPECIFIED);
        return res;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%x)\n", __func__, addr);
    return res;
}

static void zen_pci_root_realize(PCIDevice *dev, Error **errp)
{
}

static void zen_pci_root_init(Object *obj)
{
}

static Property zen_pci_root_props[] = {
    DEFINE_PROP_LINK("smn", ZenRootState, smn,
                     TYPE_MEMORY_REGION, MemoryRegion *),
    DEFINE_PROP_END_OF_LIST(),
};

static void zen_pci_root_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->desc = "AMD Zen PCIe host bridge";
    k->vendor_id = PCI_VENDOR_ID_AMD;
    k->device_id = 0x1450;
    k->revision = 0;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    k->config_write = zen_pci_root_config_write;
    k->config_read  = zen_pci_root_config_read;
    k->realize = zen_pci_root_realize;
    device_class_set_props(dc, zen_pci_root_props);
}

static const TypeInfo zen_pci_root_info = {
    .name = TYPE_ZEN_ROOT_DEVICE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(ZenRootState),
    .instance_init = zen_pci_root_init,
    .class_init = zen_pci_root_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void zen_pci_register(void)
{
    type_register_static(&zen_pci_host_info);
    type_register_static(&zen_pci_root_info);
}

type_init(zen_pci_register)
