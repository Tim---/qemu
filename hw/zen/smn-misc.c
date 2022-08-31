#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/zen/smn-misc.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/zen-utils.h"

OBJECT_DECLARE_SIMPLE_TYPE(SmnMiscState, SMN_MISC)

struct SmnMiscState {
    SysBusDevice parent_obj;

    MemoryRegion region;
    zen_codename codename;
};


static uint64_t smn_misc_read(void *opaque, hwaddr offset, unsigned size)
{
    SmnMiscState *s = SMN_MISC(opaque);
    uint64_t res = 0;

    switch(offset) {
    case 0x5a81c:
        /*
        summit-ridge/raven-ridge:
            bits 3-0: CoreCount minus one
            bits 7-4: ComplexCount minus one
            bit    8: ThreadingDisabled
        */
        return 0x100;
    case 0x5a86c:
        return zen_get_cpuid(s->codename);
    case 0x5a870:
        /*
        summit-ridge/raven-ridge:
            bitmap of enabled cores
            bit ccx*4 + core
        */
        return 1;
    case 0x5a08c:
        /*
        matisse (and later ?) expect it to have the number of "dimms" (?)
        in bits 2-4

        2   -> 2 "dimms"
        4   -> 8 "dimms"
        7   -> 4 "dimms"
        */
        return 2 << 2;
    case 0x3e10024:
        /*
        MP2_RSMU_FUSESTRAPS
        Bit 0 is set to 1 to signal that MP2 is disabled
        This is needed for the raven-ridge PSP bootloader.
        */
        return 1;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return res;
}

static void smn_misc_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, value);
}

static const MemoryRegionOps smn_misc_ops = {
    .read = smn_misc_read,
    .write = smn_misc_write,
};


static void add_region(SmnMiscState *s, const char *name, hwaddr addr, uint64_t size)
{
    MemoryRegion *region = g_malloc0(sizeof(*region));
    create_region_with_unimpl(region, OBJECT(s), name, size);
    memory_region_add_subregion(&s->region, addr, region);
}

static void add_region_printf(SmnMiscState *s, const char *fmt, hwaddr addr, uint64_t size, ...)
{
    va_list args;
    va_start(args, size);

    char *name = NULL;
    int res = vasprintf(&name, fmt, args);
    assert(res != -1);

    add_region(s, name, addr, size);

    va_end(args);
}

static void create_dxio_summit(SmnMiscState *s)
{
    // These regions are the GMI PCS regions (Physical Coding Sublayer)
    for(int ld = 0; ld < 2; ld++) {
        hwaddr base = 0x12000000 + ld * 0x100000;

        add_region_printf(s, "misc_xgmi_a%d.pcs", base + 0x0b000, 0x1000, ld);

        for(int kpnp = 0; kpnp < 5; kpnp++) {
            // registers: 0x38, 0x60, 0x64
            add_region_printf(s, "misc_xgmi_a%d.pnp%d", base + kpnp * 0x20000 + 0x9800, 0x1000, ld, kpnp);
        }
    }

    // Are these PCS regions too ? They have the same watchdog
    for(int ld = 0; ld < 4; ld++) {
        hwaddr base = 0x11a00000 + ld * 0x100000;
        add_region_printf(s, "misc_xgmi_b%d.pcs", base + 0x51000, 0x1000, ld);
    }
}

static void create_dxio_matisse(SmnMiscState *s)
{
    struct {
        uint32_t base;
        int num_entries;
    } regions[] = {
        {.base = 0x12500000, .num_entries = 5},
        {.base = 0x12900000, .num_entries = 4},
    };

    for(int i = 0; i < ARRAY_SIZE(regions); i++) {
        uint32_t base = regions[i].base;
        int num = regions[i].num_entries;

        for(int j = 0; j < num; j++) {
            // "Phy" region
            // firmware: 0x1000->0x3000
            // registers: 0x04008, 0x0401c, 0x04030, 0x04038, 0x04044, 0x04048, 0x04058, 0x0405c, 0x10000, 0x10004, 0x10030
            add_region_printf(s, "misc_a%d_b%d", base + j * 0x20000, 0x20000, i, j);
        }
        for(int j = 0; j < num; j++) {
            // Offset 0x800 is the "pnp" region
            // registers: 0x4fc, 0x838, 0x860, 0x864
            add_region_printf(s, "misc_a%d_c%d", base + 0xe0000 + j * 0x1000, 0x1000, i, j);
        }

        // registers: 0x8400, 0x8408, 0x8410, 0x8418, 0x8478
        add_region_printf(s, "misc_a%d_d", base + 0xf0000, 0x10000, i);
    }

}

