#ifndef SMN_MISC_H
#define SMN_MISC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SMN_MISC "smn-misc"
OBJECT_DECLARE_SIMPLE_TYPE(SmnMiscState, SMN_MISC)

struct SmnMiscState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion region;
    MemoryRegion region1;
};


#endif
