#include "qemu/osdep.h"
#include "exec/memory.h"
#include "qemu/log.h"
#include "xtensa_smu_internal.h"

/*
Memory region: SMUIO
*/

static uint64_t smuio_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(offset) {
    case 0xd84:
        res = 0x50; // SMUv10
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", __func__, offset, size);
    return res;
}

static void smuio_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, offset, size, data);
}

const MemoryRegionOps smuio_ops = {
    .read = smuio_read,
    .write = smuio_write,
};

void create_smuio(MemoryRegion *smn_region)
{
    MemoryRegion *region = g_malloc0(sizeof(*region));
    memory_region_init_io(region, NULL, &smuio_ops, NULL, "smuio", 0x1000);
    memory_region_add_subregion(smn_region, 0x5a000, region);
}

