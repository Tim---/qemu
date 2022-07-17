#include "qemu/osdep.h"
#include "hw/i386/x86.h"
#include "cpu.h"
#include "hw/zen/zen-rom.h"
#include "hw/zen/zen-cpuid.h"
#include "sysemu/block-backend.h"
#include "exec/address-spaces.h"
#include "sysemu/reset.h"

struct PcZenMachineClass {
    X86MachineClass parent;
};

struct PcZenMachineState {
    X86MachineState parent;
    uint32_t seg_base;
    zen_codename codename;
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

    uint32_t bin_offset;
    uint32_t bin_size;
    uint32_t bin_dest;
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

#include "qemu/qemu-print.h"

static void pc_zen_machine_state_init(MachineState *machine)
{
    X86MachineState *x86ms = X86_MACHINE(machine);
    PcZenMachineState *mms = PC_ZEN_MACHINE(machine);

    machine->cpu_type = g_strdup_printf("%s-x86_64-cpu", zen_get_name(mms->codename));
    x86_cpus_init(x86ms, CPU_VERSION_LATEST);

    memory_region_add_subregion(get_system_memory(), 0, machine->ram);

    pc_zen_simulate_psp_boot(mms);
}

static void pc_zen_machine_initfn(Object *obj)
{
}

static char *pc_zen_get_codename(Object *obj, Error **errp)
{
    PcZenMachineState *mms = PC_ZEN_MACHINE(obj);

    return strdup(zen_get_name(mms->codename));
}

static void pc_zen_set_codename(Object *obj, const char *value, Error **errp)
{
    PcZenMachineState *mms = PC_ZEN_MACHINE(obj);

    mms->codename = zen_get_codename(value);
}

static void pc_zen_class_props_init(ObjectClass *oc)
{
    object_class_property_add_str(oc, "codename", pc_zen_get_codename,
                                   pc_zen_set_codename);
    object_class_property_set_description(oc, "codename",
                                          "Zen codename");
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
    pc_zen_class_props_init(oc);
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
