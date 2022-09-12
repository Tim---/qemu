#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qemu/log.h"
#include "hw/zen/psp-intc.h"
#include "trace.h"
#include "hw/irq.h"
#include "qemu/range.h"

//#define DEBUG_PSP_INTC

#ifdef DEBUG_PSP_INTC
#define DPRINTF(fmt, ...) \
    do { qemu_log_mask(LOG_UNIMP, "%s: " fmt, __func__, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

OBJECT_DECLARE_SIMPLE_TYPE(PspIntcState, PSP_INTC)

#define PSP_INTC_ARRAY_SIZE      4
#define PSP_INTC_NUM_IRQS        (PSP_INTC_ARRAY_SIZE * 32)

struct PspIntcState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_public;
    MemoryRegion regs_private;
    uint32_t active[PSP_INTC_ARRAY_SIZE];
    qemu_irq irq;
    uint8_t storage[0x100];
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
REG8(INT_C127,          0xaf)

REG32(INT_ACK0,         0xb0)
REG32(INT_ACK1,         0xb4)
REG32(INT_ACK2,         0xb8)
REG32(INT_ACK3,         0xbc)

REG32(NO_MORE_INT,      0xc0)
REG32(INT_NUM,          0xc4)

static inline uint32_t get_reg32(PspIntcState *s, hwaddr offset)
{
    return ldl_le_p(s->storage + offset);
}

static inline void set_reg32(PspIntcState *s, hwaddr offset, uint32_t value)
{
    stl_le_p(s->storage + offset, value);
}

static uint32_t psp_intc_get_current_num(PspIntcState *s)
{
    int reg, bit;
    uint32_t enabled, valid;

    for (reg = 0; reg < PSP_INTC_ARRAY_SIZE; reg++) {
        enabled = get_reg32(s, A_INT_ENABLE0 + reg * 4);
        valid = s->active[reg] & enabled;
        if (valid) { 
            for (bit = 0; bit < 32; bit++) {
                if ((valid >> bit) & 1) {
                    return reg * 32 + bit;
                }
            }
        }
    }

    return 0xffffffff;
}

static void psp_intc_irq_update(PspIntcState *s)
{
    uint32_t int_num = psp_intc_get_current_num(s);

    DPRINTF("current IRQ: 0x%x\n", int_num);

    set_reg32(s, A_INT_NUM, int_num);
    set_reg32(s, A_NO_MORE_INT, int_num == 0xffffffff);
    qemu_set_irq(s->irq, int_num != 0xffffffff);
}

static void psp_intc_update_ack(PspIntcState *s)
{
    for(int i = 0; i < 4; i++) {
        uint32_t ack = get_reg32(s, A_INT_ACK0 + 4 * i);
        s->active[i] &= ~ack;
        set_reg32(s, A_INT_ACK0 + 4 * i, 0);
    }

    psp_intc_irq_update(s);
}

static uint64_t psp_intc_read(void *opaque, hwaddr offset, unsigned size)
{
    PspIntcState *s = PSP_INTC(opaque);

    DPRINTF("unimplemented device read  (offset 0x%lx, size 0x%x)\n", offset, size);

    return ldn_le_p(s->storage + offset, size);
}

static void psp_intc_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    PspIntcState *s = PSP_INTC(opaque);

    DPRINTF("unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", offset, size, data);

    stn_le_p(s->storage + offset, size, data);

    if(ranges_overlap(offset, size, A_INT_ACK0, 0x10)) {
        psp_intc_update_ack(s);
    }

    if(ranges_overlap(offset, size, A_INT_ENABLE0, 0x10)) {
        psp_intc_irq_update(s);
    }
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

static void psp_intc_set_irq(void *opaque, int n_IRQ, int level)
{
    PspIntcState *s = PSP_INTC(opaque);

    DPRINTF("IRQ 0x%x -> level %d\n", n_IRQ, level);

    s->active[n_IRQ >> 5] = deposit32(s->active[n_IRQ >> 5], n_IRQ & 0x1f, 1, level);
    psp_intc_irq_update(s);
}

static void psp_intc_realize(DeviceState *dev, Error **errp)
{
    PspIntcState *s = PSP_INTC(dev);

    qdev_init_gpio_in(DEVICE(dev), psp_intc_set_irq, PSP_INTC_NUM_IRQS);
    psp_intc_irq_update(s);
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
