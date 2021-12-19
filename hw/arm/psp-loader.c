/*
 * PSP on-chip bootloader emulation.
 *
 * Copyright (C) 2021 Timothée Cocault <timothee.cocault@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "qemu/qemu-print.h"
#include "qemu/error-report.h"
#include "crypto/cipher.h"
#include "qapi/error.h"

#include "psp-loader-internal.h"
#include "hw/arm/psp-loader.h"

static void memory_write(hwaddr addr, void *data, size_t size)
{
    MemTxResult mem_res;
    mem_res = address_space_write(&address_space_memory, addr,
                                  MEMTXATTRS_UNSPECIFIED, data, size);
    assert(mem_res == MEMTX_OK);
}

static void write_config(void)
{
    uint8_t data[] = {
        1, /* cores per ccx */
        1, /* number of ccx */
    };
    memory_write(0x3fa1e, data, sizeof(data));

    PspBootRomServices_t infos = {
        .PhysDieId = 0,
        .SocketId = 0,
        .PackageType = 0,
        .SystemSocketCount = 1,
        .unknown0 = 0,
        .DiesPerSocket = 1,
    };
    memory_write(0x3fa50, &infos, sizeof(infos));
}

static void *map_get_pointer(GMappedFile *mapped_file, size_t offset,
                             size_t size)
{
    gchar *mapped_data = g_mapped_file_get_contents(mapped_file);
    gsize mapped_size = g_mapped_file_get_length(mapped_file);

    if (offset > mapped_size || offset + size > mapped_size) {
        return NULL;
    }

    return mapped_data + offset;
}

static uint32_t embedded_fw_offsets[] = {
    0xfa0000, 0xf20000, 0xe20000, 0xc20000, 0x820000, 0x020000
};

static embedded_firmware *get_embedded_fw(GMappedFile *mapped_file)
{
    for (int i = 0; i < ARRAY_SIZE(embedded_fw_offsets); i++) {
        embedded_firmware *p = (embedded_firmware *)map_get_pointer(mapped_file, embedded_fw_offsets[i], sizeof(embedded_firmware));
        if (p != NULL && p->signature == EMBEDDED_FW_SIGNATURE) {
            return p;
        }
    }
    return NULL;
}

static psp_combo_header *get_psp_combo(GMappedFile *mapped_file, uint32_t offset)
{
    psp_combo_header *header = map_get_pointer(mapped_file, offset, sizeof(psp_combo_header));

    if (header == NULL || header->cookie != PSP_COMBO_COOKIE) {
        return NULL;
    }

    return map_get_pointer(mapped_file, offset, sizeof(psp_combo_header) + header->num_entries * sizeof(psp_combo_entry));
}

static uint64_t psp_combo_get_entry(GMappedFile *mapped_file, psp_combo_header *combo, uint32_t id_sel, uint32_t id)
{
    psp_combo_entry *entries = (psp_combo_entry *)(combo + 1);

    for (int i = 0; i < combo->num_entries; i++) {
        if (entries[i].id_sel == id_sel && entries[i].id == id) {
            return entries[i].lvl2_addr;
        }
    }
    return 0;
}

static psp_directory_header *get_psp_dir(GMappedFile *mapped_file, uint64_t offset)
{
    psp_directory_header *header = map_get_pointer(mapped_file, offset,
                                                   sizeof(psp_directory_header));

    if (header == NULL) {
        return NULL;
    }

    if (header->cookie != PSP_COOKIE && header->cookie != PSP_L2_COOKIE) {
        return NULL;
    }

    return map_get_pointer(mapped_file, offset, sizeof(psp_directory_header) + header->num_entries * sizeof(psp_directory_entry));
}

static bool psp_dir_get_entry(GMappedFile *mapped_file,
                              psp_directory_header *dir, amd_fw_type type,
                              uint8_t subprog,
                              uint64_t *out_addr, uint32_t *out_size)
{
    psp_directory_entry *entries = (psp_directory_entry *)(dir + 1);

    for (int i = 0; i < dir->num_entries; i++) {
        if (entries[i].type == type && entries[i].subprog == subprog) {
            *out_addr = entries[i].addr;
            *out_size = entries[i].size;
            return true;
        }
    }
    return false;
}

