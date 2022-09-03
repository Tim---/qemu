#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "sysemu/sysemu.h"
#include "hw/zen/zen-mobo.h"
#include "hw/zen/zen-utils.h"
#include "qapi/error.h"
#include "hw/zen/fch.h"
#include "hw/zen/zen-pci-root.h"
#include "hw/isa/isa.h"
#include "hw/pci/pci_host.h"
#include "hw/pci/pcie_host.h"
#include "exec/address-spaces.h"
#include "hw/zen/fch-lpc-bridge.h"
#include "hw/pci/pci.h"
#include "hw/zen/fch-spi.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "sysemu/block-backend.h"
#include "hw/zen/smu.h"
#include "hw/zen/ddr4-spd.h"
#include "hw/zen/smn-misc.h"
#include "hw/zen/zen-umc.h"
#include "hw/zen/df.h"
#include "hw/zen/psp-fuses.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/dxio.h"
#include "hw/timer/hpet.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenMoboState, ZEN_MOBO)

struct ZenMoboState {
    SysBusDevice parent_obj;

    zen_codename codename;

    MemoryRegion smn;
    MemoryRegion *ht;
    MemoryRegion ht_io;
    MemoryRegion pcie_mmconfig_alias;

    ISABus *isa_bus;
    PCIBus *pci_bus;

    DeviceState *fch_spi;
};

static void create_smn(ZenMoboState *s)
{
    create_region_with_unimpl(&s->smn, OBJECT(s), "smn", 0x1000000000UL);
}

static void create_ht(ZenMoboState *s)
{
    s->ht = get_system_memory();
    create_unimplemented_device_generic(s->ht, "ht-unimpl", 0, 0x1000000000000ULL);

    memory_region_init_alias(&s->ht_io, OBJECT(s), "ht-io", get_system_io(), 0, 0x10000);
    create_unimplemented_device_generic(get_system_io(), "ht-io-unimpl", 0, 0x10000);

    memory_region_add_subregion(s->ht, 0xfffdfc000000, &s->ht_io);
}

MemoryRegion *zen_mobo_get_ht(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return s->ht;
}

MemoryRegion *zen_mobo_get_smn(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return &s->smn;
}

ISABus *zen_mobo_get_isa(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return s->isa_bus;
}

PCIBus *zen_mobo_get_pci(DeviceState *dev)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    return s->pci_bus;
}

static void generic_map(MemoryRegion *container, SysBusDevice *sbd, int n, hwaddr addr, bool alias, bool overlap)
{
    MemoryRegion *region = sysbus_mmio_get_region(sbd, n);

    if(alias) {
        MemoryRegion *alias = g_new(MemoryRegion, 1);
        g_autofree char *alias_name = g_strdup_printf("alias-%s",
                                                    memory_region_name(region));

        memory_region_init_alias(alias, memory_region_owner(region), alias_name,
                                region, 0, memory_region_size(region));
        
        region = alias;
    }

    if(overlap) {
        memory_region_add_subregion_overlap(container, addr, region, -1000);
    } else {
        memory_region_add_subregion(container, addr, region);
    }
}

void zen_mobo_smn_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias)
{
    generic_map(zen_mobo_get_smn(dev), sbd, n, addr, alias, false);
}

void zen_mobo_smn_map_overlap(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias)
{
    generic_map(zen_mobo_get_smn(dev), sbd, n, addr, alias, true);
}

void zen_mobo_ht_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias)
{
    generic_map(zen_mobo_get_ht(dev), sbd, n, addr, alias, false);
}

static void zen_mobo_create_serial(ZenMoboState *s)
{
    serial_mm_init(get_system_io(), 0x3f8, 0, NULL, 9600, serial_hd(0), DEVICE_NATIVE_ENDIAN);
}

static void pcie_smn_access_write(void *opaque, hwaddr mmcfg_addr,
                                  uint64_t val, unsigned len)
{
    PCIDevice *pci_dev = PCI_DEVICE(opaque);
    uint32_t addr;
    uint32_t limit;

    addr = PCIE_MMCFG_CONFOFFSET(mmcfg_addr);
    limit = pci_config_size(pci_dev);
    pci_host_config_write_common(pci_dev, addr, limit, val, len);
}

static uint64_t pcie_smn_access_read(void *opaque,
                                     hwaddr mmcfg_addr,
                                     unsigned len)
{
    PCIDevice *pci_dev = PCI_DEVICE(opaque);
    uint32_t addr;
    uint32_t limit;

    addr = PCIE_MMCFG_CONFOFFSET(mmcfg_addr);
    limit = pci_config_size(pci_dev);
    return pci_host_config_read_common(pci_dev, addr, limit, len);
}

