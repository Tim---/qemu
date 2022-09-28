#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/reset.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "exec/memory.h"
#include "qemu/error-report.h"
#include "xtensa_memory.h"
#include "xtensa_sim.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
#include "hw/zen/psp-smn-bridge.h"
#include "hw/zen/psp-ht-bridge.h"
#include "hw/zen/zen-utils.h"
#include "hw/zen/psp-intc.h"
#include "hw/zen/psp-timer.h"
#include "hw/zen/psp-mb.h"
#include "hw/zen/psp-fuses.h"
#include "hw/zen/zen-cpuid.h"
#include "qemu/log.h"

#define TYPE_SMU_MACHINE                MACHINE_TYPE_NAME("smu")

struct SmuMachineClass {
    MachineClass parent;
    smu_version version;
};

struct SmuMachineState {
    MachineState parent;
};

OBJECT_DECLARE_TYPE(SmuMachineState, SmuMachineClass, SMU_MACHINE)

#define FW_ADDR 0x00000000
#define FW_SIZE 0x80000

#define PAGES_ADDR 0xFFC00000
#define PAGES_SIZE 0x00400000

static void smu_reset(void *opaque)
{
    XtensaCPU *cpu = opaque;
    CPUXtensaState *env = &cpu->env;

    cpu_reset(CPU(cpu));
    env->pc = FW_ADDR + 0x100;
}

/*
Memory region: page cache
*/

static MemTxResult pages_read_with_attrs(void *opaque, hwaddr addr, uint64_t *data, unsigned size, MemTxAttrs attrs)
{
    return MEMTX_ACCESS_ERROR;
}

static MemTxResult pages_write_with_attrs(void *opaque, hwaddr addr, uint64_t data, unsigned size, MemTxAttrs attrs)
{
    return MEMTX_ACCESS_ERROR;
}

const MemoryRegionOps pages_ops = {
    .read_with_attrs = pages_read_with_attrs,
    .write_with_attrs = pages_write_with_attrs,
};

/*
Memory region: misc SMN devices
*/

static uint64_t smn_unmpl_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t res = 0;

    switch(offset) {
    case 0x490:
        /* For SMUv10 */
        return 0xffffffff;
    case 0x18080064:
        res = 0x200;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  (offset 0x%lx, size 0x%x)\n", __func__, offset, size);
    return res;
}

static void smn_unmpl_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, offset, size, data);
}

const MemoryRegionOps smn_unmpl_ops = {
    .read = smn_unmpl_read,
    .write = smn_unmpl_write,
};

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

/*
Memory region: "PSP" registers
*/

static uint64_t regs_read(const char *name, hwaddr offset, unsigned size)
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

    // SMUv10
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

static void regs_write(const char *name, hwaddr offset, uint64_t data, unsigned size)
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

static uint64_t priv_regs_read(void *opaque, hwaddr offset, unsigned size)
{
    return regs_read("regs-priv", offset, size);
}

static void priv_regs_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    regs_write("regs-priv", offset, data, size);
}

const MemoryRegionOps priv_regs_ops = {
    .read = priv_regs_read,
    .write = priv_regs_write,
};

static uint64_t pub_regs_read(void *opaque, hwaddr offset, unsigned size)
{
    return regs_read("regs-pub", offset, size);
}

static void pub_regs_write(void *opaque, hwaddr offset,
                            uint64_t data, unsigned size)
{
    regs_write("regs-pub", offset, data, size);
}

const MemoryRegionOps pub_regs_ops = {
    .read = pub_regs_read,
    .write = pub_regs_write,
};

/*
Create devices
*/

static void create_regs_region(const char *name, hwaddr addr, const MemoryRegionOps *ops)
{
    MemoryRegion *region = g_malloc0(sizeof(*region));
    memory_region_init_io(region, NULL, ops, NULL, name, 0x1000);
    memory_region_add_subregion(get_system_memory(), addr, region);
}

