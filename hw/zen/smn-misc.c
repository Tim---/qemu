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
            // firmware: 0x1000->0x3000
            // registers: 0x04008, 0x0401c, 0x04030, 0x04038, 0x04044, 0x04048, 0x04058, 0x0405c, 0x10000, 0x10004, 0x10030
            add_region_printf(s, "misc_a%d_b%d", base + j * 0x20000, 0x20000, i, j);
        }
        for(int j = 0; j < num; j++) {
            // registers: 0x4fc, 0x838, 0x860, 0x864
            add_region_printf(s, "misc_a%d_c%d", base + 0xe0000 + j * 0x1000, 0x1000, i, j);
        }

        // registers: 0x8400, 0x8408, 0x8410, 0x8418, 0x8478
        add_region_printf(s, "misc_a%d_d", base + 0xf0000, 0x10000, i);
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
