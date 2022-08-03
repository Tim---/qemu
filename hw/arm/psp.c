#include "qemu/osdep.h"
#include "hw/boards.h"
#include "cpu.h"
#include "hw/zen/zen-rom.h"
#include "sysemu/block-backend.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
#include "hw/zen/zen-cpuid.h"
#include "hw/zen/psp-ocbl.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-soc.h"
#include "hw/zen/zen-utils.h"
#include "hw/zen/fch-spi.h"
#include "hw/zen/zen-mobo.h"
#include "hw/ssi/ssi.h"
#include "hw/zen/smu.h"

#define TYPE_PSP_MACHINE                MACHINE_TYPE_NAME("psp")

struct PspMachineClass {
    MachineClass parent;
    zen_codename codename;
};

struct PspMachineState {
    MachineState parent;

    DeviceState *mobo;
    MemoryRegion *smn;
    MemoryRegion *ht;
};

OBJECT_DECLARE_TYPE(PspMachineState, PspMachineClass, PSP_MACHINE)

static void create_soc(PspMachineState *s, const char *cpu_type, zen_codename codename)
{
    DeviceState *dev = qdev_new(TYPE_PSP_SOC);
    qdev_prop_set_string(dev, "cpu-type", cpu_type);
    qdev_prop_set_uint32(dev, "codename", codename);
    object_property_set_link(OBJECT(dev), "smn-memory",
                             OBJECT(s->smn), &error_fatal);
    object_property_set_link(OBJECT(dev), "ht-memory",
                             OBJECT(s->ht), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
}

static void run_bootloader(BlockBackend *blk, zen_codename codename)
{
    AddressSpace *as = cpu_get_address_space(first_cpu, ARMASIdx_S);
    psp_on_chip_bootloader(as, blk, codename);
}

static DeviceState* create_fch_spi(PspMachineState *s, zen_codename codename)
{
    uint32_t smn_addr;

    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        smn_addr = 0x0a000000;
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        smn_addr = 0x44000000;
        break;
    default:
        g_assert_not_reached();
    }
    DeviceState *dev = qdev_new(TYPE_FCH_SPI);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map(s->mobo, SYS_BUS_DEVICE(dev), 0, 0x02dc4000, false);
    zen_mobo_smn_map(s->mobo, SYS_BUS_DEVICE(dev), 1, smn_addr, false);
    return dev;
}

static void create_spi_rom(DeviceState *fch_spi, BlockBackend *blk)
{
    const char *flash_type;

    switch(blk_getlength(blk)) {
    case 0x1000000:
        flash_type = "mx25l12805d";
        break;
    /*
    We don't know how to handle it properly for now
    case 0x2000000:
        flash_type = "mx25l25655e";
        break;
    */
    default:
        g_assert_not_reached();
    }

    fch_spi_add_flash(fch_spi, blk, flash_type);
}

static void create_smu(PspMachineState *s, zen_codename codename)
{
    DeviceState *dev = qdev_new(TYPE_SMU);
    qdev_prop_set_uint32(dev, "codename", codename);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    zen_mobo_smn_map(s->mobo, SYS_BUS_DEVICE(dev), 0, 0x03b10000, false);
}

static void create_mobo(PspMachineState *s)
{
    DeviceState *dev = qdev_new(TYPE_ZEN_MOBO);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    s->mobo = dev;
    s->ht = zen_mobo_get_ht(dev);
    s->smn = zen_mobo_get_smn(dev);
}

static void psp_machine_init(MachineState *machine)
{
    PspMachineClass *pmc = PSP_MACHINE_GET_CLASS(machine);
    PspMachineState *s = PSP_MACHINE(machine);

    // Simulate on-chip bootloader
    DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
    assert(dinfo);
    BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
    assert(blk);

    create_mobo(s);

    create_soc(s, machine->cpu_type, pmc->codename);

    DeviceState *fch_spi = create_fch_spi(s, pmc->codename);
    create_spi_rom(fch_spi, blk);
    create_smu(s, pmc->codename);

    run_bootloader(blk, pmc->codename);

    psp_dirty_create_mp2_ram(s->smn, pmc->codename);
    psp_dirty_create_smu_firmware(s->smn, pmc->codename);
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
