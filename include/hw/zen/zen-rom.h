#ifndef ZEN_ROM_H
#define ZEN_ROM_H

#include "hw/zen/amdfwtool.h"
#include "hw/zen/zen-cpuid.h"

typedef struct {
    BlockBackend *blk;
    uint32_t id;
    zen_codename codename;

    uint32_t rom_base;

    embedded_firmware *efs;
    psp_combo_directory *psp_combo;
    psp_directory_table *psp_dir;
    psp_combo_directory *bios_combo;
    bios_directory_table *bios_dir;
} zen_rom_infos_t;

bool zen_rom_init(zen_rom_infos_t *infos, BlockBackend *blk, zen_codename codename);
void zen_rom_read(zen_rom_infos_t *infos, uint32_t offset, void *buf, int bytes);
bool zen_rom_get_psp_entry(zen_rom_infos_t *infos, amd_fw_type type,
                           uint32_t *entry_offset, uint32_t *entry_size);
bool zen_rom_get_bios_entry(zen_rom_infos_t *infos, amd_bios_type type,
                            uint64_t *entry_source, uint32_t *entry_size,
                            uint64_t *entry_dest);

#endif
