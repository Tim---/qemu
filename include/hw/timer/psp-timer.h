#ifndef PSP_TIMER_H
#define PSP_TIMER_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_PSP_TIMER "psp-timer"
OBJECT_DECLARE_SIMPLE_TYPE(PspTimerState, PSP_TIMER)

struct PspTimerState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    uint32_t ticks;
    bool int_enable;
    qemu_irq irq;
    QEMUTimer *qemu_timer;
};


#endif
