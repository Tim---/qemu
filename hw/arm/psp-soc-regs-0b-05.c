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

REG32(PUB_SMU_INTERRUPT_READY, 0x24)
REG32(PRIV_CRYPTO_FLAGS, 0x50)

uint32_t psp_soc_regs_0b_05_public_read(PspSocRegsState *s, hwaddr offset)
{
    switch (offset) {
    case A_PUB_SMU_INTERRUPT_READY:
        return 1;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, offset);

    return 0;
}

void psp_soc_regs_0b_05_public_write(PspSocRegsState *s, hwaddr offset,
                                      uint32_t data)
{
    switch (offset) {
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}


uint32_t psp_soc_regs_0b_05_private_read(PspSocRegsState *s, hwaddr offset)
{
    switch (offset) {
    case A_PRIV_CRYPTO_FLAGS:
        /* Allows RSA-4096 signatures */
        return 0x00000300;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                 "(offset 0x%lx)\n",
                 __func__, offset);

    return 0;
}

void psp_soc_regs_0b_05_private_write(PspSocRegsState *s, hwaddr offset,
                                       uint32_t data)
{
    switch (offset) {
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%x)\n",
                  __func__, offset, data);
}