static void create_unknown_summit(SmnMiscState *s)
{
    uint8_t nodes[] = {
        0x01, 0xff, // these two are special ?
        0x03, 0x04, 0x05, 0x06, 0x07, 0x0a, 0x0d, 0x13,
        0x14, 0x18, 0x1c, 0x24, 0x25, 0x27, 0x2d, 0x2e,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x42, 0x46,
        0x47, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x5a, 0x5b, 0x64, 0x6c, 0x6d, 0x6e, 0x7c,
        0x80, 0x81, 0x90, 0x96, 0x97, 0xa8, 0xaa, 0xb0,
        0xb1, 0xd8, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5,
        0xe6
    };
    for(int i = 0; i < ARRAY_SIZE(nodes); i++) {
        uint8_t node = nodes[i];
        add_region_printf(s, "misc_node_a%02x", 0x1000000 + node * 0x1000, 0x1000, node);
        add_region_printf(s, "misc_node_b%02x", 0x2f00000 + node * 0x400, 0x400, node);
    }
}

static void create_unknown_raven(SmnMiscState *s)
{
    uint8_t nodes[] = {
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0a,
        0x0c, 0x0e, 0x0f, 0x10, 0x12, 0x18, 0x1c, 0x22,
        0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
        0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,
        0x46, 0x50, 0x51, 0x52, 0x53, 0x5a, 0x5b, 0x64,
        0x6c, 0x7c, 0x80, 0x96, 0x97, 0xa8, 0xaa, 0xab,
        0xb0, 0xd8
    };
    for(int i = 0; i < ARRAY_SIZE(nodes); i++) {
        uint8_t node = nodes[i];
        add_region_printf(s, "misc_node_a%02x", 0x1000000 + node * 0x1000, 0x1000, node);
    }
}

static void create_unknown_matisse(SmnMiscState *s)
{
    uint8_t nodes[] = {
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0d,
        0x17, 0x1b, 0x1f, 0x25, 0x2e, 0x30, 0x31, 0x35,
        0x39, 0x3d, 0x41, 0x49, 0x4d, 0x50, 0x51, 0x5a,
        0x5e, 0x62, 0x67, 0x6f, 0x73, 0x77, 0x7f, 0x83,
        0x87, 0x93, 0x96, 0x97, 0xa8, 0xa9, 0xaa, 0xab,
        0xc4, 0xd3, 0xe3, 0xe7, 0xe8, 0xe9, 0xf4, 0xf5,
    };
    for(int i = 0; i < ARRAY_SIZE(nodes); i++) {
        uint8_t node = nodes[i];
        add_region_printf(s, "misc_node_a%02x", 0x9000000 + node * 0x1000, 0x1000, node);
    }
}

static void create_unknown_lucienne(SmnMiscState *s)
{
    uint8_t nodes[] = {
        0x03, 0x04, 0x05, 0x06, 0x0c, 0x0e, 0x0f, 0x10,
        0x12, 0x18, 0x1c, 0x22, 0x23, 0x24, 0x25, 0x28,
        0x29, 0x2a, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
        0x32, 0x33, 0x46, 0x50, 0x52, 0x54, 0x5a, 0x63,
        0x64, 0x6c, 0x7c, 0x80, 0x96, 0xa8, 0xaa, 0xb0,
        0xb1, 0xb5, 0xd8,
    };
    for(int i = 0; i < ARRAY_SIZE(nodes); i++) {
        uint8_t node = nodes[i];
        add_region_printf(s, "misc_node_a%02x", 0x9000000 + node * 0x1000, 0x1000, node);
    }
}

static void create_dxio(SmnMiscState *s)
{
    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        create_dxio_summit(s);
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_dxio_matisse(s);
        break;
    default:
        break;
    }
}

