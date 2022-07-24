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

static void copy_psp_dir(AddressSpace *as, zen_rom_infos_t *infos, zen_codename codename)
{
    uint32_t psp_dir_addr;
    psp_directory_table *psp_dir = infos->psp_dir;
    size_t psp_dir_size = sizeof(psp_directory_header) +
                          sizeof(psp_directory_entry) * psp_dir->header.num_entries;


    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        psp_dir_addr = 0x3f000;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        psp_dir_addr = 0x4f000;
        break;
    default:
        g_assert_not_reached();
    }

    address_space_write(as, psp_dir_addr, MEMTXATTRS_UNSPECIFIED, psp_dir, psp_dir_size);
}

static void setup_bootloader(AddressSpace *as, zen_rom_infos_t *infos)
{
    bool ret;
    uint32_t bl_offset, bl_size;

    ret = zen_rom_get_psp_entry(infos, AMD_FW_PSP_BOOTLOADER, &bl_offset, &bl_size);
    assert(ret);

    void *buf = g_malloc(bl_size);
    zen_rom_read(infos, bl_offset, buf, bl_size);
    address_space_write(as, 0, MEMTXATTRS_UNSPECIFIED, buf, bl_size);

    cpu_set_pc(first_cpu, 0x100);
}

void psp_on_chip_bootloader(AddressSpace *as, BlockBackend *blk, zen_codename codename)
{
    bool ret;

    zen_rom_infos_t infos;
    ret = zen_rom_init(&infos, blk, codename);
    assert(ret);

    copy_psp_dir(as, &infos, codename);
    setup_bootloader(as, &infos);
    psp_create_config(as, codename);
}
