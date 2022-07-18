#ifndef ZEN_MOBO_H
#define ZEN_MOBO_H

#define TYPE_ZEN_MOBO "zen-mobo"

MemoryRegion *zen_mobo_get_ht(DeviceState *dev);
MemoryRegion *zen_mobo_get_smn(DeviceState *dev);
void zen_mobo_smn_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias);
void zen_mobo_ht_map(DeviceState *dev, SysBusDevice *sbd, int n, hwaddr addr, bool alias);

#endif
