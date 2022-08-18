#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/zen/apob.h"
#include "hw/zen/zen-cpuid.h"

typedef struct {
    uint32_t group;
    uint32_t type;
    uint32_t instance;
    uint32_t size;
    uint8_t hmac[0x20];
} apob_item_header_t;

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t size;
    uint32_t offset_of_first_entry;
} apob_header_t;

/* raven-ridge specific */

typedef struct {
    struct {
        uint8_t num;
        struct {
            uint8_t num;
            struct {
                uint8_t enabled;
            } threads[2];
        } cores[4];
    } complexes[2];
} raven_ridge_topology_t;

static void apob_create_header(hwaddr apob_addr)
{
    apob_header_t header = {
        .signature = 0x424f5041,
        .version = 5,
        .size = 0x10,
        .offset_of_first_entry = 0x10,

    };
    address_space_write(&address_space_memory, apob_addr, MEMTXATTRS_UNSPECIFIED, &header, sizeof(header));
}

static void apob_add_entry(hwaddr apob_addr, uint32_t group, uint32_t type, uint32_t instance, void *buf, uint32_t size)
{
    apob_header_t apob_header;
    address_space_read(&address_space_memory, apob_addr, MEMTXATTRS_UNSPECIFIED, &apob_header, sizeof(apob_header));

    apob_item_header_t item_header = {
        .group = 3,
        .type = 1,
        .instance = 0,
        .size = 0x30 + size,
    };
    address_space_write(&address_space_memory, apob_addr + apob_header.size, MEMTXATTRS_UNSPECIFIED, &item_header, sizeof(item_header));
    address_space_write(&address_space_memory, apob_addr + apob_header.size + 0x30, MEMTXATTRS_UNSPECIFIED, &item_header, sizeof(item_header));

    apob_header.size += 0x30 + size;
    address_space_write(&address_space_memory, apob_addr, MEMTXATTRS_UNSPECIFIED, &apob_header, sizeof(apob_header));
}

static void create_apob_raven_ridge(hwaddr apob_addr)
{
    raven_ridge_topology_t topology = {
    };
    for(int complex = 0; complex < 2; complex++) {
        topology.complexes[complex].num = complex;
        for(int core = 0; core < 4; core++) {
            topology.complexes[complex].cores[core].num = complex;
            for(int thread = 0; thread < 2; thread++) {
                topology.complexes[complex].cores[core].threads[thread].enabled = 1;
            }
        }
    }
    apob_add_entry(apob_addr, 3, 1, 0, &topology, sizeof(topology));
}

void create_apob(hwaddr apob_addr, zen_codename codename)
{
    apob_create_header(apob_addr);

    switch(codename) {
    case CODENAME_RAVEN_RIDGE:
        create_apob_raven_ridge(apob_addr);
        break;
    default:
        break;
    }
}