static const MemoryRegionOps pcie_smn_access_ops = {
    .read = pcie_smn_access_read,
    .write = pcie_smn_access_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void create_pcie_smn_access(ZenMoboState *s, PCIDevice *dev, hwaddr offset, uint64_t size)
{
    MemoryRegion *region = g_malloc0(sizeof(*region));
    memory_region_init_io(region, OBJECT(s), &pcie_smn_access_ops, dev, "pcie-alias", size);
    memory_region_add_subregion(&s->smn, offset, region);
}

static DeviceState *create_fch(DeviceState *s)
{
    DeviceState *dev = qdev_new(TYPE_FCH);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    zen_mobo_ht_map(s, SYS_BUS_DEVICE(dev), 0, 0xfed80000, false);
    zen_mobo_smn_map(s, SYS_BUS_DEVICE(dev), 0, 0x02d01000, true);
    return dev;
}

static void create_isa(ZenMoboState *s)
{
    s->isa_bus = isa_bus_new(DEVICE(s), s->ht, get_system_io(), &error_fatal);
}

static void create_pcie_host(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_ZEN_HOST);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    pcie_host_mmcfg_map(PCIE_HOST_BRIDGE(dev), 0xfffe00000000, PCIE_MMCFG_SIZE_MAX);

    memory_region_init_alias(&s->pcie_mmconfig_alias, OBJECT(s), "ht-mmconfig-alias", s->ht, 0xfffe00000000, 0x4000000);
    memory_region_add_subregion(s->ht, 0xf0000000, &s->pcie_mmconfig_alias);

    PCIHostState *phb = PCI_HOST_BRIDGE(dev);
    s->pci_bus = phb->bus;
}

static void create_pcie_root(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_ZEN_ROOT_DEVICE);
    qdev_prop_set_int32(DEVICE(dev), "addr", PCI_DEVFN(0, 0));
    qdev_prop_set_bit(DEVICE(dev), "multifunction", false);
    object_property_set_link(OBJECT(dev), "smn", OBJECT(&s->smn), &error_fatal);
    qdev_realize(dev, BUS(s->pci_bus), &error_fatal);
}

static void create_fch_lpc_bridge(ZenMoboState *s)
{
    pci_create_simple_multifunction(s->pci_bus, PCI_DEVFN(0x14, 0x3),
                                    true, TYPE_FCH_LPC_BRIDGE);
}

static DeviceState* create_fch_spi(ZenMoboState *s, zen_codename codename)
{
    uint32_t smn_addr;

    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        smn_addr = 0x0a000000;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        smn_addr = 0x44000000;
        break;
    default:
        g_assert_not_reached();
    }
    DeviceState *dev = qdev_new(TYPE_FCH_SPI);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0x02dc4000, false);
    zen_mobo_ht_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0xfec10000, true);
    zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 1, smn_addr, false);
    zen_mobo_ht_map(DEVICE(s), SYS_BUS_DEVICE(dev), 1, 0xff000000, true);
    return dev;
}

static void create_spi_rom(DeviceState *fch_spi, BlockBackend *blk)
{
    const char *flash_type;

    switch(blk_getlength(blk)) {
    case 0x1000000:
        flash_type = "mx25l12805d";
        break;
    /*
    We don't know how to handle it properly for now
    case 0x2000000:
        flash_type = "mx25l25655e";
        break;
    */
    default:
        g_assert_not_reached();
    }

    fch_spi_add_flash(fch_spi, blk, flash_type);
}

static void create_smu(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_SMU);
    qdev_prop_set_uint32(dev, "codename", s->codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0x03b10000, false);
}

