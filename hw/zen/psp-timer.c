#include "qemu/osdep.h"
#include "hw/zen/psp-timer.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qemu/timer.h"
#include "hw/ptimer.h"
#include "hw/irq.h"
#include "qemu/log.h"
#include "qemu/range.h"

//#define DEBUG_PSP_TIMER

#ifdef DEBUG_PSP_TIMER
#define DPRINTF(fmt, ...) \
    do { qemu_log_mask(LOG_UNIMP, "%s: " fmt, __func__, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

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

    int64_t start_time;
    qemu_irq irq;

    uint8_t storage[0x24];
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

static inline uint32_t get_reg32(PspTimerState *s, hwaddr offset)
{
    return ldl_le_p(s->storage + offset);
}

static inline void set_reg32(PspTimerState *s, hwaddr offset, uint32_t value)
{
    stl_le_p(s->storage + offset, value);
}

static void psp_timer_hit(void *opaque)
{
    PspTimerState *s = PSP_TIMER(opaque);

    if(get_reg32(s, A_INT_ENABLE))
        qemu_irq_raise(s->irq);
}

static void psp_timer_update_value(PspTimerState *s)
{
    assert(s->state == STATE_COUNT);
    uint32_t value = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->start_time;
    set_reg32(s, A_VALUE, value);
}

static void psp_timer_execute(PspTimerState *s)
{
    uint32_t ctrl0 = get_reg32(s, A_CTRL0);

    ptimer_transaction_begin(s->ptimer);

    if((ctrl0 & 1) == 0) {
        s->state = STATE_STOPPED;
        ptimer_stop(s->ptimer);
    } else if(ctrl0 & 0x10000) {
        s->state = STATE_INTERVAL;
        ptimer_run(s->ptimer, 0);
    } else {
        s->state = STATE_COUNT;
        s->start_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        ptimer_stop(s->ptimer);
    }

    ptimer_transaction_commit(s->ptimer);
}

static uint64_t psp_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    PspTimerState *s = PSP_TIMER(opaque);

    DPRINTF("unimplemented device read  (offset 0x%lx, size 0x%x)\n", offset, size);

    if(ranges_overlap(offset, size, A_VALUE, 4)) {
        psp_timer_update_value(s);
    }

    return ldn_le_p(s->storage + offset, size);
}

static void psp_timer_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    PspTimerState *s = PSP_TIMER(opaque);

    DPRINTF("unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", offset, size, data);

    stn_le_p(s->storage + offset, size, data);

    if(ranges_overlap(offset, size, A_CTRL0, 4)) {
        psp_timer_execute(s);
    }
}

const MemoryRegionOps psp_timer_ops = {
    .read = psp_timer_read,
    .write = psp_timer_write,
    .valid.min_access_size = 1,
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
