#include "qemu/osdep.h"
#include "hw/i386/x86.h"
#include "cpu.h"
#include "hw/zen/zen-rom.h"
#include "hw/zen/zen-cpuid.h"
#include "sysemu/block-backend.h"
#include "exec/address-spaces.h"
#include "sysemu/reset.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/zen/zen-mobo.h"
#include "hw/zen/zen-utils.h"
#include "hw/zen/fch-lpc-bridge.h"

struct PcZenMachineClass {
    X86MachineClass parent;
};

struct PcZenMachineState {
    X86MachineState parent;
    uint32_t seg_base;
    zen_codename codename;

    DeviceState *mobo;
    MemoryRegion *smn;
    MemoryRegion *ht;
};

#define TYPE_PC_ZEN_MACHINE   MACHINE_TYPE_NAME("pc-zen")
OBJECT_DECLARE_TYPE(PcZenMachineState, PcZenMachineClass, PC_ZEN_MACHINE)

static void pc_zen_simulate_psp_boot(PcZenMachineState *mms)
{
    // Simulate PSP bootloader
    DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
    assert(dinfo);
    BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
    assert(blk);

    zen_rom_infos_t infos;
    assert(zen_rom_init(&infos, blk, mms->codename));

    uint64_t bin_offset;
    uint32_t bin_size;
    uint64_t bin_dest;
    assert(zen_rom_get_bios_entry(&infos, AMD_BIOS_BIN, &bin_offset, &bin_size, &bin_dest));

    g_autofree void *buf = g_malloc(bin_size);
    zen_rom_read(&infos, bin_offset, buf, bin_size);
    address_space_write(&address_space_memory, bin_dest, MEMTXATTRS_UNSPECIFIED, buf, bin_size);

    mms->seg_base = bin_dest + bin_size - 0x10000;

    // TODO: temporary
    monitor_remove_blk(blk);
}

static void pc_zen_machine_reset(MachineState *machine)
{
    PcZenMachineState *mms = PC_ZEN_MACHINE(machine);
    CPUState *cs;

    qemu_devices_reset();

    CPU_FOREACH(cs) {
        X86CPU *cpu = X86_CPU(cs);
        CPUX86State *env = &cpu->env;
        cpu_reset(cs);
        cpu_x86_load_seg_cache(env, R_CS, 0xf000, mms->seg_base, 0xffff,
                            DESC_P_MASK | DESC_S_MASK | DESC_CS_MASK |
                            DESC_R_MASK | DESC_A_MASK);
    }
}

static zen_codename pc_zen_get_codename(MachineState *machine)
{
    const char *class_name = machine->cpu_type;
    assert(g_str_has_suffix(class_name, X86_CPU_TYPE_SUFFIX));
    g_autofree char *name = g_strndup(class_name,
                            strlen(class_name) - strlen(X86_CPU_TYPE_SUFFIX));
    return zen_get_codename(name);
}

static void create_mobo(PcZenMachineState *s)
{
    DeviceState *dev = qdev_new(TYPE_ZEN_MOBO);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    s->mobo = dev;
    s->ht = zen_mobo_get_ht(dev);
    s->smn = zen_mobo_get_smn(dev);
}

static MemoryRegion *create_pcie_mmconfig(PcZenMachineState *s)
{
    MemoryRegion *pcie_mmconfig = g_malloc(sizeof(*pcie_mmconfig));
    create_region_with_unimpl(pcie_mmconfig, OBJECT(s), "pcie-mmconfig", 0x4000000);
    memory_region_add_subregion(s->ht, 0xf0000000UL, pcie_mmconfig);
    return pcie_mmconfig;
}

static DeviceState *create_fch_lpc_bridge(MemoryRegion *pcie_mmconfig)
{
    DeviceState *dev = qdev_new(TYPE_FCH_LPC_BRIDGE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    MemoryRegion *mmio = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_add_subregion(pcie_mmconfig, 0xa3000, mmio);
    return dev;
}

static void map_ht_to_cpu(MachineState *machine)
{
    PcZenMachineState *mms = PC_ZEN_MACHINE(machine);

    memory_region_add_subregion(get_system_memory(), 0, mms->ht);
    memory_region_add_subregion(mms->ht, 0, machine->ram);

    MemoryRegion *io = g_malloc(sizeof(*io));
    memory_region_init(io, OBJECT(machine), "io", 0x10000);
    memory_region_init_alias(io, OBJECT(machine), "io", mms->ht, 0xfffdfc000000, 0x10000);
    memory_region_add_subregion(get_system_io(), 0, io);

}

static void pc_zen_machine_state_init(MachineState *machine)
{
    X86MachineState *x86ms = X86_MACHINE(machine);
    PcZenMachineState *mms = PC_ZEN_MACHINE(machine);

    mms->codename = pc_zen_get_codename(machine);

    create_mobo(mms);

    x86_cpus_init(x86ms, CPU_VERSION_LATEST);

    map_ht_to_cpu(machine);

    pc_zen_simulate_psp_boot(mms);

    MemoryRegion *pcie_mmconfig = create_pcie_mmconfig(mms);
    create_fch_lpc_bridge(pcie_mmconfig);
}

static void pc_zen_machine_initfn(Object *obj)
{
}

static void pc_zen_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->init = pc_zen_machine_state_init;

    mc->family = "pc_zen";
    mc->desc = "AMD PC with Zen architecture";
    mc->default_cpu_type = TARGET_DEFAULT_CPU_TYPE;
    mc->default_ram_id = "pc_zen.ram";
    mc->max_cpus = 1;
    mc->reset = pc_zen_machine_reset;
}

static const TypeInfo pc_zen_machine_info = {
    .name          = TYPE_PC_ZEN_MACHINE,
    .parent        = TYPE_X86_MACHINE,
    .instance_size = sizeof(PcZenMachineState),
    .instance_init = pc_zen_machine_initfn,
    .class_size    = sizeof(PcZenMachineClass),
    .class_init    = pc_zen_class_init,
};

static void pc_zen_machine_init(void)
{
    type_register_static(&pc_zen_machine_info);
}
type_init(pc_zen_machine_init);