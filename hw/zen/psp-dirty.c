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
    CoreDisByFuse register for summit-ridge/raven-ridge:
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
    case CODENAME_MATISSE: ;
        /*
        22-29: CCD present (bitmap)
        30-37: CCD down (bitmap mask)
        */
        uint64_t value = 0;
        value |= 0xffULL << 22;
        value |= 0xfeULL << 30;

        psp_fuses_write32(dev, 0x218, value & 0xffffffff);
        psp_fuses_write32(dev, 0x21c, value >> 32);
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
    case CODENAME_MATISSE:
        /*
        Some kind of socket or die bitmap ?
        Bits 22-29 are used.
        */
        psp_fuses_write32(dev, 0x218, 0x00400000);
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
    create_ram(ht, "test-ram",      0xfffdf7000000, 0x10000);
    /* used for SMU, Tee, nvram... */
    create_ram(ht, "ram2",          0xfffdfb000000, 0x1000000);
    create_ram(ht, "bios",          0x000009d80000, 0x280000);
    create_ram(ht, "bios-dir",      0x00000a000000, 0x400);

    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        create_ram(ht, "apob",          0x000004000000, 0x10000);
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        create_ram(smn, "apob2",        0x03f40000, 0x10000);
        create_ram(ht, "apob",          0x000004000000, 0x10000);

        /*
        create_ram(ht, "drivers",       0xfffdfb000000, 0x40000);
        create_ram(ht, "secured-os",    0xfffdfb040000, 0x40000);
        create_ram(ht, "nvram",         0xfffdfb7b0000, 0x20000);
        create_ram(ht, "trustlets",     0xfffdfb7d0000, 0x20000);
        create_ram(ht, "smu-fw-bis",    0xfffdfb800000, 0x40000);
        create_ram(ht, "smu-fw2-bis",   0xfffdfb840000, 0x40000);
        */
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_ram(ht, "apob",          0x00000a200000, 0x20000);
        break;
    case CODENAME_CEZANNE:
        create_ram(ht, "apob",          0x00000a200000, 0x20000);
        break;
    default:
        break;
    }
}
