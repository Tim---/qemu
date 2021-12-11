#ifndef FCH_ZEN_H
#define FCH_ZEN_H

#include "exec/hwaddr.h"
#include "hw/sysbus.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie_host.h"
#include "qom/object.h"

#define TYPE_ZEN_HOST "zen-pcihost"
OBJECT_DECLARE_SIMPLE_TYPE(ZenHost, ZEN_HOST)

#define TYPE_ZEN_ROOT_DEVICE "zen-root"
OBJECT_DECLARE_SIMPLE_TYPE(ZenRootState, ZEN_ROOT_DEVICE)


struct ZenRootState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
};

struct ZenHost {
    /*< private >*/
    PCIExpressHost parent_obj;
    /*< public >*/

    ZenRootState zen_root;

    bool allow_unmapped_accesses;
};

#endif
