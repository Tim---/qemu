#ifndef ZEN_PMU_H
#define ZEN_PMU_H

#define TYPE_ZEN_PMU "psp-pmu"

uint16_t zen_pmu_read(DeviceState *s, uint32_t addr);
void zen_pmu_write(DeviceState *s, uint32_t addr, uint16_t data);

#endif
