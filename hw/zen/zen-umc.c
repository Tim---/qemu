#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "hw/zen/zen-umc.h"
#include "hw/registerfields.h"
#include "hw/zen/zen-pmu.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenUmcState, ZEN_UMC)

struct ZenUmcState {
    SysBusDevice parent_obj;
    uint8_t instance;

    MemoryRegion region;
    uint32_t indirect_addr;
    uint32_t indirect_data;
    uint32_t storage[0x2000/4];

    DeviceState *pmu;
};

REG32(CTRL_EMU_TYPE,         0x1050)
typedef enum {
    EMU_SIMULATION = 0xb1aab1aa,
    EMU_EMULATION1 = 0x4e554e55,
    EMU_EMULATION2 = 0x51e051e0,
    EMU_WITH_PORT80_DEBUG = 0x5a335a33,
} emu_type_e;

/*
 * Indirect access to the PMU.
 * See: coreboot/src/vendorcode/amd/agesa/f16kb/Proc/Mem/mnpmu.h
 * See: bkdg fam 15h mod 70h-7fh, look for "D18F2x9C_dct0"
 */
REG32(CTRL_PMU_ADDR,         0x1800) /* BFDctAddlOffsetReg, D18F2x98_dct[0] */
    FIELD(CTRL_PMU_ADDR, ACCESS_WRITE,   31,  1)
    FIELD(CTRL_PMU_ADDR, AUTO_INCREMENT, 30,  1)
    FIELD(CTRL_PMU_ADDR, OFFSET,          0, 30)
REG32(CTRL_PMU_DATA,         0x1804) /* BFDctAddlDataReg, D18F2x9C_dct[0] */
REG32(CTRL_PMU_STATUS,       0x1808) /* BFDctAddlStatusReg ? */

static uint64_t zen_umc_read(void *opaque, hwaddr offset, unsigned size)
{
    ZenUmcState *s = ZEN_UMC(opaque);

    switch(offset) {
    case A_CTRL_EMU_TYPE:
        return 0;
    case A_CTRL_PMU_DATA:
        assert(FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, ACCESS_WRITE) == 0);
        assert(FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, AUTO_INCREMENT) == 0);
        uint32_t pmu_offset = FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, OFFSET);
        s->indirect_data = zen_pmu_read(s->pmu, pmu_offset);
        return s->indirect_data;
    case A_CTRL_PMU_STATUS:
        return 1; /* Ready */
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);
    return s->storage[offset/4];
}

static void zen_umc_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    ZenUmcState *s = ZEN_UMC(opaque);

    switch(offset) {
    case A_CTRL_PMU_ADDR:
        s->indirect_addr = value;

        if(FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, ACCESS_WRITE) == 1) {
            uint32_t pmu_offset = FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, OFFSET);
            zen_pmu_write(s->pmu, pmu_offset, s->indirect_data);

            if(FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, AUTO_INCREMENT) == 1) {
                pmu_offset++;
                s->indirect_addr = FIELD_DP32(s->indirect_addr, CTRL_PMU_ADDR, OFFSET, pmu_offset);
            }
        }
        return;
    case A_CTRL_PMU_DATA:
        s->indirect_data = value;

        if(FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, ACCESS_WRITE) == 1) {
            uint32_t pmu_offset = FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, OFFSET);
            if(FIELD_EX32(s->indirect_addr, CTRL_PMU_ADDR, AUTO_INCREMENT) == 1) {
                zen_pmu_write(s->pmu, pmu_offset, s->indirect_data);
                pmu_offset++;
                s->indirect_addr = FIELD_DP32(s->indirect_addr, CTRL_PMU_ADDR, OFFSET, pmu_offset);
            }
        }
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, value);
    s->storage[offset/4] = value;
}

static const MemoryRegionOps zen_umc_ops = {
    .read = zen_umc_read,
    .write = zen_umc_write,
};

static void zen_umc_init(Object *obj)
{
}

static void zen_umc_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    ZenUmcState *s = ZEN_UMC(dev);

    /* Init the registers access */
    memory_region_init_io(&s->region, OBJECT(s), &zen_umc_ops, s,
                          "zen-umc", 0x2000);
    sysbus_init_mmio(sbd, &s->region);

    s->pmu = qdev_new(TYPE_ZEN_PMU);
    qdev_prop_set_uint8(s->pmu, "instance", s->instance);
    qdev_realize_and_unref(s->pmu, NULL, &error_fatal);
}

static Property zen_umc_props[] = {
    DEFINE_PROP_UINT8("instance", ZenUmcState, instance, 0),
    DEFINE_PROP_END_OF_LIST()
};

static void zen_umc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = zen_umc_realize;
    dc->desc = "Zen UMC";
    device_class_set_props(dc, zen_umc_props);
}

static const TypeInfo zen_umc_type_info = {
    .name = TYPE_ZEN_UMC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ZenUmcState),
    .instance_init = zen_umc_init,
    .class_init = zen_umc_class_init,
};

static void zen_umc_register_types(void)
{
    type_register_static(&zen_umc_type_info);
}

type_init(zen_umc_register_types)
