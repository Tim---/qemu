#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qemu/log.h"
#include "hw/zen/psp-intc.h"

OBJECT_DECLARE_SIMPLE_TYPE(PspIntcState, PSP_INTC)

struct PspIntcState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_public;
    MemoryRegion regs_private;
};

static uint64_t psp_intc_public_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n", __func__, offset);
    return 0;
}

static void psp_intc_public_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, value 0x%lx)\n", __func__, offset, data);
}

const MemoryRegionOps psp_intc_public_ops = {
    .read = psp_intc_public_read,
    .write = psp_intc_public_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static uint64_t psp_intc_private_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n", __func__, offset);
    return 0;
}

static void psp_intc_private_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, value 0x%lx)\n", __func__, offset, data);
}

const MemoryRegionOps psp_intc_private_ops = {
    .read = psp_intc_private_read,
    .write = psp_intc_private_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_intc_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspIntcState *s = PSP_INTC(obj);

    memory_region_init_io(&s->regs_public, OBJECT(s), &psp_intc_public_ops, s,
                          "psp-intc-public", 0x100);
    sysbus_init_mmio(sbd, &s->regs_public);
    memory_region_init_io(&s->regs_private, OBJECT(s), &psp_intc_private_ops, s,
                          "psp-intc-private", 0x100);
    sysbus_init_mmio(sbd, &s->regs_private);
}

static void psp_intc_realize(DeviceState *dev, Error **errp)
{
}

static void psp_intc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_intc_realize;
    dc->desc = "PSP interrupt controller";
}

static const TypeInfo psp_intc_type_info = {
    .name = TYPE_PSP_INTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspIntcState),
    .instance_init = psp_intc_init,
    .class_init = psp_intc_class_init,
};

static void psp_intc_register_types(void)
{
    type_register_static(&psp_intc_type_info);
}

type_init(psp_intc_register_types)
