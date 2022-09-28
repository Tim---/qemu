#include "qemu/osdep.h"
#include "exec/memory.h"
#include "qemu/log.h"
#include "hw/zen/zen-cpuid.h"
#include "xtensa_smu_internal.h"

/*
Memory region: SMUIO
*/

static uint64_t smuio_v9_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(offset) {
    case 0x1120:
    case 0x1148:
    case 0x1170:
    case 0x1198:
    case 0x11c0:
    case 0x11e8:
        res = 0x10000;
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", __func__, offset, size);
    return res;
}

static uint64_t smuio_v10_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(offset) {
    case 0xd84:
        res = 0x50;
        break;
    case 0x1928:
    case 0x1950:
    case 0x11a0:
    case 0x1128:
    case 0x11c8:
    case 0x12e0:
    case 0x12b8:
    case 0x1178:
        res = 0x10000;
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", __func__, offset, size);
    return res;
}

static uint64_t smuio_v11_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(offset) {
    case 0x884:
    case 0x888:
    case 0x88c:
    case 0x890:
    case 0x894:
    case 0x898:
    case 0x89c:
    case 0x8a0:
        res = 0x800000;
        break;
    case 0x1130:
    case 0x1180:
    case 0x11a8:
    case 0x1958:
    case 0x1980:
        res = 0x10000;
        break;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", __func__, offset, size);
    return res;
}

static void smuio_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, offset, size, data);
}

const MemoryRegionOps smuio_v9_ops = {
    .read = smuio_v9_read,
    .write = smuio_write,
};

const MemoryRegionOps smuio_v10_ops = {
    .read = smuio_v10_read,
    .write = smuio_write,
};

const MemoryRegionOps smuio_v11_ops = {
    .read = smuio_v11_read,
    .write = smuio_write,
};

void create_smuio(MemoryRegion *smn_region, smu_version version)
{
    const MemoryRegionOps *ops = NULL;

    switch(version) {
    case SMU_V9:
        ops = &smuio_v9_ops;
        break;
    case SMU_V10:
        ops = &smuio_v10_ops;
        break;
    case SMU_V11:
        ops = &smuio_v11_ops;
        break;
    default:
        g_assert_not_reached();
    }
    MemoryRegion *region = g_malloc0(sizeof(*region));
    memory_region_init_io(region, NULL, ops, NULL, "smuio", 0x3000);
    memory_region_add_subregion(smn_region, 0x5a000, region);
}

