#include "qemu/osdep.h"
#include "hw/boards.h"
#include "cpu.h"
#include "hw/zen/zen-rom.h"
#include "sysemu/block-backend-global-state.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"

#define TYPE_PSP_MACHINE                MACHINE_TYPE_NAME("psp")

struct PspMachineClass {
    MachineClass parent;
    uint32_t     cpuid;
};

struct PspMachineState {
    MachineState parent;
};

OBJECT_DECLARE_TYPE(PspMachineState, PspMachineClass, PSP_MACHINE)

static void psp_on_chip_bootloader(uint32_t cpuid)
{
    bool ret;
    uint32_t bl_offset, bl_size;

    DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);
    assert(dinfo);
    BlockBackend *blk = blk_by_legacy_dinfo(dinfo);
    assert(blk);

    zen_rom_infos_t infos;
    ret = zen_rom_find_embedded_fw(&infos, blk, cpuid);
    assert(ret);

    ret = zen_rom_get_psp_entry(&infos, AMD_FW_PSP_BOOTLOADER, &bl_offset, &bl_size);
    assert(ret);

    void *buf = g_malloc(bl_size);
    zen_rom_read(&infos, bl_offset, buf, bl_size);

    address_space_write(&address_space_memory, 0, MEMTXATTRS_UNSPECIFIED, buf, bl_size);

    // TODO: temporary
    monitor_remove_blk(blk);

    cpu_set_pc(first_cpu, 0x100);
}

static void psp_machine_init(MachineState *machine)
{
    PspMachineClass *pmc = PSP_MACHINE_GET_CLASS(machine);

    // Create CPU
    Object *cpuobj;
    cpuobj = object_new(machine->cpu_type);
    qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);

    // Create RAM
    memory_region_add_subregion(get_system_memory(), 0, machine->ram);

    psp_on_chip_bootloader(pmc->cpuid);
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

static void psp_machine_summit_ridge_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Summit Ridge)";
    pmc->cpuid = 0x00800f11;
    mc->default_ram_size = 0x3f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_pinnacle_ridge_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Pinnacle Ridge)";
    pmc->cpuid = 0x00800f82;
    mc->default_ram_size = 0x3f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_raven_ridge_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Raven Ridge)";
    pmc->cpuid = 0x00810f10;
    mc->default_ram_size = 0x3f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_picasso_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Picasso)";
    pmc->cpuid = 0x00810f81;
    mc->default_ram_size = 0x3f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_matisse_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Matisse)";
    pmc->cpuid = 0x00870f10;
    mc->default_ram_size = 0x4f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_vermeer_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Vermeer)";
    pmc->cpuid = 0x00a20f10;
    mc->default_ram_size = 0x4f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_renoir_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Renoir)";
    pmc->cpuid = 0x00860f01;
    mc->default_ram_size = 0x4f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_lucienne_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Lucienne)";
    pmc->cpuid = 0x00860f81;
    mc->default_ram_size = 0x4f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
}

static void psp_machine_cezanne_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PspMachineClass *pmc = PSP_MACHINE_CLASS(oc);
    mc->desc = "AMD PSP (Cezanne)";
    pmc->cpuid = 0x00a50f00;
    mc->default_ram_size = 0x4f000; /* 0x40000 For Zen/Zen+, 0x50000 for Zen2/3 */
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
