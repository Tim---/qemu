#ifndef HT_BRIDGE_H
#define HT_BRIDGE_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_HT_BRIDGE "ht-bridge"
OBJECT_DECLARE_SIMPLE_TYPE(HTBridgeState, HT_BRIDGE)

/* Number of low bits */
#define HT_BRIDGE_MAPPING_BITS 0x1a
#define HT_BRIDGE_MAPPING_SIZE (1UL << HT_BRIDGE_MAPPING_BITS)
#define HT_BRIDGE_NUM_MAPPINGS 15

#define HT_BRIDGE_MAX_MAPPINGS 0x3E

typedef struct {
    uint32_t addr;
    uint32_t status;
    uint32_t type1;
    uint32_t type2;
} ht_bridge_slot_t;

struct HTBridgeState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion mapped_region;
    MemoryRegion exposed_region;

    ht_bridge_slot_t slots[HT_BRIDGE_MAX_MAPPINGS];
    MemoryRegion aliases[HT_BRIDGE_MAX_MAPPINGS];
    uint32_t A[HT_BRIDGE_MAX_MAPPINGS];
    uint32_t B[HT_BRIDGE_MAX_MAPPINGS];

    MemoryRegion *source_memory;
};


#endif
