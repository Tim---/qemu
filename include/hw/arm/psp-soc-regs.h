#ifndef PSP_SOC_REGS_H
#define PSP_SOC_REGS_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/timer/psp-timer.h"
#include "hw/intc/psp-int.h"

#define TYPE_PSP_SOC_REGS "psp-soc-regs"
OBJECT_DECLARE_SIMPLE_TYPE(PspSocRegsState, PSP_SOC_REGS)

struct PspSocRegsState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region_public;
    MemoryRegion regs_region_private;
    bool serial_enabled;
};


#endif
