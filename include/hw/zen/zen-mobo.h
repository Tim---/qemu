#ifndef ZEN_MOBO_H
#define ZEN_MOBO_H

#include "zen-cpuid.h"

#define TYPE_ZEN_MOBO "zen-mobo"

MemoryRegion *zen_mobo_get_ht(DeviceState *dev);
MemoryRegion *zen_mobo_get_smn(DeviceState *dev);
ISABus *zen_mobo_get_isa(DeviceState *dev);
PCIBus *zen_mobo_get_pci(DeviceState *dev);
void zen_mobo_smn_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias);
void zen_mobo_smn_map_overlap(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias);
void zen_mobo_ht_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias);
DeviceState *zen_mobo_create(zen_codename codename, BlockBackend *blk);

#endif
