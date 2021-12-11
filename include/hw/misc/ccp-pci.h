#ifndef CCP_PCI_H
#define CCP_PCI_H

#include "hw/misc/ccp.h"

#define TYPE_CCP_PCI "ccp-pci"
OBJECT_DECLARE_SIMPLE_TYPE(CcpPciState, CCP_PCI)

struct CcpPciState {
    PCIDevice parent_obj;
    MemoryRegion mmio;
    MemoryRegion msix;
    CcpState ccp;
};

#endif /* CCP_H */
