#ifndef PSP_UMC_H
#define PSP_UMC_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_PSP_UMC "psp-umc"
OBJECT_DECLARE_SIMPLE_TYPE(PspUmcState, PSP_UMC)

struct PspUmcState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
};


#endif
