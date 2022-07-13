#include "qemu/osdep.h"
#include "hw/boards.h"
#include "cpu.h"
#include "hw/zen/zen-rom.h"
#include "sysemu/block-backend.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
#include "hw/zen/psp-regs.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/psp-ocbl.h"
#include "hw/zen/psp-smn-bridge.h"
#include "hw/zen/zen-utils.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-timer.h"
#include "hw/zen/ccp.h"

#define TYPE_PSP_MACHINE                MACHINE_TYPE_NAME("psp")

struct PspMachineClass {
    MachineClass parent;
    zen_codename codename;
};

struct PspMachineState {
    MachineState parent;

    MemoryRegion smn_region;
};

OBJECT_DECLARE_TYPE(PspMachineState, PspMachineClass, PSP_MACHINE)

static void create_cpu(const char *cpu_type)
{
    // Create CPU
    Object *cpuobj;
    cpuobj = object_new(cpu_type);
    qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
}

static void create_ram(MemoryRegion *ram)
{
    memory_region_add_subregion(get_system_memory(), 0, ram);
}

static void create_psp_regs(zen_codename codename)
{
    /* Create PSP registers */
    DeviceState *dev = qdev_new(TYPE_PSP_REGS);
    qdev_prop_set_uint32(dev, "codename", codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03010000);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x03200000);
}

static void create_smn(PspMachineState *s)
{
    /* Create SMN bridge */
    memory_region_init(&s->smn_region, OBJECT(s),
                       "smn-region", 0x1000000000UL);

    DeviceState *dev = qdev_new(TYPE_SMN_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(&s->smn_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03220000);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x01000000);

    create_unimplemented_device_generic(&s->smn_region, "smn-unimp", 0,
                                        0x1000000000UL);
}

static void create_timer(int i)
{
    DeviceState *dev = qdev_new(TYPE_PSP_TIMER);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03010400 + i * 0x24);
}

static void create_unimplemented(void)
{
    create_unimplemented_device("x86-regs",      0x03230000, 0x10000);
}

static void run_bootloader(zen_codename codename)
{
    // Simulate on-chip bootloader
    DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
    assert(dinfo);
    BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
    assert(blk);
    psp_on_chip_bootloader(blk, codename);
}

static void create_ccp(zen_codename codename)
{
    DeviceState *dev = qdev_new(TYPE_CCP);
    object_property_set_link(OBJECT(dev), "system-memory",
                             OBJECT(get_system_memory()), &error_fatal);
    qdev_prop_set_uint32(dev, "num-queues", zen_get_num_ccp_queues(codename));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03000000);
}

static void psp_machine_init(MachineState *machine)
{
    PspMachineState *s = PSP_MACHINE(machine);
    PspMachineClass *pmc = PSP_MACHINE_GET_CLASS(machine);

    create_cpu(machine->cpu_type);
    create_ram(machine->ram);
    run_bootloader(pmc->codename);
    create_unimplemented();
    create_psp_regs(pmc->codename);
    create_smn(s);
    create_ccp(pmc->codename);

    for(int i = 0; i < 2; i++) {
        create_timer(i);
    }

    psp_create_config(pmc->codename);
}

static void psp_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "AMD PSP";
    mc->init = psp_machine_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a7");
    mc->default_ram_id = "psp.ram";
    mc->ignore_memory_transaction_failures = true;
}

static void psp_machine_common_init(ObjectClass *oc, zen_codename codename,
                                    const char *desc)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);

    mc->desc = desc;
    pmc->codename = codename;
    mc->default_ram_size = zen_get_ram_size(codename) - 0x1000;
}

static void psp_machine_summit_ridge_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_SUMMIT_RIDGE, "AMD PSP (Summit Ridge)");
}

static void psp_machine_pinnacle_ridge_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_PINNACLE_RIDGE, "AMD PSP (Pinnacle Ridge)");
}

static void psp_machine_raven_ridge_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_RAVEN_RIDGE, "AMD PSP (Raven Ridge)");
}

static void psp_machine_picasso_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_PICASSO, "AMD PSP (Picasso)");
}

static void psp_machine_matisse_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_MATISSE, "AMD PSP (Matisse)");
}

static void psp_machine_vermeer_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_VERMEER, "AMD PSP (Vermeer)");
}

static void psp_machine_renoir_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_RENOIR, "AMD PSP (Renoir)");
}

static void psp_machine_lucienne_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_LUCIENNE, "AMD PSP (Lucienne)");
}

static void psp_machine_cezanne_class_init(ObjectClass *oc, void *data)
{
    psp_machine_common_init(oc, CODENAME_CEZANNE, "AMD PSP (Cezanne)");
}

static const TypeInfo psp_machine_types[] = {
    {
        .name          = MACHINE_TYPE_NAME("psp-summit-ridge"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_summit_ridge_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-pinnacle-ridge"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_pinnacle_ridge_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-raven-ridge"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_raven_ridge_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-picasso"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_picasso_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-matisse"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_matisse_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-vermeer"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_vermeer_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-renoir"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_renoir_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-lucienne"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_lucienne_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("psp-cezanne"),
        .parent        = TYPE_PSP_MACHINE,
        .class_init    = psp_machine_cezanne_class_init,
    }, {
        .name          = TYPE_PSP_MACHINE,
        .parent        = TYPE_MACHINE,
        .class_init    = psp_machine_class_init,
        .class_size    = sizeof(PspMachineClass),
        .instance_size = sizeof(PspMachineState),
        .abstract      = true,
    }
};

DEFINE_TYPES(psp_machine_types)
