#include "qemu/osdep.h"
#include "exec/memory.h"
#include "qemu/log.h"
#include "xtensa_smu_internal.h"

/*
Memory region: device at SMN:0x5b000
*/

static uint64_t dev_5b_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    /*
    The device at 0x5b000 is divided in 2 blocks of size 0x800.
    Each block seems to describe 12 "things".
    At offset 0xfc, we have 12 regions of size 40.
    When the firmware writes some values, it waits for the last register to have bit 16 enabled (ACK ?).
    */
    for(int i = 0; i < 12; i++) {
        if(offset == 0x104 + i * 40 + 0x24) {
            res = 0x10000;
        }
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", __func__, offset, size);
    return res;
}

static void dev_5b_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, offset, size, data);
}

const MemoryRegionOps dev_5b_ops = {
    .read = dev_5b_read,
    .write = dev_5b_write,
};

void create_dev_5b(MemoryRegion *smn_region)
{
    for(int i = 0; i < 2; i++) {
        MemoryRegion *region = g_malloc0(sizeof(*region));
        memory_region_init_io(region, NULL, &dev_5b_ops, NULL, "dev-5b", 0x800);
        memory_region_add_subregion(smn_region, 0x5b000 + i * 0x800, region);
    }
}

