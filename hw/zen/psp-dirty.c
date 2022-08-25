#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-fuses.h"
#include "qapi/error.h"

/* Fuses */

static void psp_dirty_core_dis_by_fuse(zen_codename codename, DeviceState *dev)
{
    /*
    CoreDisByFuse register
    Guesses:
        bits 0-7: bitmap of disabled cores
        bit 8: enable hyperthreading
    
    We only enable the first core
    */
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        psp_fuses_write32(dev, 0x25c, 0x000000fe);
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        psp_fuses_write32(dev, 0x254, 0x000000fe);
        break;
    default:
        break;
    }
}

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

    psp_dirty_core_dis_by_fuse(codename, dev);
}

static void create_ram(MemoryRegion *parent, const char *name, hwaddr offset, uint64_t size)
{
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ram, NULL, name, size, &error_fatal);
    memory_region_add_subregion(parent, offset, ram);
}

void psp_dirty_create_mp2_ram(MemoryRegion *smn, zen_codename codename)
{
    switch(codename) {
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        create_ram(smn, "mp2-ram", 0x3f56000, 0x2a000);
        break;
    default:
        break;
    }
}

void psp_dirty_create_smu_firmware(MemoryRegion *smn, zen_codename codename)
{
    create_ram(smn, "smu-fw", 0x3c00000, 0x40000);
}

void psp_dirty_create_pc_ram(MemoryRegion *ht, MemoryRegion *smn, zen_codename codename)
{
    switch(codename) {
    case CODENAME_RAVEN_RIDGE:
        create_ram(ht, "test-ram", 0xfffdf7000000, 0x400);
        create_ram(ht, "apob", 0x04000000, 0x10000);
        create_ram(smn, "apob2", 0x03f40000, 0x10000);
        create_ram(ht, "smu-fw-bis", 0xfffdfb800000, 0x40000);
        create_ram(ht, "smu-fw2-bis", 0xfffdfb840000, 0x40000);
        create_ram(ht, "bios", 0x09d80000, 0x280000);
        create_ram(ht, "bios-dir", 0x0a000000, 0x400);

        break;
    default:
        break;
    }
}
