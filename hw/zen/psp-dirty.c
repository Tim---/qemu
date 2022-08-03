#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-fuses.h"
#include "qapi/error.h"

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

void psp_dirty_create_mp2_ram(MemoryRegion *smn, zen_codename codename)
{
    MemoryRegion *ram;

    switch(codename) {
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        ram = g_new(MemoryRegion, 1);
        memory_region_init_ram(ram, NULL, "mp2-ram", 0x2a000, &error_fatal);
        memory_region_add_subregion(smn, 0x3f56000, ram);
        break;
    default:
        break;
    }
}

void psp_dirty_create_smu_firmware(MemoryRegion *smn, zen_codename codename)
{
    MemoryRegion *ram;

    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        ram = g_new(MemoryRegion, 1);
        memory_region_init_ram(ram, NULL, "smu-fw", 0x40000, &error_fatal);
        memory_region_add_subregion(smn, 0x3c00000, ram);
        break;
    default:
        break;
    }
}
