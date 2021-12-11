/* SPDX-License-Identifier: GPL-2.0-only */
/* Type definition from coreboot/util/amdfwtool/amdfwtool.h */

#ifndef PSP_LOADER_INTERNAL_H
#define PSP_LOADER_INTERNAL_H

typedef enum _amd_fw_type {
    AMD_FW_PSP_PUBKEY = 0,
    AMD_FW_PSP_BOOTLOADER = 1,
    AMD_FW_PSP_SMU_FIRMWARE = 8,
    AMD_FW_PSP_RECOVERY = 3,
    AMD_FW_PSP_RTM_PUBKEY = 5,
    AMD_FW_PSP_SECURED_OS = 2,
    AMD_FW_PSP_NVRAM = 4,
    AMD_FW_PSP_SECURED_DEBUG = 9,
    AMD_FW_PSP_TRUSTLETS = 12,
    AMD_FW_PSP_TRUSTLETKEY = 13,
    AMD_FW_PSP_SMU_FIRMWARE2 = 18,
    AMD_PSP_FUSE_CHAIN = 11,
    AMD_FW_PSP_SMUSCS = 95,
    AMD_DEBUG_UNLOCK = 0x13,
    AMD_HW_IPCFG = 0x20,
    AMD_WRAPPED_IKEK = 0x21,
    AMD_TOKEN_UNLOCK = 0x22,
    AMD_SEC_GASKET = 0x24,
    AMD_MP2_FW = 0x25,
    AMD_DRIVER_ENTRIES = 0x28,
    AMD_FW_KVM_IMAGE = 0x29,
    AMD_S0I3_DRIVER = 0x2d,
    AMD_ABL0 = 0x30,
    AMD_ABL1 = 0x31,
    AMD_ABL2 = 0x32,
    AMD_ABL3 = 0x33,
    AMD_ABL4 = 0x34,
    AMD_ABL5 = 0x35,
    AMD_ABL6 = 0x36,
    AMD_ABL7 = 0x37,
    AMD_FW_PSP_WHITELIST = 0x3a,
    AMD_VBIOS_BTLOADER = 0x3c,
    AMD_FW_L2_PTR = 0x40,
    AMD_FW_USB_PHY = 0x44,
    AMD_FW_TOS_SEC_POLICY = 0x45,
    AMD_FW_DRTM_TA = 0x47,
    AMD_FW_KEYDB_BL = 0x50,
    AMD_FW_KEYDB_TOS = 0x51,
    AMD_FW_PSP_VERSTAGE = 0x52,
    AMD_FW_VERSTAGE_SIG = 0x53,
    AMD_RPMC_NVRAM = 0x54,
    AMD_FW_DMCU_ERAM = 0x58,
    AMD_FW_DMCU_ISR = 0x59,
    AMD_FW_PSP_BOOTLOADER_AB = 0x73,
    AMD_FW_IMC = 0x200,    /* Large enough to be larger than the top BHD entry type. */
    AMD_FW_GEC,
    AMD_FW_XHCI,
    AMD_FW_INVALID,        /* Real last one to detect the last entry in table. */
    AMD_FW_SKIP        /* This is for non-applicable options. */
} amd_fw_type;


struct second_gen_efs { /* todo: expand for Server products */
    int gen:1; /* Client products only use bit 0 */
    int reserved:31;
} __attribute__((packed));

typedef struct _embedded_firmware {
    uint32_t signature; /* 0x55aa55aa */
    uint32_t imc_entry;
    uint32_t gec_entry;
    uint32_t xhci_entry;
    uint32_t psp_entry;
    uint32_t comboable;
    uint32_t bios0_entry; /* todo: add way to select correct entry */
    uint32_t bios1_entry;
    uint32_t bios2_entry;
    struct second_gen_efs efs_gen;
    uint32_t bios3_entry;
    uint32_t reserved_2Ch;
    uint32_t promontory_fw_ptr;
    uint32_t lp_promontory_fw_ptr;
    uint32_t reserved_38h;
    uint32_t reserved_3Ch;
    uint8_t spi_readmode_f15_mod_60_6f;
    uint8_t fast_speed_new_f15_mod_60_6f;
    uint8_t reserved_42h;
    uint8_t spi_readmode_f17_mod_00_2f;
    uint8_t spi_fastspeed_f17_mod_00_2f;
    uint8_t qpr_dummy_cycle_f17_mod_00_2f;
    uint8_t reserved_46h;
    uint8_t spi_readmode_f17_mod_30_3f;
    uint8_t spi_fastspeed_f17_mod_30_3f;
    uint8_t micron_detect_f17_mod_30_3f;
    uint8_t reserved_4Ah;
    uint8_t reserved_4Bh;
    uint32_t reserved_4Ch;
} __attribute__((packed, aligned(16))) embedded_firmware;

typedef struct _psp_directory_header {
    uint32_t cookie;
    uint32_t checksum;
    uint32_t num_entries;
    uint32_t additional_info;
} __attribute__((packed, aligned(16))) psp_directory_header;

typedef struct _psp_directory_entry {
    uint8_t type;
    uint8_t subprog;
    uint16_t rsvd;
    uint32_t size;
    uint64_t addr; /* or a value in some cases */
} __attribute__((packed)) psp_directory_entry;

typedef struct _psp_combo_header {
    uint32_t cookie;
    uint32_t checksum;
    uint32_t num_entries;
    uint32_t lookup;
    uint64_t reserved[2];
} __attribute__((packed, aligned(16))) psp_combo_header;

typedef struct _psp_combo_entry {
    uint32_t id_sel;
    uint32_t id;
    uint64_t lvl2_addr;
} __attribute__((packed)) psp_combo_entry;

#define EMBEDDED_FW_SIGNATURE     0x55aa55aa
#define PSP_COOKIE                 0x50535024        /* '$PSP' */
#define PSP_L2_COOKIE             0x324c5024        /* '$PL2' */
#define PSP_COMBO_COOKIE         0x50535032        /* '2PSP' */


typedef struct {
    uint8_t PhysDieId;
    uint8_t SocketId;
    uint8_t PackageType;
    uint8_t SystemSocketCount;
    uint8_t unknown0;
    uint8_t DiesPerSocket;
} PspBootRomServices_t;

#endif
