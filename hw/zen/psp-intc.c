#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qemu/log.h"
#include "hw/zen/psp-intc.h"
#include "trace.h"
#include "hw/irq.h"

OBJECT_DECLARE_SIMPLE_TYPE(PspIntcState, PSP_INTC)

#define PSP_INTC_ARRAY_SIZE      4
#define PSP_INTC_NUM_IRQS        (PSP_INTC_ARRAY_SIZE * 32)

struct PspIntcState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_public;
    MemoryRegion regs_private;
    uint32_t enabled[PSP_INTC_ARRAY_SIZE];
    uint32_t active[PSP_INTC_ARRAY_SIZE];
    uint32_t a[PSP_INTC_ARRAY_SIZE];
    uint32_t b[PSP_INTC_ARRAY_SIZE];
    uint32_t current_int;
    qemu_irq irq;
};

REG32(INT_ENABLE0,      0x00)
REG32(INT_ENABLE1,      0x04)
REG32(INT_ENABLE2,      0x08)
REG32(INT_ENABLE3,      0x0c)

/* Unknown */
REG32(INT_A0,           0x10)
REG32(INT_A1,           0x14)
REG32(INT_A2,           0x18)
REG32(INT_A3,           0x1c)

/* Unknown */
REG32(INT_B0,           0x20)
REG32(INT_B1,           0x24)
REG32(INT_B2,           0x28)
REG32(INT_B3,           0x2c)

/* Unknown. Interrupts priority ? */
REG8(INT_C0,            0x30)
// ...
REG8(INT_C23,           0x47)

REG32(INT_ACK0,         0xb0)
REG32(INT_ACK1,         0xb4)
REG32(INT_ACK2,         0xb8)
REG32(INT_ACK3,         0xbc)

REG32(NO_MORE_INT,      0xc0)
REG32(INT_NUM,          0xc4)

static void psp_int_irq_update(PspIntcState *pis)
{
    int reg, bit;

    pis->current_int = 0xffffffff;

    for (reg = 0; reg < PSP_INTC_ARRAY_SIZE; reg++) {
        uint32_t valid = pis->active[reg] & pis->enabled[reg];
        if (valid) { 
            for (bit = 0; bit < 32; bit++) {
                if ((valid >> bit) & 1) {
                    pis->current_int = reg * 32 + bit;
                }
            }
        }
    }

    qemu_set_irq(pis->irq, pis->current_int != 0xffffffff);
}

static void set_int_enable(PspIntcState *pis, size_t index, uint64_t data)
{
    uint32_t new_enabled = data & ~pis->enabled[index];
    uint32_t new_disabled = ~data & pis->enabled[index];

    for (int i = 0; i < 32; i++) {
        if ((new_enabled >> i) & 1) {
            trace_psp_intc_enable(index * 32 + i);
        }
        if ((new_disabled >> i) & 1) {
            trace_psp_intc_disable(index * 32 + i);
        }
    }
    pis->enabled[index] = data;

    psp_int_irq_update(pis);
}

static void set_int_ack(PspIntcState *pis, size_t index, uint64_t data)
{
    for (int i = 0; i < 32; i++) {
        if ((data >> i) & 1) {
            trace_psp_intc_ack(index * 32 + i);
        }
    }
    pis->active[index] &= ~data;

    psp_int_irq_update(pis);
}


static uint64_t psp_intc_read(void *opaque, hwaddr offset, unsigned size)
{
    PspIntcState *pis = PSP_INTC(opaque);
    size_t index = offset / 4;

    switch (index) {
    case R_INT_ENABLE0 ... R_INT_ENABLE3:
        return pis->enabled[index - R_INT_ENABLE0];
    case R_NO_MORE_INT:
        return pis->current_int == 0xffffffff;
    case R_INT_NUM:
        return pis->current_int;
    case R_INT_A0 ... R_INT_A3:
        return pis->a[index - R_INT_A0];
    case R_INT_B0 ... R_INT_B3:
        return pis->b[index - R_INT_B0];
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n", __func__, offset);
    return 0;
}

static void psp_intc_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    PspIntcState *pis = PSP_INTC(opaque);
    size_t index = offset / 4;

    switch (index) {
    case R_INT_ENABLE0 ... R_INT_ENABLE3:
        set_int_enable(pis, index - R_INT_ENABLE0, data);
        return;
    case R_INT_A0 ... R_INT_A3:
        pis->a[index - R_INT_A0] = data;
        return;
    case R_INT_B0 ... R_INT_B3:
        pis->b[index - R_INT_B0] = data;
        return;
    case R_INT_ACK0 ... R_INT_ACK3:
        set_int_ack(pis, index - R_INT_ACK0, data);
        return;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, value 0x%lx)\n", __func__, offset, data);
}

const MemoryRegionOps psp_intc_ops = {
    .read = psp_intc_read,
    .write = psp_intc_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_intc_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspIntcState *s = PSP_INTC(obj);

    memory_region_init_io(&s->regs_public, OBJECT(s), &psp_intc_ops, s,
                          "psp-intc-public", 0x100);
    sysbus_init_mmio(sbd, &s->regs_public);
    memory_region_init_io(&s->regs_private, OBJECT(s), &psp_intc_ops, s,
                          "psp-intc-private", 0x100);
    sysbus_init_mmio(sbd, &s->regs_private);

    sysbus_init_irq(sbd, &s->irq);
}

static void psp_int_set_irq(void *opaque, int n_IRQ, int level)
{
    PspIntcState *pis = PSP_INTC(opaque);

    pis->active[n_IRQ >> 5] = deposit32(pis->active[n_IRQ >> 5], n_IRQ & 0x1f, 1, level);
    psp_int_irq_update(pis);
}

static void psp_intc_realize(DeviceState *dev, Error **errp)
{
    PspIntcState *pis = PSP_INTC(dev);

    qdev_init_gpio_in(DEVICE(dev), psp_int_set_irq, PSP_INTC_NUM_IRQS);
    psp_int_irq_update(pis);
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
