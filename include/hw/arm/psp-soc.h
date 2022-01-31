#ifndef PSP_SOC_H
#define PSP_SOC_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "cpu.h"
#include "hw/arm/psp-soc-regs.h"
#include "hw/arm/smn-bridge.h"
#include "hw/arm/x86-bridge.h"
#include "hw/misc/fch-acpi.h"
#include "hw/ssi/fch-spi.h"
#include "hw/misc/ccp.h"

#define TYPE_PSP_SOC "psp-soc"
#define TYPE_PSP_SOC_0A_00 "psp-soc-0a-00"
#define TYPE_PSP_SOC_0B_05 "psp-soc-0b-05"

struct PspSocClass {
    DeviceClass parent;
    uint32_t version;
    const char *regs_type;
};

OBJECT_DECLARE_TYPE(PspSocState, PspSocClass, PSP_SOC)

struct PspSocState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    ARMCPU cpu;
    char *cpu_type;
    MemoryRegion *ram;
    PspSocRegsState soc_regs;
    MemoryRegion soc_regs_region_public;
    MemoryRegion soc_regs_region_private;
    CcpState ccp;

    SmnBridgeState smn_bridge;
    MemoryRegion *smn_region;

    X86BridgeState x86_bridge;
    MemoryRegion *x86_region;

    PspTimerState timer;
    PspIntState interrupt_controller;
};


#endif
