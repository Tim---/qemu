#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qemu/log.h"
#include "hw/zen/psp-mb.h"
#include "trace.h"
#include "hw/irq.h"
#include "qemu/range.h"

//#define DEBUG_PSP_MB

#ifdef DEBUG_PSP_MB
#define DPRINTF(fmt, ...) \
    do { qemu_log_mask(LOG_UNIMP, "%s: " fmt, __func__, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

OBJECT_DECLARE_SIMPLE_TYPE(PspMbState, PSP_MB)

#define NUM_IRQS 0x10

struct PspMbState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs;
    uint8_t storage[0x100];
    qemu_irq irq[NUM_IRQS];
};

static uint64_t psp_mb_read(void *opaque, hwaddr offset, unsigned size)
{
    PspMbState *s = PSP_MB(opaque);

    DPRINTF("unimplemented device read  (offset 0x%lx, size 0x%x)\n", offset, size);

    return ldn_le_p(s->storage + offset, size);
}

static void psp_mb_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    PspMbState *s = PSP_MB(opaque);

    DPRINTF("unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", offset, size, data);

    stn_le_p(s->storage + offset, size, data);

    for(int i = 0; i < NUM_IRQS; i++) {
        if(ranges_overlap(offset, size, 4 * i, 4 * (i+1))) {
            qemu_irq_raise(s->irq[i]);
        }
    }
}

const MemoryRegionOps psp_mb_ops = {
    .read = psp_mb_read,
    .write = psp_mb_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

static void psp_mb_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspMbState *s = PSP_MB(obj);

    memory_region_init_io(&s->regs, OBJECT(s), &psp_mb_ops, s,
                          "psp-mb", 0x100);
    sysbus_init_mmio(sbd, &s->regs);

    for (int i = 0; i < NUM_IRQS; i++) {
        sysbus_init_irq(sbd, &s->irq[i]);
    }
}

static void psp_mb_realize(DeviceState *dev, Error **errp)
{
}

static void psp_mb_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_mb_realize;
    dc->desc = "PSP mailbox region";
}

static const TypeInfo psp_mb_type_info = {
    .name = TYPE_PSP_MB,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspMbState),
    .instance_init = psp_mb_init,
    .class_init = psp_mb_class_init,
};

static void psp_mb_register_types(void)
{
    type_register_static(&psp_mb_type_info);
}

type_init(psp_mb_register_types)