static void create_single_ddr4(ZenMoboState *s, BusState *smbus, uint8_t address)
{
    DeviceState *dev = qdev_new(TYPE_DDR4_SPD);
    qdev_prop_set_uint8(dev, "address", address);
    object_property_set_link(OBJECT(dev), "bus",
                            OBJECT(smbus), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
}

static void create_ddr4(ZenMoboState *s, BusState *smbus)
{
    /*
    SMBus addresses of the SPDs depending on the topology.
    The "channel inversion" for Matisse is perhaps due to the APCB
    data in our firmware.

    summit-ridge:
        socket 0, channel 0, dimm 0: 0x50
        socket 0, channel 1, dimm 0: 0x51
    raven-ridge:
        socket 0, channel 0, dimm 0: 0x50
        socket 0, channel 1, dimm 0: 0x51
    matisse:
        socket 0, channel 1, dimm 0: 0x50
        socket 0, channel 0, dimm 0: 0x51
        socket 0, channel 1, dimm 1: 0x52
        socket 0, channel 0, dimm 1: 0x53
    renoir:
        socket 0, channel 0, dimm 0: 0x50
        socket 0, channel 1, dimm 0: 0x51
        socket 0, channel 0, dimm 1: 0x52
        socket 0, channel 1, dimm 1: 0x53
    cezanne:
        socket 0, channel 0, dimm 0: 0x50
        socket 0, channel 1, dimm 0: 0x51
        socket 0, channel 0, dimm 1: 0x52
        socket 0, channel 1, dimm 1: 0x53
    */

    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        create_single_ddr4(s, smbus, 0x50);
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_single_ddr4(s, smbus, 0x51);
        create_single_ddr4(s, smbus, 0x53);
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        create_single_ddr4(s, smbus, 0x50);
        create_single_ddr4(s, smbus, 0x52);
        break;
    default:
        g_assert_not_reached();
    }
}

static void create_smn_misc(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_SMN_MISC);
    qdev_prop_set_uint32(dev, "codename", s->codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map_overlap(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0, false);
}

static void create_umc(ZenMoboState *s)
{
    hwaddr addresses[] = {0x50000, 0x150000};

    for(int i = 0; i < ARRAY_SIZE(addresses); i++) {
        DeviceState *dev = qdev_new(TYPE_ZEN_UMC);
        qdev_prop_set_uint8(dev, "instance", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, addresses[i], false);
    }
}

static void create_df(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_DF);
    object_property_set_link(OBJECT(dev), "pci-bus",
                                OBJECT(s->pci_bus), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    for(int fun = 0; fun < 8; fun++) {
        PCIDevice *dev = pci_find_device(s->pci_bus, 0, PCI_DEVFN(0x18, fun));
        create_pcie_smn_access(s, dev, 0x1c000 + fun * 0x400, 0x400);
    }
}

static void create_fuses(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_PSP_FUSES);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0x5d000, false);

    psp_dirty_fuses(s->codename, dev);
}

static void create_dxio(ZenMoboState *s)
{
    if(s->codename != CODENAME_SUMMIT_RIDGE)
        return;

    DeviceState *dev = qdev_new(TYPE_DXIO);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0x12000000, false);
    zen_mobo_smn_map(DEVICE(s), SYS_BUS_DEVICE(dev), 1, 0x11a00000, false);
}

static void create_hpet(ZenMoboState *s)
{
    DeviceState *dev = qdev_new(TYPE_HPET);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_ht_map(DEVICE(s), SYS_BUS_DEVICE(dev), 0, 0xfed00000, false);
}

DeviceState *zen_mobo_create(zen_codename codename, BlockBackend *blk)
{
    DeviceState *dev = qdev_new(TYPE_ZEN_MOBO);
    qdev_prop_set_uint32(dev, "codename", codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    create_spi_rom(ZEN_MOBO(dev)->fch_spi, blk);
    return dev;
}

static void zen_mobo_init(Object *obj)
{
}

static void zen_mobo_realize(DeviceState *dev, Error **errp)
{
    ZenMoboState *s = ZEN_MOBO(dev);
    create_smn(s);
    create_ht(s);

    zen_mobo_create_serial(s);
    DeviceState *fch = create_fch(DEVICE(s));
    create_pcie_host(s);
    create_pcie_root(s);
    create_isa(s);
    create_fch_lpc_bridge(s);
    s->fch_spi = create_fch_spi(s, s->codename);
    create_smu(s);
    BusState *smbus = qdev_get_child_bus(fch, "smbus");
    create_ddr4(s, smbus);
    create_smn_misc(s);
    create_umc(s);
    create_df(s);
    create_fuses(s);
    create_dxio(s);
    create_hpet(s);
}

static Property zen_mobo_props[] = {
    DEFINE_PROP_UINT32("codename", ZenMoboState, codename, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void zen_mobo_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = zen_mobo_realize;
    dc->desc = "Zen motherboard";
    device_class_set_props(dc, zen_mobo_props);
}

static const TypeInfo zen_mobo_type_info = {
    .name = TYPE_ZEN_MOBO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ZenMoboState),
    .instance_init = zen_mobo_init,
    .class_init = zen_mobo_class_init,
};

static void zen_mobo_register_types(void)
{
    type_register_static(&zen_mobo_type_info);
}

type_init(zen_mobo_register_types)
