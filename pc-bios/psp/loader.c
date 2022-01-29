#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "loader-internal.h"


const rom_info_t rom_info = {
    .x86_addr = 0xff000000,
    .smn_addr = 0x0a000000,
    .size     = 0x01000000,
};

const psp_info_t psp_info = {
    .psp_version        = 0xbc0a0000,
    .directory_addr     = (void *)0x3f000,
    .pubkey_addr        = (void *)0x3f410,
    .core_info_addr     = (void *)0x3fa1e,
    .rom_service_addr   = (void *)0x3fa50,
};

#define SMN_IO_BASE         0x03220000 // Base of the SMN registers in ARM address space
#define SMN_MAPPING_BASE    0x01000000 // Base of the mapped SMN regions in ARM address space

#define SMN_MAPPING_BITS 0x14
#define SMN_MAPPING_SIZE (1 << SMN_MAPPING_BITS)
#define SMN_NUM_MAPPINGS 0x20

#define EMBEDDED_FW_OFFSET  0x020000

void map_spi()
{
    uint16_t *smn_slots = (uint16_t *)SMN_IO_BASE;

    for(int i = 0; i < 16; i++)
        smn_slots[i] = (rom_info.smn_addr + i * SMN_MAPPING_SIZE) >> SMN_MAPPING_BITS;
}

void unmap_spi()
{
    uint16_t *smn_slots = (uint16_t *)SMN_IO_BASE;

    for(int i = 0; i < 16; i++)
        smn_slots[i] = 0;
}

void *translate_ptr(uint32_t addr)
{
    if(addr == 0)
        return NULL;
    else if(addr >> 24 == 0)
        return (void *)(addr + SMN_MAPPING_BASE);
    else
        return (void *)(addr - rom_info.x86_addr + SMN_MAPPING_BASE);
}

int psp_find_entry(psp_directory *dir, amd_fw_type type, void **out_addr, uint32_t *out_size)
{
    psp_directory_entry *entry = dir->entries;
    for(int i = 0; i < dir->header.num_entries; i++)
    {
        if(entry->type == type) {
            *out_addr = translate_ptr(entry->addr);
            *out_size = entry->size;
            return 1;
        }
        entry++;
    }
    return 0;
}

int combo_find_entry(psp_combo *dir, uint32_t id, void **out_addr)
{
    psp_combo_entry *entry = dir->entries;
    for(int i = 0; i < dir->header.num_entries; i++)
    {
        if(entry->id_sel == 0 && entry->id == id) {
            *out_addr = translate_ptr(entry->lvl2_addr);
            return 1;
        }
        entry++;
    }
    return 0;
}

embedded_firmware *get_embedded_firmware()
{
    embedded_firmware *fw = (embedded_firmware *)(SMN_MAPPING_BASE + EMBEDDED_FW_OFFSET);
    assert(fw->signature == 0x55aa55aa);
    return fw;
}

psp_directory *combo_get_dir(psp_combo *combo, uint32_t version)
{
    for(int i = 0; i < combo->header.num_entries; i++) {
        psp_combo_entry *entry = &combo->entries[i];
        if(entry->id_sel == 0 && entry->id == version)
            return translate_ptr(entry->lvl2_addr);
    }
    return NULL;
}

psp_directory *get_psp_directory(embedded_firmware *fw, uint32_t version)
{
    void *comboable = translate_ptr(fw->comboable);
    assert(comboable);

    // TODO: also check for non combo
    assert(*(uint32_t *)comboable == PSP_COMBO_COOKIE);

    psp_directory *res = combo_get_dir((psp_combo *)comboable, version);
    // TODO: check for correct AMD key

    // TODO: also check for fw->psp_dir
    return res;
}

void _start()
{
    map_spi();

    embedded_firmware *fw = get_embedded_firmware();
    assert(fw);

    psp_directory *dir = get_psp_directory(fw, psp_info.psp_version);
    assert(dir);

    // Copy the directory
    memcpy(psp_info.directory_addr, dir, sizeof(psp_directory_header) + dir->header.num_entries * sizeof(psp_directory_entry));

    // Copy the pubkey
    void *pubkey_addr = NULL;
    uint32_t pubkey_size = 0;
    psp_find_entry(dir, AMD_FW_PSP_PUBKEY, &pubkey_addr, &pubkey_size);
    memcpy(psp_info.pubkey_addr, pubkey_addr, pubkey_size);

    // Copy the bootloader
    void *bootloader_addr = NULL;
    uint32_t bootloader_size = 0;
    psp_find_entry(dir, AMD_FW_PSP_BOOTLOADER, &bootloader_addr, &bootloader_size);
    fw_hdr_t *fw_hdr = bootloader_addr;
    void (*bootloader)() = (void (*)())(fw_hdr->load_addr);
    memcpy(bootloader, fw_hdr + 1, fw_hdr->size_signed);

    // Write config
    PspBootRomServices_t *infos = (PspBootRomServices_t *)psp_info.rom_service_addr;
    infos->SystemSocketCount = 1;
    infos->DiesPerSocket = 1;
    infos->PackageType = 1;

    X86CoresInfo_t *core_infos = (X86CoresInfo_t *)psp_info.core_info_addr;
    core_infos->cores_per_ccx = 1;
    core_infos->number_of_ccx = 1;

    unmap_spi();
    bootloader();
}
