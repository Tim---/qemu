#ifndef PSP_FUSES_H
#define PSP_FUSES_H

#define TYPE_PSP_FUSES "psp-fuses"

void psp_fuses_write32(DeviceState *dev, uint32_t addr, uint32_t value);
void psp_fuses_write_bits(DeviceState *dev, uint32_t start, uint32_t size, uint32_t value);

#endif
