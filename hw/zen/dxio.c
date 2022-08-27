#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/dxio.h"
#include "hw/zen/zen-utils.h"
#include "hw/registerfields.h"

OBJECT_DECLARE_SIMPLE_TYPE(DxioState, DXIO)

struct DxioState {
    SysBusDevice parent_obj;
    MemoryRegion region_a;
    MemoryRegion region_b;

    uint32_t target_state;
    int chaser;
};

/*
PCS: Physical Coding Sublayer

Guess:
0x00 is "pcsAperture"
0x40 is "targetState"
0x78 is "PCS_STATE_HIST1"
0x7C is "PCS_STATE_HIST2"
0x80 is related to watchdog
0x90 is related to watchdog
0x280 is ?
*/

REG32(TARGET_STATE, 0x40)
REG32(HIST1, 0x78)
REG32(HIST2, 0x7c)

uint32_t hist1_values[] = {0, 0x11, 0x81};

static uint64_t dxio_pcs_read(void *opaque, hwaddr addr, unsigned size)
{
    MemoryRegion *region = MEMORY_REGION(opaque);
    DxioState *s = DXIO(memory_region_owner(region));
    uint64_t res = 0;

    switch(addr) {
    case A_TARGET_STATE:
        res = 0;
        break;
    case A_HIST1:
        /*
        The value depends on D18F1x330, which is used to enable/disable the PCS.
        For now, we use a dirty hack, return both values alternatively.
        */
        res = hist1_values[s->chaser];
        s->chaser = (s->chaser + 1) % ARRAY_SIZE(hist1_values);
        break;
    case A_HIST2:
        res = 0;
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(%s, offset 0x%lx)\n", __func__, memory_region_name(region), addr);

    return res;
}

static void dxio_pcs_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    MemoryRegion *region = MEMORY_REGION(opaque);
    DxioState *s = DXIO(memory_region_owner(region));

    switch(addr) {
    case A_TARGET_STATE:
        s->target_state = value;
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(%s, offset 0x%lx, value 0x%lx)\n",
                  __func__, memory_region_name(region), addr, value);
}

static const MemoryRegionOps dxio_pcs_ops = {
    .read = dxio_pcs_read,
    .write = dxio_pcs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/*
PNP

Guess:
0x38 is ???. Polled until 0 after reset ?
0x60 is "hold". Set to 1 when writing other registers. Set to 0 after.
0x64 is "reset". Set to 0xff when applying "KPNP reset". Set to 0x00 when "clearing KPNP resets"
*/

static uint64_t dxio_pnp_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);

    return 0;
}

static void dxio_pnp_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, addr, value);
}

static const MemoryRegionOps dxio_pnp_ops = {
    .read = dxio_pnp_read,
    .write = dxio_pnp_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void dxio_init(Object *obj)
{
}

static void dxio_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    DxioState *s = DXIO(dev);

    create_region_with_unimpl(&s->region_a, OBJECT(s), "dxio_a", 0x200000);
    sysbus_init_mmio(sbd, &s->region_a);

    for(int ld = 0; ld < 2; ld++) {
        hwaddr base = ld * 0x100000;

        for(int kpnp = 0; kpnp < 5; kpnp++) {
            // registers: 0x38, 0x60, 0x64
            MemoryRegion *pnp_region = g_malloc0(sizeof(*pnp_region));
            g_autofree char *pnp_name = g_strdup_printf("dxio_a%d.pnp%d", ld, kpnp);
            memory_region_init_io(pnp_region, OBJECT(s), &dxio_pnp_ops, NULL, pnp_name, 0x1000);
            memory_region_add_subregion(&s->region_a, base + kpnp * 0x20000 + 0x9800, pnp_region);
        }

        // These regions are the GMI PCS regions (Physical Coding Sublayer)
        MemoryRegion *pcs_region = g_malloc0(sizeof(*pcs_region));
        g_autofree char *pcs_name = g_strdup_printf("dxio_a%d.pcs", ld);
        memory_region_init_io(pcs_region, OBJECT(s), &dxio_pcs_ops, pcs_region, pcs_name, 0x1000);
        memory_region_add_subregion(&s->region_a, base + 0x0b000, pcs_region);
    }

    // Are these PCS regions too ? They have the same watchdog
    create_region_with_unimpl(&s->region_b, OBJECT(s), "dxio_b", 0x400000);
    sysbus_init_mmio(sbd, &s->region_b);
    for(int ld = 0; ld < 4; ld++) {
        hwaddr base = ld * 0x100000;

        MemoryRegion *pcs_region = g_malloc0(sizeof(*pcs_region));
        g_autofree char *pcs_name = g_strdup_printf("dxio_b%d.pcs", ld);
        memory_region_init_io(pcs_region, OBJECT(s), &dxio_pcs_ops, pcs_region, pcs_name, 0x1000);
        memory_region_add_subregion(&s->region_b, base + 0x51000, pcs_region);
    }
}

static void dxio_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = dxio_realize;
    dc->desc = "Zen DXIO";
}

static const TypeInfo dxio_type_info = {
    .name = TYPE_DXIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DxioState),
    .instance_init = dxio_init,
    .class_init = dxio_class_init,
};

static void dxio_register_types(void)
{
    type_register_static(&dxio_type_info);
}

type_init(dxio_register_types)
