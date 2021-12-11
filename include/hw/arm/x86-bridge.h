#ifndef X86_BRIDGE_H
#define X86_BRIDGE_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_X86_BRIDGE "x86-bridge"
OBJECT_DECLARE_SIMPLE_TYPE(X86BridgeState, X86_BRIDGE)


#define X86_BRIDGE_MAPPING_BITS 0x1a       // Number of low bits
#define X86_BRIDGE_MAPPING_SIZE (1UL << X86_BRIDGE_MAPPING_BITS)
#define X86_BRIDGE_NUM_MAPPINGS 15

#define X86_BRIDGE_MAX_MAPPINGS 0x3E

typedef struct {
    uint32_t addr;
    uint32_t status;
    uint32_t type1;
    uint32_t type2;
} x86_bridge_slot_t;

struct X86BridgeState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion mapped_region;
    MemoryRegion exposed_region;

    x86_bridge_slot_t slots[X86_BRIDGE_MAX_MAPPINGS];
    MemoryRegion aliases[X86_BRIDGE_MAX_MAPPINGS];
    uint32_t A[X86_BRIDGE_MAX_MAPPINGS];
    uint32_t B[X86_BRIDGE_MAX_MAPPINGS];

    MemoryRegion *source_memory;    // Property
};


#endif