static void create_regs(void)
{
    create_regs_region("regs-pub", 0x03010000, &pub_regs_ops);
    create_regs_region("regs-priv", 0x03200000, &priv_regs_ops);
}

static MemoryRegion *create_smn_region(void)
{
    MemoryRegion *smn_region = g_malloc0(sizeof(*smn_region));
    memory_region_init_io(smn_region, NULL, &smn_unmpl_ops, NULL, "smn", 0x100000000UL);

    return smn_region;
}

static MemoryRegion *create_ht_region(void)
{
    MemoryRegion *ht_region = g_malloc0(sizeof(*ht_region));
    create_region_with_unimpl(ht_region, NULL, "ht", 0x1000000000000ULL);

    return ht_region;
}

static DeviceState *create_smn_bridge(MemoryRegion *smn_region)
{
    DeviceState *dev = qdev_new(TYPE_SMN_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(smn_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03220000);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x01000000);
    return dev;
}

static DeviceState *create_ht_bridge(MemoryRegion *smn_region)
{
    DeviceState *dev = qdev_new(TYPE_HT_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(smn_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03230000);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x04000000);
    return dev;
}

static DeviceState *create_intc(XtensaCPU *cpu)
{
    DeviceState *dev = qdev_new(TYPE_PSP_INTC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03010300);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x03200300);

    qemu_irq *extints = xtensa_get_extints(&cpu->env);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, extints[0]);
    return dev;
}

static DeviceState *create_timer(DeviceState *intc, int i)
{
    DeviceState *dev = qdev_new(TYPE_PSP_TIMER);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03200400 + i * 0x24);
    if(i == 0)
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(intc, 0x60));
    return dev;
}

static DeviceState *create_mailbox(hwaddr addr)
{
    DeviceState *dev = qdev_new(TYPE_PSP_MB);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}

static void create_mailboxes(DeviceState *intc)
{
    DeviceState *dev;
    int i;

    dev = create_mailbox(0x03010700);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 5, qdev_get_gpio_in(intc, 0x10));
    
    // SMUv9/SMUv10
    dev = create_mailbox(0x03010500);
    for(i = 0; i < 16; i++)
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, qdev_get_gpio_in(intc, 0x20 + i));
    
    // SMUv10
    dev = create_mailbox(0x03010a00);
    for(i = 0; i < 16; i++)
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, qdev_get_gpio_in(intc, 0x50 + i));
}

static void create_dev_5b(MemoryRegion *smn_region)
{
    for(int i = 0; i < 2; i++) {
        MemoryRegion *region = g_malloc0(sizeof(*region));
        memory_region_init_io(region, NULL, &dev_5b_ops, NULL, "dev-5b", 0x800);
        memory_region_add_subregion(smn_region, 0x5b000 + i * 0x800, region);
    }
}

