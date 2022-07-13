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

static void psp_machine_init(MachineState *machine)
{
    PspMachineState *s = PSP_MACHINE(machine);
    PspMachineClass *pmc = PSP_MACHINE_GET_CLASS(machine);
    DeviceState *dev;

    // Create CPU
    Object *cpuobj;
    cpuobj = object_new(machine->cpu_type);
    qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);

    // Create RAM
    memory_region_add_subregion(get_system_memory(), 0, machine->ram);

    // Simulate on-chip bootloader
    DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
    assert(dinfo);
    BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
    assert(blk);
    psp_on_chip_bootloader(blk, pmc->codename);

    // Memory regions
    create_unimplemented_device("psp-ccp",       0x03000000, 0x10000);
    create_unimplemented_device("psp-regs-pub",  0x03010000, 0x10000);
    create_unimplemented_device("psp-regs-priv", 0x03200000, 0x10000);
    create_unimplemented_device("smn-regs",      0x03220000, 0x10000);
    create_unimplemented_device("x86-regs",      0x03230000, 0x10000);

    /* Create PSP registers */
    dev = qdev_new(TYPE_PSP_REGS);
    qdev_prop_set_uint32(dev, "codename", pmc->codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03010000);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x03200000);

    /* Create SMN bridge */
    memory_region_init(&s->smn_region, OBJECT(s),
                       "smn-region", 0x1000000000UL);

    dev = qdev_new(TYPE_SMN_BRIDGE);
    object_property_set_link(OBJECT(dev), "source-memory",
                             OBJECT(&s->smn_region), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x03220000);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, 0x01000000);

    create_unimplemented_device_generic(&s->smn_region, "smn-unimp", 0,
                                        0x1000000000UL);

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

static void psp_machine_common_init(MachineClass *mc)
{
    PspMachineClass *pmc = PSP_MACHINE_CLASS(mc);
    mc->default_ram_size = zen_get_ram_size(pmc->codename) - 0x1000;
}

static void psp_machine_summit_ridge_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Summit Ridge)";
    pmc->codename = CODENAME_SUMMIT_RIDGE;
    psp_machine_common_init(mc);
}

static void psp_machine_pinnacle_ridge_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Pinnacle Ridge)";
    pmc->codename = CODENAME_PINNACLE_RIDGE;
    psp_machine_common_init(mc);
}

static void psp_machine_raven_ridge_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Raven Ridge)";
    pmc->codename = CODENAME_RAVEN_RIDGE;
    psp_machine_common_init(mc);
}

static void psp_machine_picasso_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Picasso)";
    pmc->codename = CODENAME_PICASSO;
    psp_machine_common_init(mc);
}

static void psp_machine_matisse_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Matisse)";
    pmc->codename = CODENAME_MATISSE;
    psp_machine_common_init(mc);
}

static void psp_machine_vermeer_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Vermeer)";
    pmc->codename = CODENAME_VERMEER;
    psp_machine_common_init(mc);
}

static void psp_machine_renoir_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Renoir)";
    pmc->codename = CODENAME_RENOIR;
    psp_machine_common_init(mc);
}

static void psp_machine_lucienne_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Lucienne)";
    pmc->codename = CODENAME_LUCIENNE;
    psp_machine_common_init(mc);
}

static void psp_machine_cezanne_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Cezanne)";
    pmc->codename = CODENAME_CEZANNE;
    psp_machine_common_init(mc);
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
