#ifndef PSP_SOC_REGS_INTERNAL_H
#define PSP_SOC_REGS_INTERNAL_H

#include "hw/arm/psp-soc-regs.h"

uint32_t psp_soc_regs_0a_00_public_read(PspSocRegsState *s, hwaddr offset);
void psp_soc_regs_0a_00_public_write(PspSocRegsState *s, hwaddr offset, uint32_t data);
uint32_t psp_soc_regs_0a_00_private_read(PspSocRegsState *s, hwaddr offset);
void psp_soc_regs_0a_00_private_write(PspSocRegsState *s, hwaddr offset, uint32_t data);

uint32_t psp_soc_regs_0b_05_public_read(PspSocRegsState *s, hwaddr offset);
void psp_soc_regs_0b_05_public_write(PspSocRegsState *s, hwaddr offset, uint32_t data);
uint32_t psp_soc_regs_0b_05_private_read(PspSocRegsState *s, hwaddr offset);
void psp_soc_regs_0b_05_private_write(PspSocRegsState *s, hwaddr offset, uint32_t data);

#endif
