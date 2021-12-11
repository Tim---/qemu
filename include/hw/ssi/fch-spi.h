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
};


#endif
