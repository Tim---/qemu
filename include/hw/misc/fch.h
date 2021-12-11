#ifndef FCH_H
#define FCH_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_FCH "fch"
OBJECT_DECLARE_SIMPLE_TYPE(FchState, FCH)


struct FchState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
};


#endif
