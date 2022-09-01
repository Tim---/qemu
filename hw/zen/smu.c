#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/smu.h"
#include "hw/zen/zen-cpuid.h"

OBJECT_DECLARE_SIMPLE_TYPE(SmuState, SMU)

typedef enum {
    PSPSMC_MSG_UNKNOWN = 0,

    PSPSMC_MSG_CheckSocketCompatibility,
    PSPSMC_MSG_ConfigMemoryOverclockFrequency,
    PSPSMC_MSG_ConfigMemoryOverclockVoltage,
    PSPSMC_MSG_ConfigMemoryOverclockVoltage_2,
    PSPSMC_MSG_ConfigSocRail,
    PSPSMC_MSG_CpbDis,
    PSPSMC_MSG_DxioTestMessage,
    PSPSMC_MSG_EnableMemoryRetraining,
    PSPSMC_MSG_GetSmuVersion,
    PSPSMC_MSG_GetStartupDfPstate,
    PSPSMC_MSG_GetThrottlerThresholdsPER,
    PSPSMC_MSG_MemoryTrainingDone,
    PSPSMC_MSG_QueryFclkFreqOfDfPstate,
    PSPSMC_MSG_QueryMemFreqOfDfPstate,
    PSPSMC_MSG_QueryNumberOfDfPstates,
    PSPSMC_MSG_SetPhyTxVboostAndTerm,
    PSPSMC_MSG_SetSocVid,
    PSPSMC_MSG_SetVddmCldoBypassModes,
    PSPSMC_MSG_SetVddmVoltage,
    PSPSMC_MSG_SetVddpCldoBypassModes,
    PSPSMC_MSG_SetVddpVoltage,
    PSPSMC_MSG_SwitchToMemoryTrainingState,
    PSPSMC_MSG_SwitchToStartupDfPstate,
} PSPSMC_MSG;


#define MB2_COMMAND_MAX_ID  0x20
typedef struct {
    /* Mp1FWFlagsReg */
    uint32_t smu_ok_offset;

    /* Mailbox 1 */
    uint32_t mb1_cmd_offset;
    uint32_t mb1_status_offset;
    uint32_t mb1_data_offset;

    /* Mailbox 2 */
    uint32_t mb2_cmd_offset;
    uint32_t mb2_status_offset;
    uint32_t mb2_data_offset;

    PSPSMC_MSG mb2_commands[MB2_COMMAND_MAX_ID];
} SmuConfig;

struct SmuState {
    SysBusDevice parent_obj;
    zen_codename codename;
    MemoryRegion regs;

    /* Mailbox 1 */
    uint32_t mb1_cmd;
    uint32_t mb1_status;
    uint32_t mb1_data[6];

    /* Mailbox 2 */
    uint32_t mb2_cmd;
    uint32_t mb2_status;
    uint32_t mb2_data;

    SmuConfig *config;
};

typedef enum {
    SMU_V9,
    SMU_V10,
    SMU_V11,
    SMU_V12,
} SmuVersion;

