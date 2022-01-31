#ifndef PSP_SOC_REGS_H
#define PSP_SOC_REGS_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/timer/psp-timer.h"
#include "hw/intc/psp-int.h"

#define TYPE_PSP_SOC_REGS "psp-soc-regs"
#define TYPE_PSP_SOC_REGS_0A_00 "psp-soc-regs-0a-00"
#define TYPE_PSP_SOC_REGS_0B_05 "psp-soc-regs-0b-05"

struct PspSocRegsState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region_public;
    MemoryRegion regs_region_private;
    bool serial_enabled;
};

struct PspSocRegsClass
{
    SysBusDeviceClass parent;
    uint32_t (*public_read  )(struct PspSocRegsState *s, hwaddr offset);
    void     (*public_write )(struct PspSocRegsState *s, hwaddr offset, uint32_t value);
    uint32_t (*private_read )(struct PspSocRegsState *s, hwaddr offset);
    void     (*private_write)(struct PspSocRegsState *s, hwaddr offset, uint32_t value);
};

OBJECT_DECLARE_TYPE(PspSocRegsState, PspSocRegsClass, PSP_SOC_REGS)


#endif
