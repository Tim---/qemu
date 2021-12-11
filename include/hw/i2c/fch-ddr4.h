#ifndef FCH_DDR4_H
#define FCH_DDR4_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/i2c/smbus_slave.h"

#define TYPE_FCH_SPD_BANK "fch-spd-bank"
OBJECT_DECLARE_SIMPLE_TYPE(FchSpdBankState, FCH_SPD_BANK)

struct FchSpdBankState {
    SMBusDevice parent_obj;

    int *bank;
};

#define TYPE_FCH_SPD_DATA "fch-spd-data"
OBJECT_DECLARE_SIMPLE_TYPE(FchSpdDataState, FCH_SPD_DATA)

struct FchSpdDataState {
    SMBusDevice parent_obj;

    int *bank;
    int offset;
    uint8_t data[512];
};


#endif
