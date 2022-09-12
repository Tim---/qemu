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
#include "hw/zen/zen-utils.h"

#define FW_ADDR 0x00000000
#define FW_SIZE 0x40000

static void smu_reset(void *opaque)
{
    XtensaCPU *cpu = opaque;
    CPUXtensaState *env = &cpu->env;

    cpu_reset(CPU(cpu));
    env->pc = FW_ADDR + 0x100;
}

static MemoryRegion *create_smn(void)
{
    MemoryRegion *smn_region = g_malloc0(sizeof(*smn_region));
    create_region_with_unimpl(smn_region, NULL, "smn", 0x100000000UL);

    DeviceState *dev = qdev_new(TYPE_SMN_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(smn_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0x03220000, sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0));
    memory_region_add_subregion(get_system_memory(), 0x01000000, sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 1));
    return smn_region;
}

static XtensaCPU *smu_common_init(MachineState *machine)
{
    XtensaCPU *cpu = NULL;
    CPUXtensaState *env = NULL;
    int n;

    for (n = 0; n < machine->smp.cpus; n++) {
        cpu = XTENSA_CPU(cpu_create(machine->cpu_type));
        env = &cpu->env;

        env->sregs[PRID] = n;
        qemu_register_reset(smu_reset, cpu);
        smu_reset(cpu);
    }

    XtensaMemory fw_mem = {
        .num = 1,
        .location[0] = {
            .addr = FW_ADDR,
            .size = FW_SIZE,
        }
    };
    xtensa_create_memory_regions(&fw_mem, "xtensa.fw", get_system_memory());

    create_unimplemented_device("pub-regs",  0x03010000, 0x00010000);
    create_unimplemented_device("priv-regs", 0x03200000, 0x00010000);
    create_unimplemented_device("dev0321",   0x03210000, 0x00010000);
    create_unimplemented_device("dev0327",   0x03270000, 0x00010000);
    create_unimplemented_device("timer[0]",  0x03200400, 0x00000024);
    create_unimplemented_device("timer[1]",  0x03200424, 0x00000024);
    create_unimplemented_device("intc[0]",   0x03200200, 0x00000100);
    create_unimplemented_device("intc[1]",   0x03200300, 0x00000100);

    MemoryRegion *smn = create_smn();
    create_unimplemented_device_generic(smn, "smuthm", 0x59800, 0x0800);
    create_unimplemented_device_generic(smn, "smuio",  0x5a000, 0x1000);
    create_unimplemented_device_generic(smn, "fuses",  0x5d000, 0x1000);

    return cpu;
}

static void xtensa_smu_init(MachineState *machine)
{
    smu_common_init(machine);
    load_image_targphys("/tmp/smu.bin", FW_ADDR, FW_SIZE);
}

static void xtensa_smu_machine_init(MachineClass *mc)
{
    mc->desc = "smu machine (" XTENSA_DEFAULT_CPU_MODEL ")";
    mc->init = xtensa_smu_init;
    mc->max_cpus = 1;
    mc->default_cpu_type = XTENSA_CPU_TYPE_NAME("smu");
}

DEFINE_MACHINE("smu", xtensa_smu_machine_init)