static void create_fuses(MemoryRegion *smn_region)
{
    DeviceState *dev = qdev_new(TYPE_PSP_FUSES);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    memory_region_add_subregion(smn_region, 0x5d000, sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0));

    psp_fuses_write_bits(dev, 0x066b,  6, 0x10);
    psp_fuses_write_bits(dev, 0x0671,  1, 0x0);
    psp_fuses_write_bits(dev, 0x0680,  8, 0x7c);
    psp_fuses_write_bits(dev, 0x06e0,  6, 0x10);
    psp_fuses_write_bits(dev, 0x0726,  1, 0x1);
    psp_fuses_write_bits(dev, 0x1367, 14, 0x8ea);
    psp_fuses_write_bits(dev, 0x1376,  6, 0x8);
    psp_fuses_write_bits(dev, 0x137c,  6, 0xa);
    psp_fuses_write_bits(dev, 0x1382,  6, 0x10);
    psp_fuses_write_bits(dev, 0x1388,  6, 0x7);
    psp_fuses_write_bits(dev, 0x138e,  6, 0x14);
    psp_fuses_write_bits(dev, 0x1394,  6, 0x0);
    psp_fuses_write_bits(dev, 0x139a,  6, 0x10);
    psp_fuses_write_bits(dev, 0x13a0,  6, 0x10);
    psp_fuses_write_bits(dev, 0x13a6,  8, 0x78);
    psp_fuses_write_bits(dev, 0x13ae,  8, 0x87);
    psp_fuses_write_bits(dev, 0x13b6,  8, 0x7c);
    psp_fuses_write_bits(dev, 0x13be,  8, 0x7);
    psp_fuses_write_bits(dev, 0x13c6,  8, 0x5);
    psp_fuses_write_bits(dev, 0x13ce,  8, 0x22);
    psp_fuses_write_bits(dev, 0x13d6,  8, 0x1);
    psp_fuses_write_bits(dev, 0x13de,  8, 0x8);
    psp_fuses_write_bits(dev, 0x13e6,  8, 0x3a);
    psp_fuses_write_bits(dev, 0x13ee,  8, 0x50);
    psp_fuses_write_bits(dev, 0x13f6,  8, 0x6c);
    psp_fuses_write_bits(dev, 0x1426,  1, 0x1);
    psp_fuses_write_bits(dev, 0x1427,  1, 0x1);
    psp_fuses_write_bits(dev, 0x1428,  1, 0x1);
    psp_fuses_write_bits(dev, 0x142e,  6, 0x0);
    psp_fuses_write_bits(dev, 0x1434,  8, 0x0);
    psp_fuses_write_bits(dev, 0x143c,  8, 0x6d);
    psp_fuses_write_bits(dev, 0x1444,  3, 0x2);
    psp_fuses_write_bits(dev, 0x1447,  3, 0x3);
    psp_fuses_write_bits(dev, 0x144a,  3, 0x4);
    psp_fuses_write_bits(dev, 0x144d,  8, 0x60);
    psp_fuses_write_bits(dev, 0x1455,  8, 0x64);
    psp_fuses_write_bits(dev, 0x145d,  8, 0x68);
    psp_fuses_write_bits(dev, 0x1465,  8, 0x60);
    psp_fuses_write_bits(dev, 0x146d,  8, 0x64);
    psp_fuses_write_bits(dev, 0x1475,  8, 0x68);
    psp_fuses_write_bits(dev, 0x147d,  8, 0x60);
    psp_fuses_write_bits(dev, 0x1485,  8, 0x16);
    psp_fuses_write_bits(dev, 0x148d,  8, 0x1a);
    psp_fuses_write_bits(dev, 0x1bb1,  8, 0x5f);
    psp_fuses_write_bits(dev, 0x1bb9,  8, 0x41);
    psp_fuses_write_bits(dev, 0x1bc1,  8, 0x41);
    psp_fuses_write_bits(dev, 0x1bc9,  8, 0x41);
    psp_fuses_write_bits(dev, 0x1bd9,  8, 0x5a);
    psp_fuses_write_bits(dev, 0x1be1,  8, 0x3c);
    psp_fuses_write_bits(dev, 0x1bfa,  4, 0x2);
    psp_fuses_write_bits(dev, 0x1bfe,  8, 0x80);
    psp_fuses_write_bits(dev, 0x1c93,  1, 0x0);
    psp_fuses_write_bits(dev, 0x2100,  7, 0x41);
    psp_fuses_write_bits(dev, 0x2107,  7, 0x4d);
    psp_fuses_write_bits(dev, 0x210e,  7, 0x44);
    psp_fuses_write_bits(dev, 0x2115,  7, 0x20);
    psp_fuses_write_bits(dev, 0x211c,  7, 0x52);
    psp_fuses_write_bits(dev, 0x2123,  7, 0x79);
    psp_fuses_write_bits(dev, 0x212a,  7, 0x7a);
    psp_fuses_write_bits(dev, 0x2131,  7, 0x65);
    psp_fuses_write_bits(dev, 0x2138,  7, 0x6e);
    psp_fuses_write_bits(dev, 0x213f,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2146,  7, 0x37);
    psp_fuses_write_bits(dev, 0x214d,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2154,  7, 0x31);
    psp_fuses_write_bits(dev, 0x215b,  7, 0x37);
    psp_fuses_write_bits(dev, 0x2162,  7, 0x30);
    psp_fuses_write_bits(dev, 0x2169,  7, 0x30);
    psp_fuses_write_bits(dev, 0x2170,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2177,  7, 0x45);
    psp_fuses_write_bits(dev, 0x217e,  7, 0x69);
    psp_fuses_write_bits(dev, 0x2185,  7, 0x67);
    psp_fuses_write_bits(dev, 0x218c,  7, 0x68);
    psp_fuses_write_bits(dev, 0x2193,  7, 0x74);
    psp_fuses_write_bits(dev, 0x219a,  7, 0x2d);
    psp_fuses_write_bits(dev, 0x21a1,  7, 0x43);
    psp_fuses_write_bits(dev, 0x21a8,  7, 0x6f);
    psp_fuses_write_bits(dev, 0x21af,  7, 0x72);
    psp_fuses_write_bits(dev, 0x21b6,  7, 0x65);
    psp_fuses_write_bits(dev, 0x21bd,  7, 0x20);
    psp_fuses_write_bits(dev, 0x21c4,  7, 0x50);
    psp_fuses_write_bits(dev, 0x21cb,  7, 0x72);
    psp_fuses_write_bits(dev, 0x21d2,  7, 0x6f);
    psp_fuses_write_bits(dev, 0x21d9,  7, 0x63);
    psp_fuses_write_bits(dev, 0x21e0,  7, 0x65);
    psp_fuses_write_bits(dev, 0x21e7,  7, 0x73);
    psp_fuses_write_bits(dev, 0x21ee,  7, 0x73);
    psp_fuses_write_bits(dev, 0x21f5,  7, 0x6f);
    psp_fuses_write_bits(dev, 0x21fc,  7, 0x72);
    psp_fuses_write_bits(dev, 0x2203,  7, 0x20);
    psp_fuses_write_bits(dev, 0x220a,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2211,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2218,  7, 0x20);
    psp_fuses_write_bits(dev, 0x221f,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2226,  7, 0x20);
    psp_fuses_write_bits(dev, 0x222d,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2234,  7, 0x20);
    psp_fuses_write_bits(dev, 0x223b,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2242,  7, 0x20);
    psp_fuses_write_bits(dev, 0x2249,  7, 0x2);
    psp_fuses_write_bits(dev, 0x224a,  4, 0x1);
    psp_fuses_write_bits(dev, 0x224e,  4, 0x0);
    psp_fuses_write_bits(dev, 0x2252,  4, 0x1);
    psp_fuses_write_bits(dev, 0x23e0, 10, 0x134);
    psp_fuses_write_bits(dev, 0x23ea, 10, 0x133);
    psp_fuses_write_bits(dev, 0x23f4, 10, 0x135);
    psp_fuses_write_bits(dev, 0x2400, 10, 0xc3);
    psp_fuses_write_bits(dev, 0x240a, 10, 0xc4);
    psp_fuses_write_bits(dev, 0x2414, 10, 0x133);
    psp_fuses_write_bits(dev, 0x2420, 10, 0x133);
    psp_fuses_write_bits(dev, 0x242a, 10, 0x130);
    psp_fuses_write_bits(dev, 0x2434, 10, 0x130);
    psp_fuses_write_bits(dev, 0x248c,  8, 0x0);
    psp_fuses_write_bits(dev, 0x2494,  8, 0x0);
    psp_fuses_write_bits(dev, 0x249d,  8, 0x5d);
}