static void create_pcie_misc(SmnMiscState *s)
{
    if(s->codename != CODENAME_SUMMIT_RIDGE)
        return;
    
    /* Secondary PCI buses behing GPP bridges */
    for(int i = 0; i < 3; i++) {
        hwaddr base = 0x10100000 + i * 0x100000;
        add_region_printf(s, "misc_gpp%d", base, 0x100000, i);

        for(int j = 0; j < 8; j++) {
            add_region_printf(s, "misc_gpp%d_a%d", base + 0x40000 + j * 0x1000, 0x1000, i, j); /* PCIe conf */
            add_region_printf(s, "misc_gpp%d_b%d", base + 0x34000 + j * 0x200, 0x200, i, j);
        }
        add_region_printf(s, "misc_gpp%d_c", base + 0x23000, 0x1000, i);
        add_region_printf(s, "misc_gpp%d_d", base + 0x3a000, 0x2000, i);
        add_region_printf(s, "misc_gpp%d_e", base + 0x30000, 0x1000, i);
        add_region_printf(s, "misc_gpp%d_f", base + 0x00000, 0x1000, i); /* PCIe conf */
    }

    for(int wrapper = 0; wrapper < 2; wrapper++) {
        hwaddr base = 0x11100000 + wrapper * 0x100000;

        add_region_printf(s, "misc_wrapper%d", base, 0x100000, wrapper);
        for(int port = 0; port < 8; port++) {
            add_region_printf(s, "misc_wrapper%d_a_port%x", base + port * 0x1000, 0x1000, wrapper, port);
            add_region_printf(s, "misc_wrapper%d_b_port%x", base + 0x40000 + port * 0x1000, 0x1000, wrapper, port);
        }
    }

    add_region_printf(s, "misc_indirect%d", 0x4a348, 0x8, 0);
    add_region_printf(s, "misc_indirect%d", 0x4a3c8, 0x8, 1);

    /* PCIe config alias for 00:00.0 */
    add_region_printf(s, "misc_rb", 0x13b00000, 0x1000);
    for(int i = 0; i < 16; i++) {
        add_region_printf(s, "misc_rb_a%d", 0x13b31000 + i * 0x400, 0x400, i);
    }

    for(int i = 0; i < 7; i++)
        add_region_printf(s, "misc_bits_a%d", 0x13b14400 + i * 0x400, 0x400, i);
    for(int i = 0; i < 4; i++)
        add_region_printf(s, "misc_bits_b%d", 0x15b00400 + i * 0x400, 0x400, i);
    for(int i = 0; i < 5; i++)
        add_region_printf(s, "misc_bits_c%d", 0x04400400 + i * 0x400, 0x400, i);

    
    /* Some device. Taishan-specific ? */
    add_region_printf(s, "misc_x", 0x03100000, 0x10000);
    for(int i = 0; i < 8; i++)
        add_region_printf(s, "misc_x_a%d", 0x03101100 + i * 0x80, 0x80, i);

    /* Some device. Taishan-specific ? */
    add_region_printf(s, "misc_y_a", 0x16d80000, 0x1000);
    add_region_printf(s, "misc_y_b", 0x16c0c000, 0x1000);
    for(int i = 0; i < 4; i++) {
        add_region_printf(s, "misc_y_c%d", 0x16d04000 + i * 0x400, 0x400, i);
        add_region_printf(s, "misc_y_d%d", 0x16d08000 + i * 0x400, 0x400, i);
        add_region_printf(s, "misc_y_e%d", 0x16d0c000 + i * 0x400, 0x400, i);
    }

    /* IOMMU */
    add_region_printf(s, "iommu_cfg",  0x13f00000, 0x1000); /* PCI */
    add_region_printf(s, "iommu_mmio", 0x02400000, 0x10000);
    add_region_printf(s, "iommu_l2a",  0x15700000, 0x1000);
    add_region_printf(s, "iommu_l2b",  0x13f01000, 0x1000);
    for(int i = 0; i < 4; i++) {
        add_region_printf(s, "iommu_l1int%d", 0x14700000 + i * 0x100000, 0x100000, i);
    }


}

static void create_unknown_blocks(SmnMiscState *s)
{
    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        create_unknown_summit(s);
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        create_unknown_raven(s);
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_unknown_matisse(s);
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        create_unknown_lucienne(s);
        break;
    default:
        break;
    }
}

static void smn_misc_init(Object *obj)
{
}

static void smn_misc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    SmnMiscState *s = SMN_MISC(dev);

    /* Init the registers access */
    memory_region_init_io(&s->region, OBJECT(s), &smn_misc_ops, s,
                          "smn-misc", 0x100000000);
    sysbus_init_mmio(sbd, &s->region);

    create_dxio(s);
    create_pcie_misc(s);
    create_unknown_blocks(s);
}

static Property smn_misc_props[] = {
    DEFINE_PROP_UINT32("codename", SmnMiscState, codename, 0),
    DEFINE_PROP_END_OF_LIST()
};

static void smn_misc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = smn_misc_realize;
    dc->desc = "PSP SMN misc";
    device_class_set_props(dc, smn_misc_props);
}

static const TypeInfo smn_misc_type_info = {
    .name = TYPE_SMN_MISC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SmnMiscState),
    .instance_init = smn_misc_init,
    .class_init = smn_misc_class_init,
};

static void smn_misc_register_types(void)
{
    type_register_static(&smn_misc_type_info);
}

type_init(smn_misc_register_types)
