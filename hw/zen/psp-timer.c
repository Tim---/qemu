#include "qemu/osdep.h"
#include "hw/zen/psp-timer.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"

OBJECT_DECLARE_SIMPLE_TYPE(PspTimerState, PSP_TIMER)

struct PspTimerState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    uint32_t ticks;
};

/**
 * We have 2 modes of operations:
 * 
 * - busy sleep (implemented... kind of)
 *   set CTRL0 = 0x101
 *   read VALUE in a loop
 * 
 * - interval mode (not implemented)
 *   set CTRL2 = 0
 *   set CTRL3 = 1
 *   set INTERVAL = value * 100000
 *   set CTRL1 = 0x100
 *   set CTRL0 = 0x10101
 *   wait for interrupts
 */

REG32(CTRL0,        0x00)
REG32(CTRL1,        0x04)
REG32(CTRL2,        0x08) /* Int enable ? */
REG32(CTRL3,        0x0C)
REG32(INTERVAL,     0x10)
REG32(VALUE,        0x20)

static uint64_t psp_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    PspTimerState *pts = PSP_TIMER(opaque);

    switch (offset) {
    case A_VALUE:
        pts->ticks += 10000; /* TODO: dirty hack ! */
        return pts->ticks;
    default:
        g_assert_not_reached();
    }

    return 0;
}

static void psp_timer_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    switch (offset) {
    case A_CTRL0:
        assert(data == 0 || data == 0x101);
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
}

static void psp_timer_realize(DeviceState *dev, Error **errp)
{
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
