#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/psp-regs.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/qdev-properties.h"
#include "trace.h"

/* Class structures */

struct _PspRegisterLoc;

struct PspRegsState {
    SysBusDevice parent_obj;

    MemoryRegion regs_region_public;
    MemoryRegion regs_region_private;
    struct _PspRegisterLoc *reg_locs;

    uint32_t postcode;
    uint32_t bootrom_revid;
    zen_codename codename;
};

struct PspRegsClass
{
    SysBusDeviceClass parent;
};

OBJECT_DECLARE_TYPE(PspRegsState, PspRegsClass, PSP_REGS)

/* Register structures */

typedef enum {
    REG_TYPE_PRIVATE,
    REG_TYPE_PUBLIC,
} PspRegisterType;

typedef struct {
    const char *name;
    uint32_t (*read)(PspRegsState *s);
    void (*write)(PspRegsState *s, uint32_t value);
} PspRegister;

typedef struct _PspRegisterLoc {
    PspRegisterType type;
    uint32_t offset;
    PspRegister *reg;
} PspRegisterLoc;

/* Register definitions */

static uint32_t reg_postcode_read(PspRegsState *s)
{
    return s->postcode;
}

static void reg_postcode_write(PspRegsState *s, uint32_t value)
{
    trace_psp_postcode(value);
    s->postcode = value;
}

PspRegister reg_postcode = {
    .name = "postcode",
    .read = reg_postcode_read,
    .write = reg_postcode_write,
};

static uint32_t reg_bootrom_revid_read(PspRegsState *s)
{
    return s->bootrom_revid;
}

PspRegister reg_bootrom_revid = {
    .name = "bootrom_revid",
    .read = reg_bootrom_revid_read,
};

static uint32_t reg_crypto_flags_read(PspRegsState *s)
{
    uint32_t res = 0;
    /*
    Bits 8 and 9 allow the use of RSA-4096 keys.
    They must have the same value.
    */
    res |= 1 << 8;
    res |= 1 << 9;
    return res;
}

PspRegister reg_crypto_flags = {
    .name = "crypto_flags",
    .read = reg_crypto_flags_read,
};

/* Location definitions */

#define LOC(type, offset, register) {REG_TYPE_ ## type, offset, &reg_ ## register}
#define LOC_END {.reg = NULL}

PspRegisterLoc locs_summit[] = {
    LOC(PRIVATE, 0x4c, bootrom_revid),
    LOC(PRIVATE, 0xe8, postcode),
    LOC_END
};
PspRegisterLoc locs_pinnacle[] = {
    LOC(PRIVATE, 0x4c, bootrom_revid),
    LOC(PRIVATE, 0xe8, postcode),
    LOC_END
};

PspRegisterLoc locs_raven[] = {
    LOC(PRIVATE, 0x4c, bootrom_revid),
    LOC(PRIVATE, 0xf0, postcode),
    LOC_END
};
PspRegisterLoc locs_picasso[] = {
    LOC(PRIVATE, 0x4c, bootrom_revid),
    LOC(PRIVATE, 0xf0, postcode),
    LOC_END
};

PspRegisterLoc locs_matisse[] = {
    LOC(PRIVATE, 0x48, bootrom_revid),
    LOC(PRIVATE, 0x50, crypto_flags),
    LOC(PRIVATE, 0xd8, postcode),
    LOC_END
};
PspRegisterLoc locs_vermeer[] = {
    LOC(PRIVATE, 0x48, bootrom_revid),
    LOC(PRIVATE, 0x50, crypto_flags),
    LOC(PRIVATE, 0xd8, postcode),
    LOC_END
};
PspRegisterLoc locs_lucienne[] = {
    LOC(PRIVATE, 0x48, bootrom_revid),
    LOC(PRIVATE, 0xd8, postcode),
    LOC_END
};
PspRegisterLoc locs_renoir[] = {
    LOC(PRIVATE, 0x48, bootrom_revid),
    LOC(PRIVATE, 0xd8, postcode),
    LOC_END
};
PspRegisterLoc locs_cezanne[] = {
    LOC(PRIVATE, 0x48, bootrom_revid),
    LOC(PRIVATE, 0xd8, postcode),
    LOC_END
};

