#include "qemu/osdep.h"
#include "hw/zen/df.h"
#include "qemu/log.h"
#include "hw/registerfields.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "hw/sysbus.h"

OBJECT_DECLARE_SIMPLE_TYPE(DfState, DF)

struct DfState {
    SysBusDevice parent_obj;

    MemoryRegion regs;
    uint32_t fica_addr;
    PCIBus *bus;
};

OBJECT_DECLARE_SIMPLE_TYPE(DfFuncState, DF_FUNC)

struct DfFuncState {
    PCIDevice parent_obj;

    DfState *parent;
};

#define FUN_REG(fun, reg) (((fun) << 12) | ((reg) & 0xfff))

REG32(D18F1x200, FUN_REG(1, 0x200))
REG32(D18F1x204, FUN_REG(1, 0x204))

static uint64_t df_read(DfState *s, int fun, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(FUN_REG(fun, offset)) {
    case A_D18F1x200:
        res = 0x00010001;
        break;
    case A_D18F1x204:
        res = 0x00010001;
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(size %d, offset D18F%dx%lx)\n",
                  __func__, size, fun, offset);
    return res;
}

static void df_write(DfState *s, int fun, hwaddr offset,
                         uint64_t data, unsigned size)
{
    switch(FUN_REG(fun, offset)) {
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(size %d, offset D18F%dx%lx, value 0x%lx)\n",
                  __func__, size, fun, offset, data);
}

static void df_init(Object *obj)
{
}

static void df_realize(DeviceState *dev, Error **errp)
{
    DfState *s = DF(dev);

    int fun;

    for(fun = 0; fun < 8; fun++) {
        PCIDevice *child = pci_new_multifunction(PCI_DEVFN(0x18, fun), true,
                                                 TYPE_DF_FUNC);
        object_property_set_link(OBJECT(child), "parent",
                                 OBJECT(dev), &error_fatal);
        pci_realize_and_unref(child, s->bus, &error_fatal);
        pci_config_set_device_id(child->config, 0x1460 + fun);
    }
}

static Property df_props[] = {
    DEFINE_PROP_LINK("pci-bus", DfState, bus, TYPE_PCI_BUS, PCIBus *),
    DEFINE_PROP_END_OF_LIST(),
};

static void df_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    device_class_set_props(dc, df_props);

    dc->realize = df_realize;
    dc->desc = "Zen PCI DataFabric";
}

static const TypeInfo df_type_info = {
    .name = TYPE_DF,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DfState),
    .instance_init = df_init,
    .class_init = df_class_init,
};

/* Function */

static void df_func_write_config(PCIDevice *d, uint32_t address,
                                    uint32_t val, int len)
{
    DfFuncState *s = DF_FUNC(d);
    return df_write(s->parent, PCI_FUNC(d->devfn), address, val, len);
}

static uint32_t df_func_read_config(PCIDevice *d, uint32_t address, int len)
{
    DfFuncState *s = DF_FUNC(d);
    return df_read(s->parent, PCI_FUNC(d->devfn), address, len);
}

static void df_func_realize(PCIDevice *d, Error **errp)
{
}

static Property df_func_props[] = {
    DEFINE_PROP_LINK("parent", DfFuncState, parent, TYPE_DF, DfState *),
    DEFINE_PROP_END_OF_LIST(),
};

static void df_func_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    device_class_set_props(dc, df_func_props);
    k->vendor_id = PCI_VENDOR_ID_AMD;
    k->device_id = 0x1460;
    k->revision = 0;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    dc->desc = "Zen PCI DataFabric function";
    k->realize = df_func_realize;
    k->config_write = df_func_write_config;
    k->config_read = df_func_read_config;
}

static const TypeInfo df_func_type_info = {
    .name = TYPE_DF_FUNC,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(DfFuncState),
    .class_init = df_func_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void df_register_types(void)
{
    type_register_static(&df_type_info);
    type_register_static(&df_func_type_info);
}

type_init(df_register_types)
