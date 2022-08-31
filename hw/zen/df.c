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
    uint32_t fica2_addr;
    uint32_t mbat_ind_addr;
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

/* FICA (see linux arch/x86/kernel/amd_nb.c) */
REG32(FICA_ADDR,   FUN_REG(4, 0x5c))
    FIELD(FICA_ADDR,  NODE,  16, 8)
    FIELD(FICA_ADDR,  FUNC,  11, 3)
    FIELD(FICA_ADDR,  LARGE, 10, 1) /* 64 bits op */
    FIELD(FICA_ADDR,  REG,    2, 8) /* multiply by 4 */
    FIELD(FICA_ADDR,  RUN,    0, 2) /* must be one */
REG32(FICA_DATA0,  FUN_REG(4, 0x98))
REG32(FICA_DATA1,  FUN_REG(4, 0x9c))

/* FICA alias ? */
REG32(FICA2_ADDR,  FUN_REG(4, 0x50))
    FIELD(FICA2_ADDR, NODE,  16, 8)
    FIELD(FICA2_ADDR, LARGE, 14, 1) /* 64 bits op */
    FIELD(FICA2_ADDR, FUNC,  11, 3)
    FIELD(FICA2_ADDR, REG,    2, 9) /* multiply by 4 */
    FIELD(FICA2_ADDR, RUN,    0, 2) /* must be one */
REG32(FICA2_DATA0, FUN_REG(4, 0x80))
REG32(FICA2_DATA1, FUN_REG(4, 0x84))

/* MBAT entries ? for >=Zen2 */
REG32(MBAT_IND_ADDR, FUN_REG(6, 0x240))
REG32(MBAT_IND_DATA, FUN_REG(6, 0x244))

static uint32_t mbat_ind_read(uint32_t addr)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented read  "
                  "(addr 0x%x)\n",
                  __func__, addr);
    
    return 0;
}

static void mbat_ind_write(uint32_t addr, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented write "
                  "(addr 0x%x, value 0x%x)\n",
                  __func__, addr, data);
}

static uint32_t do_fica_read(uint8_t node, uint8_t func, uint16_t reg)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented read  "
                  "(addr [%02x:%01x:%03x])\n",
                  __func__, node, func, reg);
    
    return 0;
}

static void do_fica_write(uint8_t node, uint8_t func, uint16_t reg, uint32_t data)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented write "
                  "(addr [%02x:%01x:%03x], value 0x%x)\n",
                  __func__, node, func, reg, data);
}

static uint64_t df_read(DfState *s, int fun, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(FUN_REG(fun, offset)) {
    case FUN_REG(0, 0xa0):
        /*
        bits:
        24-31 is the "upper bound" (PCI bus ?)
        16-23 is the "lower bound" (PCI bus ?)
         4-11 is some value (Socket/RootBridge address ?)
         0- 1   must be 3

        */
        res = 0xff000043;
        break;
    case A_D18F1x200:
        /*
        not sure:
        bit 27: 0=single socket, 1=dual sockets
        */
        res = 0x00010001;
        break;
    case A_D18F1x204:
        /*
        not sure:
        bits 16-31: total number of root bridges
        */
        res = 0x00010001;
        break;
    case A_FICA_DATA0:
        return do_fica_read(
            FIELD_EX32(s->fica_addr, FICA_ADDR, NODE),
            FIELD_EX32(s->fica_addr, FICA_ADDR, FUNC),
            FIELD_EX32(s->fica_addr, FICA_ADDR, REG) * 4
        );
    case A_FICA_DATA1:
        return do_fica_read(
            FIELD_EX32(s->fica_addr, FICA_ADDR, NODE),
            FIELD_EX32(s->fica_addr, FICA_ADDR, FUNC),
            FIELD_EX32(s->fica_addr, FICA_ADDR, REG) * 4 + 4
        );
    case A_FICA2_DATA0:
        return do_fica_read(
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, NODE),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, FUNC),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, REG) * 4
        );
    case A_FICA2_DATA1:
        return do_fica_read(
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, NODE),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, FUNC),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, REG) * 4 + 4
        );
    case A_MBAT_IND_DATA:
        return mbat_ind_read(s->mbat_ind_addr);
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
    case A_FICA_ADDR:
        s->fica_addr = data;
        assert(FIELD_EX32(s->fica_addr, FICA_ADDR, RUN) == 1);
        assert(FIELD_EX32(s->fica_addr, FICA_ADDR, LARGE) == 0);
        return;
    case A_FICA2_ADDR:
        s->fica2_addr = data;
        assert(FIELD_EX32(s->fica2_addr, FICA2_ADDR, RUN) == 1);
        //assert(FIELD_EX32(s->fica2_addr, FICA2_ADDR, LARGE) == 0);
        return;
    case A_FICA_DATA0:
        do_fica_write(
            FIELD_EX32(s->fica_addr, FICA_ADDR, NODE),
            FIELD_EX32(s->fica_addr, FICA_ADDR, FUNC),
            FIELD_EX32(s->fica_addr, FICA_ADDR, REG) * 4,
            data
        );
        return;
    case A_FICA_DATA1:
        do_fica_write(
            FIELD_EX32(s->fica_addr, FICA_ADDR, NODE),
            FIELD_EX32(s->fica_addr, FICA_ADDR, FUNC),
            FIELD_EX32(s->fica_addr, FICA_ADDR, REG) * 4 + 4,
            data
        );
        return;
    case A_FICA2_DATA0:
        do_fica_write(
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, NODE),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, FUNC),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, REG) * 4,
            data
        );
        return;
    case A_FICA2_DATA1:
        do_fica_write(
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, NODE),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, FUNC),
            FIELD_EX32(s->fica2_addr, FICA2_ADDR, REG) * 4 + 4,
            data
        );
        return;
    case A_MBAT_IND_ADDR:
        s->mbat_ind_addr = data;
        return;
    case A_MBAT_IND_DATA:
        mbat_ind_write(s->mbat_ind_addr, data);
        return;
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