static PspRegisterLoc *get_locs(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
        return locs_summit;
    case CODENAME_PINNACLE_RIDGE:
        return locs_pinnacle;
    case CODENAME_RAVEN_RIDGE:
        return locs_raven;
    case CODENAME_PICASSO:
        return locs_picasso;
    case CODENAME_MATISSE:
        return locs_matisse;
    case CODENAME_VERMEER:
        return locs_vermeer;
    case CODENAME_LUCIENNE:
        return locs_lucienne;
    case CODENAME_RENOIR:
        return locs_renoir;
    case CODENAME_CEZANNE:
        return locs_cezanne;
    default:
        g_assert_not_reached();
    }
}

/* Memory operations */

static PspRegister *get_register(PspRegsState *s, uint32_t type, uint32_t offset)
{
    PspRegisterLoc *loc;
    
    for(loc = s->reg_locs; loc->reg; loc++) {
        if(loc->type == type && loc->offset == offset) {
            return loc->reg;
        }
    }
    return NULL;
}

static uint64_t psp_regs_public_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    PspRegsState *s = PSP_REGS(opaque);
    PspRegister *reg = get_register(s, REG_TYPE_PUBLIC, offset);

    if(reg) {
        return reg->read(s);
    } else {
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                    "(offset 0x%lx)\n", __func__, offset);
        return 0;
    }
}

static void psp_regs_public_write(void *opaque, hwaddr offset,
                                  uint64_t data, unsigned size)
{
    PspRegsState *s = PSP_REGS(opaque);
    PspRegister *reg = get_register(s, REG_TYPE_PUBLIC, offset);

    if(reg) {
        reg->write(s, data);
    } else {
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                    "(offset 0x%lx, value 0x%lx)\n", __func__, offset, data);
    }
}

static uint64_t psp_regs_private_read(void *opaque, hwaddr offset,
                                     unsigned size)
{
    PspRegsState *s = PSP_REGS(opaque);
    PspRegister *reg = get_register(s, REG_TYPE_PRIVATE, offset);

    if(reg) {
        return reg->read(s);
    } else {
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                    "(offset 0x%lx)\n", __func__, offset);
        return 0;
    }
}

static void psp_regs_private_write(void *opaque, hwaddr offset,
                                  uint64_t data, unsigned size)
{
    PspRegsState *s = PSP_REGS(opaque);
    PspRegister *reg = get_register(s, REG_TYPE_PRIVATE, offset);

    if(reg) {
        reg->write(s, data);
    } else {
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                    "(offset 0x%lx, value 0x%lx)\n", __func__, offset, data);
    }
}

const MemoryRegionOps psp_regs_public_ops = {
    .read = psp_regs_public_read,
    .write = psp_regs_public_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

const MemoryRegionOps psp_regs_private_ops = {
    .read = psp_regs_private_read,
    .write = psp_regs_private_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

/* Init functions */

static void psp_regs_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspRegsState *s = PSP_REGS(obj);

    memory_region_init_io(&s->regs_region_public, OBJECT(s),
                          &psp_regs_public_ops, s,
                          "psp-regs-public", 0x10000);
    sysbus_init_mmio(sbd, &s->regs_region_public);

    memory_region_init_io(&s->regs_region_private, OBJECT(s),
                          &psp_regs_private_ops, s,
                          "psp-regs-private", 0x10000);
    sysbus_init_mmio(sbd, &s->regs_region_private);
}

static void psp_regs_realize(DeviceState *dev, Error **errp)
{
    PspRegsState *s = PSP_REGS(dev);

    s->reg_locs = get_locs(s->codename);
    s->bootrom_revid = zen_get_bootrom_revid(s->codename);
}

static Property psp_regs_props[] = {
    DEFINE_PROP_UINT32("codename", PspRegsState, codename, 0),
    DEFINE_PROP_END_OF_LIST()
};

static void psp_regs_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_regs_realize;
    dc->desc = "PSP registers";
    device_class_set_props(dc, psp_regs_props);
}

static const TypeInfo psp_regs_type_info = {
    .name = TYPE_PSP_REGS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspRegsState),
    .instance_init = psp_regs_init,
    .class_size = sizeof(PspRegsClass),
    .class_init = psp_regs_class_init,
};

static void psp_regs_register_types(void)
{
    type_register_static(&psp_regs_type_info);
}

type_init(psp_regs_register_types)
