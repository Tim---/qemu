#include "qemu/osdep.h"
#include "sysemu/block-backend.h"
#include "hw/zen/zen-rom.h"

uint32_t rom_offsets[] = {
    0x00fa0000,
    0x00f20000,
    0x00e20000,
    0x00c20000,
    0x00820000,
    0x00020000,
};

static uint32_t get_selector(uint32_t cpuid)
{
    uint32_t selector;

    switch(cpuid & 0x0fff0f00) {
    case 0x00800f00: /* Summit Ridge, Pinnacle Ridge */
        selector = 0xbc090000;
        break;
    case 0x00810f00: /* Raven Ridge, Picasso */
        selector = 0xbc0a0000;
        break;
    case 0x00870f00: /* Matisse */
    case 0x00a20f00: /* Vermeer */
        selector = 0xbc0b0500;
        break;
    case 0x00860f00: /* Lucienne, Renoir */
        selector = 0xbc0c0000;
        break;
    case 0x00a50f00: /* Cezanne */
        selector = 0xbc0c0140;
        break;
    default:
        g_assert_not_reached();
    }

    return selector;
}
void zen_rom_read(zen_rom_infos_t *infos, uint32_t offset, void *buf, int bytes)
{
    int ret = blk_pread(infos->blk, infos->rom_base + (offset & 0x00ffffff), buf, bytes);
    assert(ret >= 0);
}

bool zen_rom_get_psp_entry(zen_rom_infos_t *infos, amd_fw_type type, uint32_t *entry_offset, uint32_t *entry_size)
{
    psp_directory_header hdr;
    psp_directory_entry entry;
    int i;
    uint32_t offset = infos->psp_dir_offset;

    zen_rom_read(infos, infos->psp_dir_offset, &hdr, sizeof(hdr));

    assert(hdr.cookie == PSP_COOKIE);
    offset += sizeof(hdr);

    for(i = 0; i < hdr.num_entries; i++) {
        zen_rom_read(infos, offset, &entry, sizeof(entry));

        if(entry.type == type) {
            *entry_offset = entry.addr;
            *entry_size = entry.size;
            return true;
        }

        offset += sizeof(entry);
    }
    return false;
}

static bool zen_rom_parse_psp_combo(zen_rom_infos_t *infos, uint32_t offset)
{
    psp_combo_header hdr;
    psp_combo_entry entry;
    int i;

    zen_rom_read(infos, offset, &hdr, sizeof(hdr));

    assert(hdr.cookie == PSP2_COOKIE);
    offset += sizeof(hdr);

    for(i = 0; i < hdr.num_entries; i++) {
        zen_rom_read(infos, offset, &entry, sizeof(entry));

        if(entry.id_sel == 0 && entry.id == infos->id) {
            infos->psp_dir_offset = entry.lvl2_addr;
            return true;
        }

        offset += sizeof(entry);
    }
    return false;
}

bool zen_rom_find_embedded_fw(zen_rom_infos_t *infos, BlockBackend *blk, uint32_t cpuid)
{
    uint64_t size;
    uint64_t rom_base;
    int i;
    embedded_firmware efs;

    infos->blk = blk;
    infos->id = get_selector(cpuid);

    size = blk_getlength(infos->blk);
    assert(size == 0x2000000 || size == 0x1000000);

    for(rom_base = 0; rom_base < size; rom_base += 0x1000000) {
        infos->rom_base = rom_base;

        for(i = 0; i < ARRAY_SIZE(rom_offsets); i++) {
            zen_rom_read(infos, rom_offsets[i], &efs, sizeof(efs));

            if(efs.signature == EMBEDDED_FW_SIGNATURE) {
                infos->emb_fw_offset = rom_offsets[i];
                if(zen_rom_parse_psp_combo(infos, efs.combo_psp_directory)) {
                    return true;
                }
            }
        }
    }

    return false;
}
