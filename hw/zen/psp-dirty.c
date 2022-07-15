#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-fuses.h"


/* Fuses */

void psp_dirty_fuses(zen_codename codename, DeviceState *dev)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        /* Needed unless the UMC is marked as emulation/simulation */
        psp_fuses_write32(dev, 0xcc, 0x20);
        break;
    default:
        break;
    }
}
