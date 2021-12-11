#ifndef FCH_SMBUS_H
#define FCH_SMBUS_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#include "hw/i2c/fch-ddr4.h"

#define TYPE_FCH_SMBUS "fch-smbus"
OBJECT_DECLARE_SIMPLE_TYPE(FchSmbusState, FCH_SMBUS)


struct FchSmbusState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    uint8_t status;
    uint8_t slave_status;
    uint8_t control;
    uint8_t host_cmd;
    uint8_t address;
    uint8_t data0;
    uint8_t slave_control;

    I2CBus *bus;

    /* Temporary ! */
    FchSpdBankState spd_bank0;
    FchSpdBankState spd_bank1;
    FchSpdDataState spd_data0;
    FchSpdDataState spd_data1;
    int bank;

    MemoryRegion regs_region;
};

#endif
