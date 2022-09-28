#include "qemu/osdep.h"
#include "exec/memory.h"
#include "qemu/log.h"
#include "hw/zen/zen-cpuid.h"
#include "xtensa_smu_internal.h"
#include "qemu/timer.h"
#include "exec/address-spaces.h"

/*
SMU v9
*/

static uint64_t regs_v9_read(const char *name, hwaddr offset, unsigned size)
{
    switch(offset) {
    // SMUv9
    case 0x54:
    case 0x56: // size 2
        return 0;
    case 0x58:
        return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    case 0x5c:
        return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) >> 32;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", name, offset, size);
    return 0;
}

static void regs_v9_write(const char *name, hwaddr offset, uint64_t data, unsigned size)
{
    switch(offset) {
    case 0x54:
    case 0x56:
        return; // noisy
    case 0xf00 ... 0xfff:
        /* Debug registers ? */
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", name, offset, size, data);
}

/*
SMU V10
*/

static uint64_t regs_v10_read(const char *name, hwaddr offset, unsigned size)
{
    switch(offset) {
    case 0x4c:
        return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    case 0x50:
        return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) >> 32;
    case 0xf00 ... 0xfff:
        /* Debug registers ? */
        return 0;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", name, offset, size);
    return 0;
}

static void regs_v10_write(const char *name, hwaddr offset, uint64_t data, unsigned size)
{
    switch(offset) {
    case 0xf00 ... 0xfff:
        /* Debug registers ? */
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", name, offset, size, data);
}

struct {
    uint64_t (*read)(const char *name, hwaddr offset, unsigned size);
    void (*write)(const char *name, hwaddr offset, uint64_t data, unsigned size);
} regs_handlers[] = {
    [SMU_V9] = {
        .read = regs_v9_read,
        .write = regs_v9_write,
    },
    [SMU_V10] = {
        .read = regs_v10_read,
        .write = regs_v10_write,
    },
};

/*
Memory region: "PSP" registers
*/

static uint64_t regs_read(smu_version version, const char *name, hwaddr offset, unsigned size)
{
    return regs_handlers[version].read(name, offset, size);
}

static void regs_write(smu_version version, const char *name, hwaddr offset, uint64_t data, unsigned size)
{
    regs_handlers[version].write(name, offset, data, size);
}

static uint64_t priv_regs_read(void *opaque, hwaddr offset, unsigned size)
{
    return regs_read((smu_version) opaque, "regs-priv", offset, size);
}

static void priv_regs_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    regs_write((smu_version) opaque, "regs-priv", offset, data, size);
}

const MemoryRegionOps priv_regs_ops = {
    .read = priv_regs_read,
    .write = priv_regs_write,
};

static uint64_t pub_regs_read(void *opaque, hwaddr offset, unsigned size)
{
    return regs_read((smu_version) opaque, "regs-pub", offset, size);
}

static void pub_regs_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    regs_write((smu_version) opaque, "regs-pub", offset, data, size);
}

const MemoryRegionOps pub_regs_ops = {
    .read = pub_regs_read,
    .write = pub_regs_write,
};

static void create_regs_region(const char *name, hwaddr addr, const MemoryRegionOps *ops, smu_version version)
{
    MemoryRegion *region = g_malloc0(sizeof(*region));
    memory_region_init_io(region, NULL, ops, (void *)version, name, 0x1000);
    memory_region_add_subregion(get_system_memory(), addr, region);
}

void create_regs(smu_version version)
{
    create_regs_region("regs-pub", 0x03010000, &pub_regs_ops, version);
    create_regs_region("regs-priv", 0x03200000, &priv_regs_ops, version);
}
