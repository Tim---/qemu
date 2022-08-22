#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/qdev-core.h"
#include "hw/zen/zen-pmu.h"
#include "qom/object.h"
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/registerfields.h"
#include "qemu/fifo32.h"
#include "hw/zen/zen-utils.h"

OBJECT_DECLARE_SIMPLE_TYPE(ZenPmuState, ZEN_PMU)

typedef enum {
    IDLE,
    WAIT_ACK_LOW,
    WAIT_ACK_HIGH,
} PspMailboxState;

struct ZenPmuState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    uint8_t instance;

    uint32_t pmu_reset;
    uint32_t pmu_ready;

    Fifo32 mailbox_fifo;
    PspMailboxState mailbox_state;
    uint32_t mailbox_data;
    uint32_t psp_ack;

    MemoryRegion pmu_region;
    AddressSpace pmu_as;
    MemoryRegion pmu_imem;
    MemoryRegion pmu_dmem;
};

typedef struct PspPmuState PspPmuState;

REG32(PMU_RESET, 0xd0099) /* D18F2x9C_x0002_0099_dct[3:0] PMU Reset */
    FIELD(PMU_RESET, RESET, 3, 1)
    FIELD(PMU_RESET, STALL, 0, 1)
REG32(PMU_READY, 0xd0004) /* D18F2x9C_x0002_0004_dct[3:0] Mailbox Protocol Shadow */

REG32(PSP_ACK,   0xd0031)
REG32(DATA_LO,   0xd0032)

REG32(PLL_LOCKED,0x200c9)

static void pmu_try_send_data(ZenPmuState *s)
{
    if(s->mailbox_state != IDLE)
        return;
    if(fifo32_is_empty(&s->mailbox_fifo))
        return;

    /* Set the data */
    s->mailbox_data = fifo32_pop(&s->mailbox_fifo);
    /* Inform the PSP that it can read the data */
    s->pmu_ready = 0;
    /* Wait for the PSP to ack the data */
    s->mailbox_state = WAIT_ACK_LOW;
}

static void pmu_queue_data(ZenPmuState *s, uint32_t data)
{
    fifo32_push(&s->mailbox_fifo, data);
    pmu_try_send_data(s);
}

uint16_t zen_pmu_read(DeviceState *dev, uint32_t addr)
{
    ZenPmuState *s = ZEN_PMU(dev);

    const MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;

    if(addr >= 0x54000 && addr < 0x56000) {
        // Read DMEM
        return address_space_lduw_le(&s->pmu_as, 0x80000000 + (addr - 0x54000) * 2, attrs, NULL);
    }
    switch(addr) {
    case A_PMU_RESET:
        return s->pmu_reset;
    case A_PMU_READY:
        return s->pmu_ready;
    case A_DATA_LO:
        return s->mailbox_data;
    case A_PSP_ACK:
        return s->psp_ack;
    case 0xd00fa:
        return 1;
    case A_PLL_LOCKED:
        return 1;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%x)\n",
                __func__, addr);

    return 0;
}

void zen_pmu_write(DeviceState *dev, uint32_t addr, uint16_t data)
{
    ZenPmuState *s = ZEN_PMU(dev);

    const MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;

    if(addr >= 0x50000 && addr < 0x54000) {
        // Write IMEM
        address_space_stw_le(&s->pmu_as, 0x00000000 + (addr - 0x50000) * 2, data, attrs, NULL);
        return;
    } else if(addr >= 0x54000 && addr < 0x56000) {
        // Write DMEM
        address_space_stw_le(&s->pmu_as, 0x80000000 + (addr - 0x54000) * 2, data, attrs, NULL);
        return;
    }

    switch(addr) {
    case A_PMU_RESET:
        s->pmu_reset = data;
        if(data == 0) {
            // No reset, not stalled
            fifo32_reset(&s->mailbox_fifo);
            pmu_queue_data(s, 0xa0); /* aggressors started */
            pmu_queue_data(s, 0x07); /* finished */
        }
        return;
    case A_PSP_ACK:
        s->psp_ack = data;
        if(s->mailbox_state == WAIT_ACK_LOW && s->psp_ack == 0) {
            s->pmu_ready = 1;
            s->mailbox_state = WAIT_ACK_HIGH;
        } else if(s->mailbox_state == WAIT_ACK_HIGH && s->psp_ack == 1) {
            s->mailbox_state = IDLE;
            pmu_try_send_data(s);
        }
        return;
    }

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%x, value 0x%x)\n",
                  __func__, addr, data);


    return;
}

static void zen_pmu_init(Object *obj)
{
    ZenPmuState *s = ZEN_PMU(obj);

    create_region_with_unimpl(&s->pmu_region, obj, "pmu-region", 0x100000000UL);
    address_space_init(&s->pmu_as, &s->pmu_region, "pmu-address-space");
}

static void zen_pmu_realize(DeviceState *dev, Error **errp)
{
    ZenPmuState *s = ZEN_PMU(dev);
    Object *obj = OBJECT(dev);

    char *region_name = NULL;

    region_name = g_strdup_printf("pmu[%d]-imem", s->instance);
    memory_region_init_ram(&s->pmu_imem, obj, region_name, 0x8000, &error_fatal);
    g_free(region_name);

    region_name = g_strdup_printf("pmu[%d]-dmem", s->instance);
    memory_region_init_ram(&s->pmu_dmem, obj, region_name, 0x4000, &error_fatal);
    g_free(region_name);

    memory_region_add_subregion(&s->pmu_region, 0x00000000UL, &s->pmu_imem);
    memory_region_add_subregion(&s->pmu_region, 0x80000000UL, &s->pmu_dmem);

    fifo32_create(&s->mailbox_fifo, 10);
}

static Property zen_pmu_props[] = {
    DEFINE_PROP_UINT8("instance", ZenPmuState, instance, 0),
    DEFINE_PROP_END_OF_LIST()
};

static void zen_pmu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = zen_pmu_realize;
    dc->desc = "Zen PMU";
    device_class_set_props(dc, zen_pmu_props);
}

static const TypeInfo zen_pmu_type_info = {
    .name = TYPE_ZEN_PMU,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(ZenPmuState),
    .instance_init = zen_pmu_init,
    .class_init = zen_pmu_class_init,
};

static void zen_pmu_register_types(void)
{
    type_register_static(&zen_pmu_type_info);
}

type_init(zen_pmu_register_types)
