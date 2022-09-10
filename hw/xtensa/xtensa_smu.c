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

#define FW_ADDR 0x00000000
#define FW_SIZE 0x40000

static void smu_reset(void *opaque)
{
    XtensaCPU *cpu = opaque;
    CPUXtensaState *env = &cpu->env;

    cpu_reset(CPU(cpu));
    env->pc = FW_ADDR + 0x100;
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
    create_unimplemented_device("smn-regs",  0x03220000, 0x00010000);

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