static bool do_copy_psp_dir(GMappedFile *mapped_file, psp_directory_header *dir)
{
    size_t dir_size = sizeof(psp_directory_header) + dir->num_entries * sizeof(psp_directory_entry);
    memory_write(0x3f000, dir, dir_size);
    return true;
}

static bool copy_dir_entry(GMappedFile *mapped_file, hwaddr dst,
                           uint64_t offset, uint32_t size)
{
    void *data = map_get_pointer(mapped_file, offset, size);

    if (data == NULL) {
        return false;
    }

    memory_write(dst, data, size);
    return true;
}

/* Depends on the PSP version */
uint8_t hardcoded_key[] = {
    0x4c, 0x77, 0x63, 0x65, 0x32, 0xfe, 0x4c, 0x6f,
    0xd6, 0xb9, 0xd6, 0xd7, 0xb5, 0x1e, 0xde, 0x59
};

static bool copy_firmware(GMappedFile *mapped_file, hwaddr dst,
                          uint64_t offset, uint32_t size)
{
    QCryptoCipher *cipher;

    fw_hdr_t *hdr = map_get_pointer(mapped_file, offset, 0x100);

    if (hdr == NULL) {
        return false;
    }

    memory_write(dst, hdr, 0x100);

    size_t data_size = hdr->rom_size - 0x100;
    uint8_t *data = map_get_pointer(mapped_file, offset + 0x100, data_size);

    if (hdr->is_encrypted) {
        g_autofree uint8_t *dec = g_malloc(data_size);
        uint8_t unwrapped_key[16];


        {
            cipher = qcrypto_cipher_new(QCRYPTO_CIPHER_ALG_AES_128,
                                        QCRYPTO_CIPHER_MODE_ECB,
                                        hardcoded_key, 16, &error_fatal);
            if (!cipher) {
                return false;
            }
            if (qcrypto_cipher_decrypt(cipher, hdr->wrapped_key, unwrapped_key,
                                       16, &error_fatal) < 0) {
                return false;
            }
            qcrypto_cipher_free(cipher);
        }

        {
            cipher = qcrypto_cipher_new(QCRYPTO_CIPHER_ALG_AES_128,
                                        QCRYPTO_CIPHER_MODE_CBC,
                                        unwrapped_key, 16, &error_fatal);
            if (!cipher) {
                return false;
            }
            if (qcrypto_cipher_setiv(cipher, hdr->iv, 16, &error_fatal) < 0) {
                return false;
            }
            if (qcrypto_cipher_decrypt(cipher, data, dec, data_size,
                                       &error_fatal) < 0) {
                return false;
            }
            qcrypto_cipher_free(cipher);
        }

        memory_write(dst+0x100, dec, data_size);
    } else {
        memory_write(dst+0x100, data, data_size);
    }

    return true;
}

static bool psp_copy_bootloader(GMappedFile *mapped_file,
                                psp_directory_header *dir)
{
    bool res;
    uint64_t bl_addr = 0;
    uint32_t bl_size = 0;

    res = psp_dir_get_entry(mapped_file, dir, AMD_FW_PSP_BOOTLOADER, 0,
                            &bl_addr, &bl_size);
    if (res == false) {
        error_report("Could not get PSP bootloader");
        return false;
    }

    res = copy_firmware(mapped_file, 0, bl_addr & 0x00ffffff, bl_size);
    if (res == false) {
        error_report("Could not copy PSP bootloader");
        return false;
    }

    return true;
}

static bool psp_copy_pubkey(GMappedFile *mapped_file, psp_directory_header *dir)
{
    bool res;
    uint64_t key_addr = 0;
    uint32_t key_size = 0;

    res = psp_dir_get_entry(mapped_file, dir, AMD_FW_PSP_PUBKEY, 0,
                            &key_addr, &key_size);
    if (res == false) {
        error_report("Could not get PSP pubkey");
        return false;
    }

    res = copy_dir_entry(mapped_file, 0x3f410, key_addr & 0x00ffffff, key_size);
    if (res == false) {
        error_report("Could not copy PSP pubkey");
        return false;
    }

    return true;
}

