#ifndef FCH_LPC_H
#define FCH_LPC_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/pci/pci.h"

#define TYPE_FCH_LPC "fch-lpc"
OBJECT_DECLARE_SIMPLE_TYPE(FchLpcState, FCH_LPC)


struct FchLpcState {
    /*< private >*/
    PCIDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    ISABus *isa_bus;
    qemu_irq isa_irqs[ISA_NUM_IRQS];
};


#endif
