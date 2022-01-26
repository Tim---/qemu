/*
 * FCH SPI device.
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
#include "hw/char/serial.h"
#include "sysemu/sysemu.h"
#include "qemu/qemu-print.h"
#include "hw/qdev-properties-system.h"
#include "sysemu/block-backend.h"
#include "hw/ssi/ssi.h"
#include "hw/irq.h"

#include "hw/ssi/fch-spi.h"

REG32(SPI_CNTRL0,       0x00)
    FIELD(SPI_CNTRL0, SPI_OPCODE,                0, 8)
    FIELD(SPI_CNTRL0, EXECUTE_OPCODE,           16, 1)
    FIELD(SPI_CNTRL0, SPI_READ_MODE_0,          18, 1)
    FIELD(SPI_CNTRL0, SPI_ARB_ENABLE,           19, 1)
    FIELD(SPI_CNTRL0, FIFO_PTR_CLR,             20, 1)
    FIELD(SPI_CNTRL0, ILLEGAL_ACCESS,           21, 1)
    FIELD(SPI_CNTRL0, SPI_ACCESS_MAC_ROM_EN,    22, 1)
    FIELD(SPI_CNTRL0, SPI_HOST_ACCESS_ROM_EN,   23, 1)
    FIELD(SPI_CNTRL0, SPI_CLK_GATE,             28, 1)
    FIELD(SPI_CNTRL0, SPI_READ_MODE_2_1,        29, 2)
    FIELD(SPI_CNTRL0, SPI_BUSY,                 31, 1)
REG32(SPI_CNTRL1,       0x0C)
    FIELD(SPI_CNTRL1, SPI_PARAMETERS,            0, 8)
    FIELD(SPI_CNTRL1, FIFO_PTR,                  8, 3)
    FIELD(SPI_CNTRL1, TRACK_MAC_LOCK_EN,        11, 1)
    FIELD(SPI_CNTRL1, WAIT_COUNT,               16, 6)
    FIELD(SPI_CNTRL1, BYTE_COMMAND,             24, 8)
REG8(SPI_EXT_REG_INDX,  0x1E)
REG8(SPI_EXT_REG_DATA,  0x1F)
REG8(SPI100_ENABLE,     0x20)

enum {
    EXT_TX_BYTE_COUNT = 0x05,
    EXT_RX_BYTE_COUNT = 0x06,
};

static void fch_spi_ext_write(FchSpiState *s, uint8_t value)
{
    switch(s->ext_idx) {
    case EXT_TX_BYTE_COUNT:
        s->write_count = value;
        break;
    case EXT_RX_BYTE_COUNT:
        s->read_count = value;
        break;
    }
}

static void fch_spi_execute(FchSpiState *s)
{
    assert(FIELD_EX32(s->cntrl0, SPI_CNTRL0, SPI_ARB_ENABLE) == 0);
    assert(FIELD_EX32(s->cntrl0, SPI_CNTRL0, ILLEGAL_ACCESS) == 0);
    assert(FIELD_EX32(s->cntrl0, SPI_CNTRL0, SPI_ACCESS_MAC_ROM_EN) == 0);
    assert(FIELD_EX32(s->cntrl0, SPI_CNTRL0, SPI_HOST_ACCESS_ROM_EN) == 0);
    assert(FIELD_EX32(s->cntrl0, SPI_CNTRL0, SPI_CLK_GATE) == 0);
    assert(FIELD_EX32(s->cntrl0, SPI_CNTRL0, SPI_BUSY) == 0);

    // Clear the fifo pointer
    if(FIELD_EX32(s->cntrl0, SPI_CNTRL0, FIFO_PTR_CLR) == 1) {
        s->cntrl1 = FIELD_DP32(s->cntrl1, SPI_CNTRL1, FIFO_PTR, 0);
        s->cntrl0 = FIELD_DP32(s->cntrl0, SPI_CNTRL0, FIFO_PTR_CLR, 0);
    }

    // Execute opcode
    if(FIELD_EX32(s->cntrl0, SPI_CNTRL0, EXECUTE_OPCODE) == 1) {

        uint8_t op = FIELD_EX32(s->cntrl0, SPI_CNTRL0, SPI_OPCODE);

        qemu_irq_lower(s->cs0);

        ssi_transfer(s->spi, op);

        int fifo_ptr = FIELD_EX32(s->cntrl1, SPI_CNTRL1, FIFO_PTR);

        for(int i = 0; i < s->write_count; i++) {
            ssi_transfer(s->spi, s->fifo[fifo_ptr]);
            fifo_ptr = (fifo_ptr + 1) % 8;
        }

        s->cntrl1 = FIELD_DP32(s->cntrl1, SPI_CNTRL1, FIFO_PTR, fifo_ptr);

        for(int i = 0; i < s->read_count; i++) {
            s->fifo[fifo_ptr] = ssi_transfer(s->spi, 0);
            fifo_ptr = (fifo_ptr + 1) % 8;
        }

        qemu_irq_raise(s->cs0);

        s->cntrl0 = FIELD_DP32(s->cntrl0, SPI_CNTRL0, EXECUTE_OPCODE, 0);
    }
}

static uint64_t fch_spi_read(void *opaque, hwaddr offset, unsigned size)
{
    FchSpiState *s = FCH_SPI(opaque);

    switch (offset) {
    case A_SPI_CNTRL0:
        return s->cntrl0;
    case A_SPI_CNTRL1: ;
        int fifo_ptr = FIELD_EX32(s->cntrl1, SPI_CNTRL1, FIFO_PTR);
        s->cntrl1 = FIELD_DP32(s->cntrl1, SPI_CNTRL1, SPI_PARAMETERS, s->fifo[fifo_ptr]);
        fifo_ptr = (fifo_ptr + 1) % 8;
        s->cntrl1 = FIELD_DP32(s->cntrl1, SPI_CNTRL1, FIFO_PTR, fifo_ptr);
        return s->cntrl1;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);

    return 0;
}

static void fch_spi_write(void *opaque, hwaddr offset,
                          uint64_t data, unsigned size)
{
    FchSpiState *s = FCH_SPI(opaque);

    switch (offset) {
    case A_SPI_CNTRL0:
        s->cntrl0 = data;
        fch_spi_execute(s);
        break;
    case A_SPI_EXT_REG_INDX:
        s->ext_idx = data;
        return;
    case A_SPI_EXT_REG_DATA:
        fch_spi_ext_write(s, data);
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_spi_ops = {
    .read = fch_spi_read,
    .write = fch_spi_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

static uint64_t fch_spi_da_read(void *opaque, hwaddr offset, unsigned size)
{
    FchSpiState *s = FCH_SPI(opaque);

    qemu_irq_lower(s->cs0);

    ssi_transfer(s->spi, 0x03);
    ssi_transfer(s->spi, extract32(offset, 16, 8));
    ssi_transfer(s->spi, extract32(offset,  8, 8));
    ssi_transfer(s->spi, extract32(offset,  0, 8));

    uint64_t res = 0;
    
    for(int i = 0; i < size; i++) {
        res = deposit64(res, 8 * i, 8, ssi_transfer(s->spi, 0));
    }

    qemu_irq_raise(s->cs0);

    return res;
}

static void fch_spi_da_write(void *opaque, hwaddr offset,
                          uint64_t data, unsigned size)
{
    g_assert_not_reached();
}

const MemoryRegionOps fch_spi_da_ops = {
    .read = fch_spi_da_read,
    .write = fch_spi_da_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .valid.unaligned = true,
};

static void fch_spi_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    FchSpiState *s = FCH_SPI(obj);

    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_spi_ops, s,
                          "fch-spi", 0x80);
    sysbus_init_mmio(sbd, &s->regs_region);

    memory_region_init_io(&s->direct_access, OBJECT(s), &fch_spi_da_ops, s,
                          "fch-spi-direct-access", 0x01000000);
    sysbus_init_mmio(sbd, &s->direct_access);

    sysbus_init_irq(sbd, &s->cs0);

    s->spi = ssi_create_bus(DEVICE(s), "spi");
}

static void fch_spi_realize(DeviceState *dev, Error **errp)
{
}

static void fch_spi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_spi_realize;
    dc->desc = "AMD FCH SPI";
}

static const TypeInfo fch_spi_type_info = {
    .name = TYPE_FCH_SPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchSpiState),
    .instance_init = fch_spi_init,
    .class_init = fch_spi_class_init,
};

static void fch_spi_register_types(void)
{
    type_register_static(&fch_spi_type_info);
}

type_init(fch_spi_register_types)
