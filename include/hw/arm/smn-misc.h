#ifndef SMN_MISC_H
#define SMN_MISC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SMN_MISC "smn-misc"
#define TYPE_SMN_MISC_0A_00 "smn-misc-0a-00"
#define TYPE_SMN_MISC_0B_05 "smn-misc-0b-05"

OBJECT_DECLARE_TYPE(SmnMiscState, SmnMiscClass, SMN_MISC)

struct SmnMiscClass {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    uint32_t cpuid;
};

struct SmnMiscState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion region;
    MemoryRegion region1;
};


#endif