static void create_smuio(MemoryRegion *smn_region)
{
    MemoryRegion *region = g_malloc0(sizeof(*region));
    memory_region_init_io(region, NULL, &smuio_ops, NULL, "smuio", 0x1000);
    memory_region_add_subregion(smn_region, 0x5a000, region);
}

/*
Memory region: general functions
*/

static XtensaCPU *smu_common_init(MachineState *machine)
{
    XtensaCPU *cpu = NULL;
    CPUXtensaState *env = NULL;

    cpu = XTENSA_CPU(cpu_create(machine->cpu_type));
    env = &cpu->env;

    env->sregs[PRID] = 0;
    qemu_register_reset(smu_reset, cpu);
    smu_reset(cpu);

    XtensaMemory fw_mem = {
        .num = 1,
        .location[0] = {
            .addr = FW_ADDR,
            .size = FW_SIZE,
        }
    };
    xtensa_create_memory_regions(&fw_mem, "xtensa.fw", get_system_memory());

    MemoryRegion *pages_region = g_malloc0(sizeof(*pages_region));
    memory_region_init_io(pages_region, NULL, &pages_ops, NULL, "pages", PAGES_SIZE);
    memory_region_add_subregion(get_system_memory(), PAGES_ADDR, pages_region);

    create_unimplemented_device("dev0321",   0x03210000, 0x00010000);
    create_unimplemented_device("dev0327",   0x03270000, 0x00010000);

    create_regs();

    MemoryRegion *smn_region = create_smn_region();
    create_unimplemented_device_generic(smn_region, "smuthm", 0x59800, 0x0800);

    create_smn_bridge(smn_region);

    MemoryRegion *ht_region = create_ht_region();
    create_ht_bridge(ht_region);

    DeviceState *intc = create_intc(cpu);
    create_timer(intc, 0);
    create_timer(intc, 1);

    create_mailboxes(intc);
    create_dev_5b(smn_region);
    create_fuses(smn_region);
    create_smuio(smn_region);

    return cpu;
}

