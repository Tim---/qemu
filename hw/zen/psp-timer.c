#include "qemu/osdep.h"
#include "hw/zen/psp-timer.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qemu/timer.h"
#include "hw/ptimer.h"
#include "hw/irq.h"

OBJECT_DECLARE_SIMPLE_TYPE(PspTimerState, PSP_TIMER)

typedef enum {
    STATE_STOPPED,
    STATE_COUNT,
    STATE_INTERVAL,
} psp_timer_state_e;

struct PspTimerState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    uint32_t ticks;

    ptimer_state *ptimer;
    psp_timer_state_e state;
    uint32_t ctrl0;
    uint32_t ctrl1;
    uint32_t ctrl2;
    uint32_t int_enable;
    uint32_t interval;

    int64_t start_time;
    qemu_irq irq;
};

/**
 * We have 2 modes of operations:
 * 
 * - busy sleep (implemented... kind of)
 *   set CTRL0 = 0x101
 *   read VALUE in a loop
 * 
 *   (assume VALUE is in nanoseconds ?)
 * 
 * - interval mode
 *   set CTRL2 = 0
 *   set INT_ENABLE = 1
 *   set INTERVAL = value * 100000
 *   set CTRL1 = 0x100
 *   set CTRL0 = 0x10101
 *   wait for interrupts
 * 
 *   (INTERVAL is set to 2e9, which is huge !)
 * 
 * - interval mode 2 !
 *   set INT_ENABLE = 0
 *   set CTRL0 = 0x10000
 *   set INTERVAL = value * 25
 *   set CTRL1 = 0x100
 *   set CTRL0 = 0x10100
 *   set CTRL0 = 0x10001
 *   set INT_ENABLE = 1
 *  - disable interval mode 2:
 *   set INT_ENABLE = 0
 */

REG32(CTRL0,        0x00)
REG32(CTRL1,        0x04)
REG32(CTRL2,        0x08)
REG32(INT_ENABLE,   0x0C)
REG32(INTERVAL,     0x10)
REG32(VALUE,        0x20)

static void psp_timer_hit(void *opaque)
{
    PspTimerState *s = PSP_TIMER(opaque);

    qemu_irq_raise(s->irq);
}

static void psp_timer_execute(PspTimerState *s)
{
    ptimer_transaction_begin(s->ptimer);

    switch(s->ctrl0) {
    case 0:
        s->state = STATE_STOPPED;
        ptimer_stop(s->ptimer);
        break;
    case 0x101:
        s->state = STATE_COUNT;
        s->start_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        ptimer_stop(s->ptimer);
        break;
    case 0x10001:
    case 0x10101:
        s->state = STATE_INTERVAL;
        ptimer_run(s->ptimer, 0);
        break;
    }

    ptimer_transaction_commit(s->ptimer);
}

static uint64_t psp_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    PspTimerState *s = PSP_TIMER(opaque);

    switch (offset) {
    case A_VALUE:
        assert(s->state == STATE_COUNT);
        return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->start_time;
    default:
        g_assert_not_reached();
    }

    return 0;
}

static void psp_timer_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    PspTimerState *s = PSP_TIMER(opaque);

    switch (offset) {
    case A_CTRL0:
        s->ctrl0 = data;
        psp_timer_execute(s);
        return;
    case A_CTRL1:
        assert(data == 0 || data == 0x100);
        s->ctrl1 = data;
        return;
    case A_CTRL2:
        assert(data == 0);
        s->ctrl2 = data;
        return;
    case A_INT_ENABLE:
        assert(data == 0 || data == 1);
        s->int_enable = data;
        return;
    case A_INTERVAL:
        s->interval = data;
        return;
    default:
        assert(data == 0);
    }
}

const MemoryRegionOps psp_timer_ops = {
    .read = psp_timer_read,
    .write = psp_timer_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void psp_timer_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PspTimerState *pts = PSP_TIMER(obj);

    memory_region_init_io(&pts->regs_region, OBJECT(pts), &psp_timer_ops, pts,
                          "psp-timer", 0x24);
    sysbus_init_mmio(sbd, &pts->regs_region);

    sysbus_init_irq(sbd, &pts->irq);
}

static void psp_timer_realize(DeviceState *dev, Error **errp)
{
    PspTimerState *s = PSP_TIMER(dev);

    s->ptimer = ptimer_init(psp_timer_hit, s, PTIMER_POLICY_LEGACY);

    ptimer_transaction_begin(s->ptimer);
    /* This is completely temporary */
    ptimer_set_freq(s->ptimer, 1000);
    ptimer_set_limit(s->ptimer, 100, 0);
    ptimer_transaction_commit(s->ptimer);
}

static void psp_timer_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_timer_realize;
    dc->desc = "PSP timer";
}

static const TypeInfo psp_timer_type_info = {
    .name = TYPE_PSP_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PspTimerState),
    .instance_init = psp_timer_init,
    .class_init = psp_timer_class_init,
};

static void psp_timer_register_types(void)
{
    type_register_static(&psp_timer_type_info);
}

type_init(psp_timer_register_types)
