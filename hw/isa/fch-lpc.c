/*
 * FCH LPC.
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
#include "sysemu/sysemu.h"
#include "hw/pci/pci.h"
#include "hw/arm/psp-utils.h"
#include "hw/char/serial.h"
#include "hw/misc/psp-debug.h"

#include "hw/isa/fch-lpc.h"

/* D14F3x / FCH::ITF::LPC */

REG32(DEVICE_VENDOR_ID,             0x00)
REG32(IO_PORT_DECODE_ENABLE,        0x44)
    FIELD(IO_PORT_DECODE_ENABLE,        AD_LIB,     31, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        ACPI_UC,    30, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        KBC,        29, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        GAME,       28, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        FDC1,       27, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        FDC0,       26, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MSS3,       25, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MSS2,       24, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MSS1,       23, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MSS0,       22, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MIDI3,      21, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MIDI2,      20, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MIDI1,      19, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        MIDI0,      18, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        AUDIO3,     17, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        AUDIO2,     16, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        AUDIO1,     15, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        AUDIO0,     14, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL7,    13, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL6,    12, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL5,    11, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL4,    10, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL3,     9, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL2,     8, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL1,     7, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        SERIAL0,     6, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        PARALLEL5,   5, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        PARALLEL4,   4, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        PARALLEL3,   3, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        PARALLEL2,   2, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        PARALLEL1,   1, 1)
    FIELD(IO_PORT_DECODE_ENABLE,        PARALLEL0,   0, 1)
REG32(IO_MEM_PORT_DECODE_ENABLE, 0x48)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    WIDE_IO2_EN,                   25, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    WIDE_IO1_EN,                   24, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN6,                   23, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN5,                   22, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN4,                   21, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    MEM_PORT_EN,                   20, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN3,                   19, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN2,                   18, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN1,                   17, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    IO_PORT_EN0,                   16, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    SYNC_TIMEOUT_CNT,               8, 8)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    SYNC_TIMEOUT_CNTR_EN,           7, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    RTC_IO_RANGE_PORT_EN,           6, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    MEMORY_RANGE_PORT_EN,           5, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    WIDE_IO0_EN,                    2, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    ALT_SUPERIO_CONFIG_PORT_EN,     1, 1)
    FIELD(IO_MEM_PORT_DECODE_ENABLE,    SUPERIO_CONFIG_PORT_EN,         0, 1)
REG32(PCI_IO_BASE_ADDR_WIDE_IO,     0x64)
    FIELD(PCI_IO_BASE_ADDR_WIDE_IO,     IO_BASE_ADDRESS1,    16, 16)
    FIELD(PCI_IO_BASE_ADDR_WIDE_IO,     IO_BASE_ADDRESS0,     0, 16)
REG32(SPI_BASE_ADDR,                0xa0)
    FIELD(SPI_BASE_ADDR,                SPI_ESPI_BASE_ADDR,    8, 24)
    FIELD(SPI_BASE_ADDR,                PSP_SPI_MMIO_SEL,      4,  1)
    FIELD(SPI_BASE_ADDR,                ROUTE_TPM2SPI,         3,  1)
    FIELD(SPI_BASE_ADDR,                ABORT_EN,              2,  1)
    FIELD(SPI_BASE_ADDR,                SPI_ROM_EN,            1,  1)
    FIELD(SPI_BASE_ADDR,                ALT_SPI_CS_EN,         0,  1)
REG32(CLK_CNTRL,                    0xd0)
    FIELD(CLK_CNTRL,                    CLK_RUN_EN,                31, 1)
    FIELD(CLK_CNTRL,                    CLK_RUN_DLY_CNTR,          24, 7)
    FIELD(CLK_CNTRL,                    LCLK1_CLK_RUN_OVRID,       22, 1)
    FIELD(CLK_CNTRL,                    LCLK0_CLK_RUN_OVRID,       21, 1)
    FIELD(CLK_CNTRL,                    LCLK1_EN,                  14, 1)
    FIELD(CLK_CNTRL,                    LCLK0_EN,                  13, 1)
    FIELD(CLK_CNTRL,                    LPC_CLK_RUN_EN,             7, 1)
    FIELD(CLK_CNTRL,                    SPI_ON_CLK_RUN,             2, 1)
    FIELD(CLK_CNTRL,                    CLK_GATE_CNTRL,             0, 2)



