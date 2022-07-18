#include "qemu/osdep.h"
#include "sysemu/block-backend.h"
#include "hw/zen/zen-rom.h"
#include "hw/zen/zen-cpuid.h"

#define BHD2_COOKIE 0x44484232		/* 'DHB2' */

uint32_t rom_offsets[] = {
    0x00fa0000,
    0x00f20000,
    0x00e20000,
    0x00c20000,
    0x00820000,
    0x00020000,
};

static uint32_t get_signature(zen_rom_infos_t *infos, uint32_t offset)
{
    uint32_t signature;

    zen_rom_read(infos, offset, &signature, sizeof(signature));
    return signature;
}

/* embedded_firmware */

static embedded_firmware *efs_read(zen_rom_infos_t *infos, uint32_t offset)
{
    embedded_firmware *efs = g_malloc(sizeof(*efs));
    zen_rom_read(infos, offset, efs, sizeof(*efs));
    assert(efs->signature == EMBEDDED_FW_SIGNATURE);
    return efs;
}

/* psp_directory_table */

static psp_directory_table *psp_dir_read(zen_rom_infos_t *infos, uint32_t offset)
{
    psp_directory_header hdr;
    gsize table_size;
    psp_directory_table *table;

    zen_rom_read(infos, offset, &hdr, sizeof(hdr));
    assert(hdr.cookie == PSP_COOKIE || hdr.cookie == PSPL2_COOKIE);

    table_size = sizeof(psp_directory_header) + hdr.num_entries * sizeof(psp_directory_entry);
    table = g_malloc(table_size);
    zen_rom_read(infos, offset, table, table_size);

    return table;
}

static bool psp_dir_get_entry(psp_directory_table *table, amd_fw_type type,
                              uint32_t *entry_offset, uint32_t *entry_size)
{
    psp_directory_entry *entry = &table->entries[0];

    for(int i = 0; i < table->header.num_entries; i++) {
        if(entry->type == type) {
            *entry_offset = entry->addr;
            *entry_size = entry->size;
            return true;
        }
        entry++;
    }
    return false;
}

/* psp_combo_directory */

static psp_combo_directory *psp_combo_read(zen_rom_infos_t *infos, uint32_t offset)
{
    psp_combo_header hdr;
    gsize table_size;
    psp_combo_directory *table;

    zen_rom_read(infos, offset, &hdr, sizeof(hdr));
    assert(hdr.cookie == PSP2_COOKIE || hdr.cookie == BHD2_COOKIE);

    table_size = sizeof(psp_combo_header) + hdr.num_entries * sizeof(psp_combo_entry);
    table = g_malloc(table_size);
    zen_rom_read(infos, offset, table, table_size);

    return table;
}

static bool psp_combo_get_entry(psp_combo_directory *table, uint32_t id_sel, uint32_t id, uint32_t *addr)
{
    psp_combo_entry *entry = &table->entries[0];

    for(int i = 0; i < table->header.num_entries; i++) {
        if(entry->id == id && entry->id_sel == id_sel) {
            *addr = entry->lvl2_addr;
            return true;
        }
        entry++;
    }
    return false;
}

/* bios_dir */

static bios_directory_table *bios_dir_read(zen_rom_infos_t *infos, uint32_t offset)
{
    bios_directory_hdr hdr;
    gsize table_size;
    bios_directory_table *table;

    zen_rom_read(infos, offset, &hdr, sizeof(hdr));
    assert(hdr.cookie == BHD_COOKIE || hdr.cookie == BHDL2_COOKIE);

    table_size = sizeof(bios_directory_hdr) + hdr.num_entries * sizeof(bios_directory_entry);
    table = g_malloc(table_size);
    zen_rom_read(infos, offset, table, table_size);

    return table;
}

