#ifndef PSP_UTILS_H
#define PSP_UTILS_H

#include "hw/sysbus.h"

void create_unimplemented_device_custom(const char *name, MemoryRegion *region,
                                        hwaddr base, hwaddr size, bool ones);

void stub_create(const char *name, MemoryRegion *parent_region,
                 hwaddr offset, uint32_t size, uint64_t value);

#endif
