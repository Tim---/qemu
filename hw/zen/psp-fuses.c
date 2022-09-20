#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/psp-fuses.h"

OBJECT_DECLARE_SIMPLE_TYPE(PspFusesState, PSP_FUSES)

struct PspFusesState {
    SysBusDevice parent_obj;

    MemoryRegion data_region;
    MemoryRegion ctrl_region;
    uint32_t fuses[0x1000 / 4];
};

void psp_fuses_write32(DeviceState *dev, uint32_t addr, uint32_t value)
{
    PspFusesState *s = PSP_FUSES(dev);
    s->fuses[addr / 4] |= value;
}

void psp_fuses_write_bits(DeviceState *dev, uint32_t start, uint32_t size, uint32_t value)
{
    PspFusesState *s = PSP_FUSES(dev);

    assert(size <= 32);
    assert(start / 32 <= 0x400);

    uint32_t *p = &s->fuses[start / 32];

    uint32_t first_size = MIN(size, 32 - start % 32);

    printf("start = %d, size = %d\n", start % 32, first_size);
    *p = deposit32(*p, start % 32, first_size, value);

    size -= first_size;
    value >>= first_size;

    if(size) {
        printf("start = %d, size = %d\n", 0, size);
        *(p+1) = deposit32(*(p+1), 0, size, value);
    }
}

/**
 * Fuses can only be set from 0 to 1 !
 * 
 * The "ctrl" registers are used to "commit" a fuse write.
 * 
 * Perhaps the layout is the following:
 * 5d000-5dfff: fuses data 0
 * 5e000-5e0ff: fuses ctrl
 * 5e100-5e3ff: fuses data 1
 */

static uint64_t psp_fuses_data_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);

    PspFusesState *s = PSP_FUSES(opaque);
    return s->fuses[addr / 4];
}

static void psp_fuses_data_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, addr, value);
}

static uint64_t psp_fuses_ctrl_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);
    return 0;
}

static void psp_fuses_ctrl_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, addr, value);
}

static const MemoryRegionOps psp_fuses_data_ops = {
    .read = psp_fuses_data_read,
    .write = psp_fuses_data_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const MemoryRegionOps psp_fuses_ctrl_ops = {
    .read = psp_fuses_ctrl_read,
    .write = psp_fuses_ctrl_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void psp_fuses_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspFusesState *s = PSP_FUSES(obj);

    /* Init the registers access */
    memory_region_init_io(&s->data_region, OBJECT(s), &psp_fuses_data_ops, s,
                          "psp-fuses-data",
                          0x1000);
    sysbus_init_mmio(sbd, &s->data_region);
    memory_region_init_io(&s->ctrl_region, OBJECT(s), &psp_fuses_ctrl_ops, s,
                          "psp-fuses-ctrl",
                          0x1000);
    sysbus_init_mmio(sbd, &s->ctrl_region);
}

static void psp_fuses_realize(DeviceState *dev, Error **errp)
{
}

static void psp_fuses_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_fuses_realize;
    dc->desc = "PSP fuses";
}

static const TypeInfo psp_fuses_type_info = {
    .name = TYPE_PSP_FUSES,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspFusesState),
    .instance_init = psp_fuses_init,
    .class_init = psp_fuses_class_init,
};

static void psp_fuses_register_types(void)
{
    type_register_static(&psp_fuses_type_info);
}

type_init(psp_fuses_register_types)