SmuConfig smu_configs[] = {
    [SMU_V9] = {
        .smu_ok_offset      = 0x34,

        .mb1_cmd_offset     = 0x528,
        .mb1_status_offset  = 0x564,
        .mb1_data_offset    = 0x598,

        .mb2_cmd_offset     = 0x714,
        .mb2_data_offset    = 0x700,
        .mb2_status_offset  = 0x704,
        .mb2_commands = {
            [0x07] = PSPSMC_MSG_ConfigSocRail,
            [0x0a] = PSPSMC_MSG_SetVddpCldoBypassModes,
            [0x0b] = PSPSMC_MSG_SetVddmCldoBypassModes,
            [0x0c] = PSPSMC_MSG_SetVddpVoltage,
            [0x0d] = PSPSMC_MSG_SetVddmVoltage,
            [0x0f] = PSPSMC_MSG_DxioTestMessage,
            [0x10] = PSPSMC_MSG_SwitchToStartupDfPstate,
            [0x12] = PSPSMC_MSG_GetThrottlerThresholdsPER,
        }
    },
    [SMU_V10] = {
        .smu_ok_offset      = 0x28,

        .mb1_cmd_offset     = 0x528,
        .mb1_status_offset  = 0x564,
        .mb1_data_offset    = 0x998,

        .mb2_cmd_offset     = 0x714,
        .mb2_data_offset    = 0x700,
        .mb2_status_offset  = 0x704,
        .mb2_commands = {
            [0x06] = PSPSMC_MSG_SwitchToMemoryTrainingState,
            [0x07] = PSPSMC_MSG_SwitchToStartupDfPstate,
            [0x08] = PSPSMC_MSG_QueryNumberOfDfPstates,
            [0x09] = PSPSMC_MSG_ConfigSocRail,
            [0x0a] = PSPSMC_MSG_QueryMemFreqOfDfPstate,
            [0x0b] = PSPSMC_MSG_SetVddpCldoBypassModes,
            [0x0c] = PSPSMC_MSG_SetVddmCldoBypassModes,
            [0x0d] = PSPSMC_MSG_SetVddpVoltage,
            [0x0e] = PSPSMC_MSG_SetVddmVoltage,
            [0x0f] = PSPSMC_MSG_GetThrottlerThresholdsPER,
            [0x16] = PSPSMC_MSG_SetSocVid,
        }
    },
    [SMU_V11] = {
        .smu_ok_offset      = 0x24,

        .mb1_cmd_offset     = 0x530,
        .mb1_status_offset  = 0x57c,
        .mb1_data_offset    = 0x9c4,

        .mb2_cmd_offset     = 0x718,
        .mb2_data_offset    = 0x700,
        .mb2_status_offset  = 0x704,
        .mb2_commands = {
            [0x02] = PSPSMC_MSG_GetSmuVersion,
            [0x05] = PSPSMC_MSG_SwitchToMemoryTrainingState,
            [0x06] = PSPSMC_MSG_SwitchToStartupDfPstate,
            [0x07] = PSPSMC_MSG_QueryNumberOfDfPstates,
            [0x08] = PSPSMC_MSG_QueryMemFreqOfDfPstate,
            [0x09] = PSPSMC_MSG_ConfigSocRail,
            [0x0b] = PSPSMC_MSG_CheckSocketCompatibility,
            [0x0c] = PSPSMC_MSG_DxioTestMessage,
            [0x0e] = PSPSMC_MSG_QueryFclkFreqOfDfPstate,
            [0x16] = PSPSMC_MSG_MemoryTrainingDone,
            [0x1a] = PSPSMC_MSG_ConfigMemoryOverclockFrequency,
            [0x1b] = PSPSMC_MSG_ConfigMemoryOverclockVoltage,
            [0x1c] = PSPSMC_MSG_CpbDis,
            [0x1d] = PSPSMC_MSG_GetStartupDfPstate,
            [0x1f] = PSPSMC_MSG_ConfigMemoryOverclockVoltage_2,
        }
    },
    [SMU_V12] = {
        .smu_ok_offset      = 0x24,

        .mb1_cmd_offset     = 0x528,
        .mb1_status_offset  = 0x564,
        .mb1_data_offset    = 0x998,

        .mb2_cmd_offset     = 0x71c,
        .mb2_data_offset    = 0x700,
        .mb2_status_offset  = 0x704,
        .mb2_commands = {
            [0x05] = PSPSMC_MSG_SwitchToMemoryTrainingState,
            [0x06] = PSPSMC_MSG_SwitchToStartupDfPstate,
            [0x07] = PSPSMC_MSG_QueryNumberOfDfPstates,
            [0x08] = PSPSMC_MSG_ConfigSocRail,
            [0x09] = PSPSMC_MSG_QueryMemFreqOfDfPstate,
            [0x0a] = PSPSMC_MSG_SetVddmVoltage,
            [0x0b] = PSPSMC_MSG_ConfigMemoryOverclockFrequency,
            [0x0c] = PSPSMC_MSG_ConfigMemoryOverclockVoltage,
            [0x14] = PSPSMC_MSG_DxioTestMessage,
            [0x16] = PSPSMC_MSG_EnableMemoryRetraining,
            [0x19] = PSPSMC_MSG_SetVddpVoltage,
            [0x1a] = PSPSMC_MSG_SetPhyTxVboostAndTerm,
            [0x1b] = PSPSMC_MSG_CpbDis,
            [0x1c] = PSPSMC_MSG_SetSocVid,
        }
    },
};