static bool bios_dir_get_entry(bios_directory_table *table, amd_bios_type type,
                               uint64_t *entry_source, uint32_t *entry_size,
                               uint64_t *entry_dest)
{
    bios_directory_entry *entry = &table->entries[0];

    for(int i = 0; i < table->header.num_entries; i++) {
        if(entry->type == type) {
            *entry_source = entry->source;
            *entry_size = entry->size;
            *entry_dest = entry->dest;
            return true;
        }
        entry++;
    }
    return false;
}

/* other */

void zen_rom_read(zen_rom_infos_t *infos, uint32_t offset, void *buf, int bytes)
{
    int ret = blk_pread(infos->blk, infos->rom_base + (offset & 0x00ffffff), buf, bytes);
    assert(ret >= 0);
}

bool zen_rom_get_psp_entry(zen_rom_infos_t *infos, amd_fw_type type,
                           uint32_t *entry_offset, uint32_t *entry_size)
{
    return psp_dir_get_entry(infos->psp_dir, type, entry_offset, entry_size);
}

bool zen_rom_get_bios_entry(zen_rom_infos_t *infos, amd_bios_type type,
                            uint64_t *entry_source, uint32_t *entry_size,
                            uint64_t *entry_dest)
{
    return bios_dir_get_entry(infos->bios_dir, type, entry_source, entry_size, entry_dest);
}

static bool find_psp_infos(zen_rom_infos_t *infos)
{
    uint32_t psp_dir_addr;

    infos->psp_combo = psp_combo_read(infos, infos->efs->combo_psp_directory);

    if(psp_combo_get_entry(infos->psp_combo, 0, infos->id, &psp_dir_addr)) {
        infos->psp_dir = psp_dir_read(infos, psp_dir_addr);
        return true;
    }

    return false;
}

static bool find_bios_infos(zen_rom_infos_t *infos)
{
    uint32_t bios_dir_addr;

    switch(infos->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        bios_dir_addr = infos->efs->bios0_entry;
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        bios_dir_addr = infos->efs->bios1_entry;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        bios_dir_addr = infos->efs->bios2_entry;
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        bios_dir_addr = infos->efs->bios3_entry;
        break;
    default:
        g_assert_not_reached();
    }

    uint32_t signature = get_signature(infos, bios_dir_addr);
    uint64_t l2_source = 0;
    uint32_t l2_size;
    uint64_t l2_dest;

    switch(signature) {
    case BHD_COOKIE:
        infos->bios_dir = bios_dir_read(infos, bios_dir_addr);
        break;
    case BHD2_COOKIE:
        infos->bios_combo = psp_combo_read(infos, bios_dir_addr);
        psp_combo_get_entry(infos->bios_combo, 0, infos->id, &bios_dir_addr);
        infos->bios_dir = bios_dir_read(infos, bios_dir_addr);
        /* Meh. We should merge the L2 dir */
        bios_dir_get_entry(infos->bios_dir, AMD_BIOS_L2_PTR, &l2_source, &l2_size, &l2_dest);
        infos->bios_dir = bios_dir_read(infos, l2_source);
        break;
    default:
        g_assert_not_reached();
    }

    return true;
}

bool zen_rom_init(zen_rom_infos_t *infos, BlockBackend *blk, zen_codename codename)
{
    uint64_t size;
    uint64_t rom_base;
    int i;

    infos->blk = blk;
    infos->codename = codename;
    infos->id = zen_get_bootrom_revid(codename);

    size = blk_getlength(infos->blk);
    assert(size == 0x2000000 || size == 0x1000000);

    for(rom_base = 0; rom_base < size; rom_base += 0x1000000) {
        infos->rom_base = rom_base;

        for(i = 0; i < ARRAY_SIZE(rom_offsets); i++) {
            if(get_signature(infos, rom_offsets[i]) == EMBEDDED_FW_SIGNATURE) {
                infos->efs = efs_read(infos, rom_offsets[i]);

                if(find_psp_infos(infos)) {
                    assert(find_bios_infos(infos));
                    return true;
                }
            }
        }
    }

    return false;
}