static void xtensa_smu_init(MachineState *machine)
{
    smu_common_init(machine);
    load_image_targphys("/tmp/smu.bin", FW_ADDR, FW_SIZE);
}

static void xtensa_smu_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "smu machine (generic)";
    mc->init = xtensa_smu_init;
    mc->max_cpus = 1;
    mc->default_cpu_type = XTENSA_CPU_TYPE_NAME("smu");
    mc->ignore_memory_transaction_failures = true;
}

static void xtensa_smu_machine_common_class_init(ObjectClass *oc, smu_version version, const char *desc)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    SmuMachineClass *smc = SMU_MACHINE_CLASS(oc);

    mc->desc = desc;
    smc->version = version;
}

static void xtensa_smu_machine_v9_class_init(ObjectClass *oc, void *data)
{
    xtensa_smu_machine_common_class_init(oc, SMU_V9, "SMU machine (v9)");
}

static void xtensa_smu_machine_v10_class_init(ObjectClass *oc, void *data)
{
    xtensa_smu_machine_common_class_init(oc, SMU_V10, "SMU machine (v10)");
}

static const TypeInfo smu_machine_types[] = {
    {
        .name          = MACHINE_TYPE_NAME("smu-v9"),
        .parent        = TYPE_SMU_MACHINE,
        .class_init    = xtensa_smu_machine_v9_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("smu-v10"),
        .parent        = TYPE_SMU_MACHINE,
        .class_init    = xtensa_smu_machine_v10_class_init,
    }, {
        .name          = TYPE_SMU_MACHINE,
        .parent        = TYPE_MACHINE,
        .class_init    = xtensa_smu_machine_class_init,
        .class_size    = sizeof(SmuMachineClass),
        .instance_size = sizeof(SmuMachineState),
        .abstract      = true,
    }
};

DEFINE_TYPES(smu_machine_types)
