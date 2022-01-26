#ifndef FCH_SPI_H
#define FCH_SPI_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_FCH_SPI "fch-spi"
OBJECT_DECLARE_SIMPLE_TYPE(FchSpiState, FCH_SPI)


struct FchSpiState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion direct_access;
    BlockBackend *blk;

    SSIBus *spi;

    uint32_t cntrl0;
    uint32_t cntrl1;

    uint8_t ext_idx;
    uint8_t write_count;
    uint8_t read_count;
    uint8_t fifo[8];

    qemu_irq cs0;
};


#endif
