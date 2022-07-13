#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/zen-rom.h"
#include "cpu.h"
#include "hw/boards.h"
#include "hw/zen/psp-ocbl.h"
#include "exec/address-spaces.h"
#include "sysemu/block-backend.h"

void psp_on_chip_bootloader(BlockBackend *blk, zen_codename codename)
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

    address_space_write(&address_space_memory, 0, MEMTXATTRS_UNSPECIFIED, buf, bl_size);

    // TODO: temporary
    monitor_remove_blk(blk);

    cpu_set_pc(first_cpu, 0x100);
}
