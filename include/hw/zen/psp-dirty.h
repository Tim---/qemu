#ifndef PSP_DIRTY_H
#define PSP_DIRTY_H

void psp_create_config(MemoryRegion *container, zen_codename codename);
void psp_dirty_fuses(zen_codename codename, DeviceState *dev);

#endif
