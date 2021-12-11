#ifndef PSP_INT_H
#define PSP_INT_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_PSP_INT "psp-int"
OBJECT_DECLARE_SIMPLE_TYPE(PspIntState, PSP_INT)

#define PSP_INT_ARRAY_SIZE      4
#define PSP_INT_NUM_IRQS        (PSP_INT_ARRAY_SIZE * 32)
struct PspIntState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region_public;
    MemoryRegion regs_region_private;

    uint32_t enabled[PSP_INT_ARRAY_SIZE];
    uint32_t active[PSP_INT_ARRAY_SIZE];
    uint32_t current_int;
    qemu_irq irq;
};


#endif