static void fch_lpc_init(Object *obj)
{
}

static void fch_lpc_realize(PCIDevice *dev, Error **errp)
{
    ISABus *isa_bus;
    FchLpcState *lpc = FCH_LPC(dev);

    isa_bus = isa_bus_new(DEVICE(dev), pci_address_space(dev),
                          get_system_io(), errp);
    if (!isa_bus) {
        return;
    }
    lpc->isa_bus = isa_bus;

    isa_bus_irqs(isa_bus, lpc->isa_irqs);

    /* Create serial ports */
    serial_hds_isa_init(lpc->isa_bus, 0, 1);

    /* Debug port */
    ISADevice *isa = isa_new(TYPE_PSP_DEBUG);
    isa_realize_and_unref(isa, isa_bus, errp);
}

static void fch_lpc_exit(PCIDevice *dev)
{
}

static void fch_lpc_config_write(PCIDevice *d, uint32_t addr,
                                 uint32_t val, int len)
{
    assert(len == 4);
    switch (addr) {
    case A_DEVICE_VENDOR_ID:
        assert(val == 0xffffff00); /* why ? */
        return;
    case A_IO_PORT_DECODE_ENABLE:
        assert(val == 0xc0); /* enable serial port 0/1 */
        return;
    case A_IO_MEM_PORT_DECODE_ENABLE:
        /* 0x200000: IO_PORT_EN4 */
        /* 0x7: SUPERIO_CONFIG_PORT_EN | ALT_SUPERIO_CONFIG_PORT_EN | WIDE_IO0_EN */
        assert(val == 0x200000 || val == 0x7);
        return;
    case A_PCI_IO_BASE_ADDR_WIDE_IO:
        /* Wide IO 0 base address */
        assert(val == 0x1640);
        return;
    case A_SPI_BASE_ADDR:
        /* 
         * assert(val == 0xfec10002);
         *  AltSpiCSEnable = 0
         *  SpiRomEnable = 1
         *  AbortEnable = 0
         *  RouteTpm2Spi = 0
         *  PspSpiMmioSel = 0
         *  Spi_eSpi_BaseAddr = 0xfec10000
         */
        break;
    case A_CLK_CNTRL:
        /*
         * Lclk1ClkrunOvrid = 1
         */
        assert(val == 0x40000);
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%x, value 0x%x)\n",
                  __func__, addr, val);
}

static uint32_t fch_lpc_config_read(PCIDevice *d, uint32_t addr, int len)
{
    assert(len == 4);

    int res = 0;

    switch (addr) {
    case A_IO_PORT_DECODE_ENABLE:
    case A_IO_MEM_PORT_DECODE_ENABLE:
    case A_PCI_IO_BASE_ADDR_WIDE_IO:
    case A_CLK_CNTRL:
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%x)\n", __func__, addr);

    return res;
}

static void fch_lpc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(oc);

    dc->desc = "AMD FCH LPC";

    k->realize      = fch_lpc_realize;
    k->exit         = fch_lpc_exit;
    k->vendor_id    = PCI_VENDOR_ID_AMD;
    k->device_id    = 0x790E;
    k->class_id     = PCI_CLASS_BRIDGE_ISA;
    k->config_write = fch_lpc_config_write;
    k->config_read  = fch_lpc_config_read;
}

static const TypeInfo fch_lpc_type_info = {
    .name = TYPE_FCH_LPC,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(FchLpcState),
    .instance_init = fch_lpc_init,
    .class_init = fch_lpc_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { }
    },
};

static void fch_lpc_register_types(void)
{
    type_register_static(&fch_lpc_type_info);
}

type_init(fch_lpc_register_types)
