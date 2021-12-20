#ifndef AMD_DF_H
#define AMD_DF_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_AMD_DF "amd-df"
OBJECT_DECLARE_SIMPLE_TYPE(AmdDfState, AMD_DF)


struct AmdDfState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs;
};


#endif
