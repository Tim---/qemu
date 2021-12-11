#ifndef PSP_SMU_H
#define PSP_SMU_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_PSP_SMU "psp-smu"
OBJECT_DECLARE_SIMPLE_TYPE(PspSmuState, PSP_SMU)

struct PspSmuState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
};


#endif
