/*
 * PSP "SoC" registers.
 *
 * Copyright (C) 2021 Timothée Cocault <timothee.cocault@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/registerfields.h"

#include "psp-soc-regs-internal.h"

#define PSP_VERSION     0xbc0a0000
#define MAJOR(version)  (((version) >> 16) & 0xff)
#define MINOR(version)  (((version) >> 8) & 0xff)

#if MINOR(PSP_VERSION) == 0x00
REG32(PUB_SMU_INTERRUPT_READY, 0x28)
REG32(PUB_SERIAL_PORT_ENABLE, 0x3C)
REG32(PUB_BL_VERSION,         0x44)
#define PRIV_GENERIC_REG_BASE  0xA0
#else
REG32(PUB_SMU_INTERRUPT_READY, 0x20)
REG32(PUB_SERIAL_PORT_ENABLE, 0x40)
REG32(PUB_BL_VERSION,         0x48)
#define PRIV_GENERIC_REG_BASE  0x90
#endif

REG32(PRIV_PSP_VERSION,           0x4c)
REG32(PRIV_SECURE_UNLOCK_RELATED, PRIV_GENERIC_REG_BASE + 0x6 * 4)
REG32(PRIV_ENTRY_TYPE,            PRIV_GENERIC_REG_BASE + 0x7 * 4)
REG32(PRIV_SECURE_UNLOCK_STATUS,  PRIV_GENERIC_REG_BASE + 0x8 * 4)
REG32(PRIV_POSTCODE,              PRIV_GENERIC_REG_BASE + 0x14 * 4)
REG32(PUB_CLOCK0,                 0x108)
REG32(PUB_CLOCK1,                 0x10C)

REG32(PUB_SMU_MSG_DATA,           0x700)
REG32(PUB_SMU_MSG_STATUS,         0x704)
/* REG32(PUB_SMU_MSG_???,         0x70C) */
/* REG32(PUB_SMU_MSG_???,         0x710) */
REG32(PUB_SMU_MSG_CMD,            0x714)
/* REG32(PUB_SMU_MSG_???,         0x71C) */

REG32(PUB_BL_VERSION2,      0x9EC)

uint32_t psp_soc_regs_0a_00_public_read(PspSocRegsState *s, hwaddr offset)
{
    switch (offset) {
    case A_PUB_SERIAL_PORT_ENABLE:
        return s->serial_enabled;
    case A_PUB_CLOCK0:
    case A_PUB_CLOCK1:
        return 0;
    case A_PUB_SMU_INTERRUPT_READY:
        return 1;
    case A_PUB_SMU_MSG_STATUS:
        return 1;
    case A_PUB_SMU_MSG_DATA:
        return 0;
    case 0x4:
    case 0x104:
    case 0x518:
    case 0x51c:
    case 0x570:
    case 0x698:
    case 0x69c:
    case 0x6a0:
    case 0x6a4:
    case 0x6a8:
    case 0x6ac:
    case 0x984:
    case 0x998:
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, offset);

    return 0;
}

void psp_soc_regs_0a_00_public_write(PspSocRegsState *s, hwaddr offset,
                                      uint32_t data)
{
    switch (offset) {
    case A_PUB_SERIAL_PORT_ENABLE:
        /* s->serial_enabled = (data != 0); */
        return;
    case A_PUB_BL_VERSION2:
        return;
    case A_PUB_SMU_MSG_DATA:
    case A_PUB_SMU_MSG_CMD:
        return;
    case 0x44:
    case 0x300:
    case 0x304:
    case 0x308:
    case 0x30c:
    case 0x514:
    case 0x544:
    case 0x570:
    case 0x690:
    case 0x698:
    case 0x69c:
    case 0x6a0:
    case 0x6a4:
    case 0x6a8:
    case 0x6ac:
    case 0x70c:
    case 0x710:
    case 0x71c:
    case 0x984:
    case 0x994:
    case 0x998:
    case 0x9e8:
    case 0x9f0:
    case 0x9f4:
    case 0x9f8:
    case 0xa00:
    case 0xa44:
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}


uint32_t psp_soc_regs_0a_00_private_read(PspSocRegsState *s, hwaddr offset)
{
    switch (offset) {
    case A_PRIV_PSP_VERSION:
        return PSP_VERSION;
    case 0xf0:
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                 "(offset 0x%lx)\n",
                 __func__, offset);

    return 0;
}

void psp_soc_regs_0a_00_private_write(PspSocRegsState *s, hwaddr offset,
                                       uint32_t data)
{
    switch (offset) {
    case A_PRIV_POSTCODE:
        return;
    case 0xbc:
    case 0xa0c:
    case 0xa10:
    case 0xa14:
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}
