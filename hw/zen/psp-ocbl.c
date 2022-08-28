#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/zen-rom.h"
#include "cpu.h"
#include "hw/boards.h"
#include "hw/zen/psp-ocbl.h"
#include "exec/address-spaces.h"
#include "sysemu/block-backend.h"


typedef struct __attribute__((__packed__)) {
    uint8_t PhysDieId;
    uint8_t SocketId;
    uint8_t PackageType;
    uint8_t SystemSocketCount;
    uint8_t unknown0;
    uint8_t DiesPerSocket;
} mcm_info_t;

typedef struct __attribute__((__packed__))
{
    uint8_t _pad0[0x14];
    uint32_t SecurityState;
    uint8_t _pad1[0x6];
    uint8_t PhysicalCoreCount;
    uint8_t PhysicalCoreComplexCount;
    uint8_t EnabledCoreCountOnDie;
    uint8_t _pad2[0x1];
    uint8_t LogicalCoresPerPhysicalComplex[2];

    /*
    Indexed by logical core (< EnabledCoreCountOnDie).
    Unused entries are (0xff, 0xff).
    */
    struct {
        uint8_t CoreComplex;
        uint8_t Core;
    } LogicalToPhysical[8];

    /*
    Indexed by (PhysComplex, PhysCore).
    Unused entries are (0xff, 0xff).
    */
    struct {
        uint8_t LogicalComplex;
        uint8_t LogicalCore;
    } PhysicalToLogical[2][4];

    uint8_t padding[0xc];
    mcm_info_t McmInfos;
} early_config_t;

static void create_config(AddressSpace *as, uint32_t addr)
{
    early_config_t conf = {
        .SecurityState = 0, // TODO
        .PhysicalCoreCount = 4,
        .PhysicalCoreComplexCount = 2,
        .EnabledCoreCountOnDie = 1,
        .LogicalCoresPerPhysicalComplex = {
            4, 4,
        },
        .LogicalToPhysical = {
            {.CoreComplex = 0x00, .Core = 0x00},
            {.CoreComplex = 0xff, .Core = 0xff},
            {.CoreComplex = 0xff, .Core = 0xff},
            {.CoreComplex = 0xff, .Core = 0xff},
            {.CoreComplex = 0xff, .Core = 0xff},
            {.CoreComplex = 0xff, .Core = 0xff},
            {.CoreComplex = 0xff, .Core = 0xff},
            {.CoreComplex = 0xff, .Core = 0xff},
        },
        .PhysicalToLogical = {
            {
                {.LogicalComplex = 0x00, .LogicalCore = 0x00},
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
            }, {
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
                {.LogicalComplex = 0xff, .LogicalCore = 0xff},
            },
        },
        .McmInfos = {
            .PhysDieId = 0,
            .SocketId = 0,
            .PackageType = 2,
            .SystemSocketCount = 1,
            .DiesPerSocket = 1,
        }
    };
    address_space_write(as, addr, MEMTXATTRS_UNSPECIFIED, &conf, sizeof(conf));
}

static void create_config_lucienne_renoir(AddressSpace *as, uint32_t addr)
{
    // 0x4fd54:4 -> default SPI rom
    // 0x4fd5d:1 -> socket id
    // 0x4fd5f:1 -> socket Count

    // SocketId
    address_space_stb(as, addr + 0x5d, 0, MEMTXATTRS_UNSPECIFIED, NULL);
    // SocketCount
    address_space_stb(as, addr + 0x5f, 1, MEMTXATTRS_UNSPECIFIED, NULL);
}

static void create_config_cezanne(AddressSpace *as, uint32_t addr)
{
    // 0x4fd50:4 -> default SPI rom
    // 0x4fd59:1 -> socket id
    // 0x4fd5b:1 -> socket Count

    // SocketId
    address_space_stb(as, addr + 0x59, 0, MEMTXATTRS_UNSPECIFIED, NULL);
    // SocketCount
    address_space_stb(as, addr + 0x5b, 1, MEMTXATTRS_UNSPECIFIED, NULL);
}

static uint32_t get_psp_dir_addr(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        return 0x3f000;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        return 0x4f000;
    default:
        g_assert_not_reached();
    }
}

static void psp_create_config(AddressSpace *as, zen_codename codename)
{
    uint32_t psp_dir_addr = get_psp_dir_addr(codename);

    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        create_config(as, psp_dir_addr + 0xa00);
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_config(as, psp_dir_addr + 0xd00);
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
        create_config_lucienne_renoir(as, psp_dir_addr + 0xd00);
        break;
    case CODENAME_CEZANNE:
        create_config_cezanne(as, psp_dir_addr + 0xd00);
        break;
    default:
        g_assert_not_reached();
    }
}

static void copy_psp_dir(AddressSpace *as, zen_rom_infos_t *infos, zen_codename codename)
{
    psp_directory_table *psp_dir = infos->psp_dir;
    size_t psp_dir_size = sizeof(psp_directory_header) +
                          sizeof(psp_directory_entry) * psp_dir->header.num_entries;

    address_space_write(as, get_psp_dir_addr(codename), MEMTXATTRS_UNSPECIFIED,
                        psp_dir, psp_dir_size);
}

static void copy_pubkey(AddressSpace *as, zen_rom_infos_t *infos, zen_codename codename)
{
    bool ret;
    uint32_t key_addr, key_size;

    ret = zen_rom_get_psp_entry(infos, AMD_FW_PSP_PUBKEY, &key_addr, &key_size);
    assert(ret);

    void *buf = g_malloc(key_size);
    zen_rom_read(infos, key_addr, buf, key_size);
    address_space_write(as, get_psp_dir_addr(codename) + 0x410, MEMTXATTRS_UNSPECIFIED, buf, key_size);
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
    copy_pubkey(as, &infos, codename);
    setup_bootloader(as, &infos);
    psp_create_config(as, codename);
}
