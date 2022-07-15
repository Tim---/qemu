#ifndef PSP_DIRTY_H
#define PSP_DIRTY_H

void psp_dirty_fuses(zen_codename codename, DeviceState *dev);
void psp_dirty_create_mp2_ram(MemoryRegion *smn, zen_codename codename);

#endif
