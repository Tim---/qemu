#ifndef ZEN_UTILS_H
#define ZEN_UTILS_H

void create_unimplemented_device_generic(MemoryRegion *region, const char *name,
                                         hwaddr base, hwaddr size);
void create_region_with_unimpl(MemoryRegion *region, Object *owner,
                                      const char *name, uint64_t size);

#endif
