#include <stdint.h>
#include <stddef.h>
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

void *memcpy(void *dst, void *src, size_t n)
{
    uint8_t *d = dst;
    uint8_t *s = src;
    while(n) {
        *d++ = *s++;
        n--;
    }
}

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

void _start()
{
    map_spi();

    embedded_firmware *fw = (embedded_firmware *)(SMN_MAPPING_BASE + EMBEDDED_FW_OFFSET);

    psp_combo *combo_dir = translate_ptr(fw->comboable);

    psp_directory *psp_dir = NULL;
    combo_find_entry(combo_dir, psp_info.psp_version, (void **)&psp_dir);

    // Copy the directory
    memcpy(psp_info.directory_addr, psp_dir, sizeof(psp_directory_header) + psp_dir->header.num_entries * sizeof(psp_directory_entry));

    // Copy the pubkey
    void *pubkey_addr = NULL;
    uint32_t pubkey_size = 0;
    psp_find_entry(psp_dir, AMD_FW_PSP_PUBKEY, &pubkey_addr, &pubkey_size);
    memcpy(psp_info.pubkey_addr, pubkey_addr, pubkey_size);

    // Copy the bootloader
    void *bootloader_addr = NULL;
    uint32_t bootloader_size = 0;
    psp_find_entry(psp_dir, AMD_FW_PSP_BOOTLOADER, &bootloader_addr, &bootloader_size);
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
