#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/zen-rom.h"
#include "cpu.h"
#include "hw/boards.h"
#include "hw/zen/psp-ocbl.h"
#include "exec/address-spaces.h"
#include "sysemu/block-backend.h"

/*
typedef struct {
    uint8_t PhysDieId;
    uint8_t SocketId;
    uint8_t PackageType;
    uint8_t SystemSocketCount;
    uint8_t unknown0;
    uint8_t DiesPerSocket;
} PspBootRomServices_t;
*/

static void create_config(AddressSpace *as, uint32_t addr)
{
    // SystemSocketCount
    address_space_stb(as, addr + 3, 1, MEMTXATTRS_UNSPECIFIED, NULL);
    // DiesPerSocket
    address_space_stb(as, addr + 5, 1, MEMTXATTRS_UNSPECIFIED, NULL);
}

static void psp_create_config(AddressSpace *as, zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        create_config(as, 0x3fa50);
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_config(as, 0x4fd50);
        break;
    default:
        break;
    }
}

void psp_on_chip_bootloader(AddressSpace *as, BlockBackend *blk, zen_codename codename)
{
    bool ret;
    uint32_t bl_offset, bl_size;

    zen_rom_infos_t infos;
    ret = zen_rom_find_embedded_fw(&infos, blk, codename);
    assert(ret);

    ret = zen_rom_get_psp_entry(&infos, AMD_FW_PSP_BOOTLOADER, &bl_offset, &bl_size);
    assert(ret);

    void *buf = g_malloc(bl_size);
    zen_rom_read(&infos, bl_offset, buf, bl_size);

    address_space_write(as, 0, MEMTXATTRS_UNSPECIFIED, buf, bl_size);

    // TODO: temporary
    monitor_remove_blk(blk);

    cpu_set_pc(first_cpu, 0x100);

    psp_create_config(as, codename);
}