static void smu_mb1_execute(SmuState *s)
{
    qemu_log_mask(LOG_UNIMP, "%s: execute (cmd=0x%x)\n", __func__, s->mb1_cmd);
    for(int i = 0; i < 6; i++)
        s->mb1_data[i] = 0;
    
    switch(s->mb1_cmd) {
    case 8:
        s->mb1_data[0] = 3; /* DXIO_MBOX_RETVAL_OK */
        break;
    }
    s->mb1_status = 1;
}

static void smu_mb2_execute(SmuState *s)
{
    qemu_log_mask(LOG_UNIMP, "%s: execute (cmd=0x%x, data=0x%x)\n", __func__, s->mb2_cmd, s->mb2_data);

    assert(s->mb2_cmd < MB2_COMMAND_MAX_ID);

    uint32_t res = 0;

    switch(s->config->mb2_commands[s->mb2_cmd]) {
    case PSPSMC_MSG_QueryNumberOfDfPstates:
        res = 1;
        break;
    case PSPSMC_MSG_QueryMemFreqOfDfPstate:
        res = 800; /* MHz */
    default:
        break;
    }

    s->mb2_data = res;
    s->mb2_status = 1;
}

static uint64_t smu_read(void *opaque, hwaddr addr, unsigned size)
{
    SmuState *s = SMU(opaque);

    if(addr == s->config->smu_ok_offset) {
        return 1;
    } else if(addr == s->config->mb1_status_offset) {
        return s->mb1_status;
    } else if(addr >= s->config->mb1_data_offset && addr < s->config->mb1_data_offset + 6 * 4) {
        return s->mb1_data[(addr - s->config->mb1_data_offset) / 4];
    } else if(addr == s->config->mb2_status_offset) {
        return s->mb2_status;
    } else if(addr == s->config->mb2_data_offset) {
        return s->mb2_data;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n", __func__, addr);

    return 0;
}

static void smu_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    SmuState *s = SMU(opaque);

    if(addr == s->config->mb1_cmd_offset) {
        s->mb1_cmd = value;
        smu_mb1_execute(s);
        return;
    } else if(addr == s->config->mb1_status_offset) {
        s->mb1_status = value;
        return;
    } else if(addr >= s->config->mb1_data_offset && addr < s->config->mb1_data_offset + 6 * 4) {
        s->mb1_data[(addr - s->config->mb1_data_offset) / 4] = value;
        return;
    } else if(addr == s->config->mb2_cmd_offset) {
        s->mb2_cmd = value;
        smu_mb2_execute(s);
        return;
    } else if(addr == s->config->mb2_status_offset) {
        s->mb2_status = value;
        return;
    } else if(addr == s->config->mb2_data_offset) {
        s->mb2_data = value;
        return;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, addr, value);
}

static const MemoryRegionOps smu_data_ops = {
    .read = smu_read,
    .write = smu_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void smu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SmuState *s = SMU(obj);

    /* Is this region an alias of 0x03010000 ? */
    memory_region_init_io(&s->regs, OBJECT(s), &smu_data_ops, s,
                          "smu",
                          0x1000);
    sysbus_init_mmio(sbd, &s->regs);
}

static void smu_realize(DeviceState *dev, Error **errp)
{
    SmuState *s = SMU(dev);

    switch(s->codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        s->config = &smu_configs[SMU_V9];
        break;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        s->config = &smu_configs[SMU_V10];
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        s->config = &smu_configs[SMU_V11];
        break;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        s->config = &smu_configs[SMU_V12];
        break;
    default:
        g_assert_not_reached();
    }

    s->mb2_status = 1;
}

static Property psp_soc_props[] = {
    DEFINE_PROP_UINT32("codename", SmuState, codename, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void smu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = smu_realize;
    dc->desc = "Zen SMU";
    device_class_set_props(dc, psp_soc_props);
}

static const TypeInfo smu_type_info = {
    .name = TYPE_SMU,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SmuState),
    .instance_init = smu_init,
    .class_init = smu_class_init,
};

static void smu_register_types(void)
{
    type_register_static(&smu_type_info);
}

type_init(smu_register_types)
