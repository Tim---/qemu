#ifndef FCH_ACPI_H
#define FCH_ACPI_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#include "hw/i2c/fch-smbus.h"

#define TYPE_FCH_ACPI "fch-acpi"
OBJECT_DECLARE_SIMPLE_TYPE(FchAcpiState, FCH_ACPI)


struct FchAcpiState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs;
    MemoryRegion regs_alias;

    MemoryRegion regs_pm;
    MemoryRegion regs_acpi;
    MemoryRegion regs_smbus;
    MemoryRegion regs_aoac;
    MemoryRegion regs_unimp;

    uint8_t smbus_slave_control;
    FchSmbusState smbus;
};


#endif