static bool psp_load_common(GMappedFile *mapped_file, uint32_t target_id,
                            psp_directory_header **out_dir)
{
    bool res;
    embedded_firmware *romsig = get_embedded_fw(mapped_file);
    if (romsig == NULL) {
        error_report("Could not find ROM signature");
        return false;
    }

    psp_combo_header *combo = get_psp_combo(mapped_file,
                                            romsig->comboable & 0x00ffffff);
    if (combo == NULL) {
        error_report("Could not get PSP combo");
        return false;
    }

    uint64_t psp_dir_addr = psp_combo_get_entry(mapped_file, combo, 0,
                                                target_id);

    psp_directory_header *dir = get_psp_dir(mapped_file,
                                            psp_dir_addr & 0x00ffffff);
    if (dir == NULL) {
        error_report("Could not get PSP directory");
        return false;
    }

    res = do_copy_psp_dir(mapped_file, dir);
    if (res == false) {
        error_report("Could not copy PSP directory");
        return false;
    }

    res = psp_copy_pubkey(mapped_file, dir);
    if (res == false) {
        return false;
    }

    write_config();

    *out_dir = dir;

    return true;
}

static bool psp_copy_secured_os(GMappedFile *mapped_file,
                                psp_directory_header *dir)
{
    bool res;
    uint64_t sec_os_addr = 0;
    uint32_t sec_os_size = 0;

    res = psp_dir_get_entry(mapped_file, dir, AMD_FW_PSP_SECURED_OS, 0,
                            &sec_os_addr, &sec_os_size);
    if (res == false) {
        error_report("Could not get PSP secured OS");
        return false;
    }

    res = copy_dir_entry(mapped_file, 0, (sec_os_addr & 0x00ffffff) + 0x100,
                         sec_os_size - 0x100);
    if (res == false) {
        error_report("Could not copy PSP secured OS");
        return false;
    }

    return true;
}

static bool psp_copy_driver_entries(GMappedFile *mapped_file,
                                    psp_directory_header *dir)
{
    bool res;
    uint64_t drv_addr = 0;
    uint32_t drv_size = 0;

    res = psp_dir_get_entry(mapped_file, dir, AMD_DRIVER_ENTRIES, 0,
                            &drv_addr, &drv_size);
    if (res == false) {
        error_report("Could not get PSP driver entries");
        return false;
    }

    res = copy_dir_entry(mapped_file, 0x04000000,
                         drv_addr & 0x00ffffff, drv_size);
    if (res == false) {
        error_report("Could not copy PSP driver entries");
        return false;
    }

    return true;
}

static bool psp_load_normal(GMappedFile *mapped_file, uint32_t target_id)
{
    bool res;
    psp_directory_header *dir = NULL;

    res = psp_load_common(mapped_file, target_id, &dir);
    if (res == false) {
        return false;
    }

    res = psp_copy_bootloader(mapped_file, dir);
    if (res == false) {
        return false;
    }

    return true;
}

static bool psp_load_secured_os(GMappedFile *mapped_file, uint32_t target_id)
{
    bool res;
    psp_directory_header *dir = NULL;

    res = psp_load_common(mapped_file, target_id, &dir);
    if (res == false) {
        return false;
    }

    uint64_t l2_dir_addr = 0;
    uint32_t l2_dir_size = 0;

    res = psp_dir_get_entry(mapped_file, dir, AMD_FW_L2_PTR, 0,
                            &l2_dir_addr, &l2_dir_size);
    if (res == false) {
        error_report("Could not get PSP L2 directory");
        return false;
    }

    psp_directory_header *l2_dir = get_psp_dir(mapped_file,
                                               l2_dir_addr & 0x00ffffff);

    res = psp_copy_secured_os(mapped_file, l2_dir);
    if (res == false) {
        return false;
    }

    res = psp_copy_driver_entries(mapped_file, l2_dir);
    if (res == false) {
        return false;
    }

    return true;
}

bool psp_load(const char *filename, psp_load_type_e load_type,
              uint32_t target_id)
{
    int fd;
    GMappedFile *mapped_file;
    bool res;

    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0) {
        error_report("Could not open ROM file");
        return false;
    }

    mapped_file = g_mapped_file_new_from_fd(fd, true, NULL);
    if (!mapped_file) {
        error_report("Could not map ROM file");
        close(fd);
        return false;
    }

    switch (load_type) {
    case PSP_LOAD_BOOTLOADER:
        res = psp_load_normal(mapped_file, target_id);
        break;
    case PSP_LOAD_SECURED_OS:
        res = psp_load_secured_os(mapped_file, target_id);
        break;
    default:
        g_assert_not_reached();
    }

    g_mapped_file_unref(mapped_file);
    close(fd);
    return res;
}
