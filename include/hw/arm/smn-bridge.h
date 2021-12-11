#ifndef SMN_BRIDGE_H
#define SMN_BRIDGE_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SMN_BRIDGE "smn-bridge"
OBJECT_DECLARE_SIMPLE_TYPE(SmnBridgeState, SMN_BRIDGE)


#define SMN_BRIDGE_MAPPING_BITS 0x14       // Number of low bits
#define SMN_BRIDGE_MAPPING_SIZE (1 << SMN_BRIDGE_MAPPING_BITS)
#define SMN_BRIDGE_NUM_MAPPINGS 0x20

struct SmnBridgeState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion mapped_region;
    MemoryRegion aliases[SMN_BRIDGE_NUM_MAPPINGS];

    MemoryRegion *source_memory;    // Property
};


#endif
