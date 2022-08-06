#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/registerfields.h"
#include "hw/acpi/acpi.h"
#include "hw/zen/fch.h"
#include "exec/address-spaces.h"
#include "qemu/range.h"

#define PM   0x0300
#define AOAC 0x1e00

#define TOTAL_SIZE 0x2000

/* FCH::PM:: */
REG32(S5ResetStat,      PM + 0xC0)
    FIELD(S5ResetStat, UserRst, 16, 1)

/* FCH::AOAC */
REG8(DevD3Ctl_eSPI,     AOAC + 0x76)
    FIELD(DevD3Ctl_eSPI,   PwrOnDev,      3, 1)
REG8(DevD3State_eSPI,   AOAC + 0x77)
    FIELD(DevD3State_eSPI, RstBState,     2, 1)
    FIELD(DevD3State_eSPI, RefClkOkState, 1, 1)
    FIELD(DevD3State_eSPI, PwrRstBState,  0, 1)


struct FchState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion acpi_region;
    uint8_t storage[TOTAL_SIZE];
    ACPIREGS acpi_regs;
};

OBJECT_DECLARE_SIMPLE_TYPE(FchState, FCH)

static inline uint8_t get_reg8(FchState *s, hwaddr offset)
{
    return ldub_p(s->storage + offset);
}

static inline void set_reg8(FchState *s, hwaddr offset, uint8_t value)
{
    stb_p(s->storage + offset, value);
}

static void update_aoac_espi(FchState *s)
{
    uint8_t ctl = get_reg8(s, A_DevD3Ctl_eSPI);
    uint8_t pwron = FIELD_EX8(ctl, DevD3Ctl_eSPI, PwrOnDev);
    uint8_t state = get_reg8(s, A_DevD3State_eSPI);

    state = FIELD_DP32(state, DevD3State_eSPI, RstBState, pwron);
    state = FIELD_DP32(state, DevD3State_eSPI, RefClkOkState, pwron);
    state = FIELD_DP32(state, DevD3State_eSPI, PwrRstBState, pwron);

    set_reg8(s, A_DevD3State_eSPI, state);
}

static uint64_t fch_io_read(void *opaque, hwaddr addr, unsigned size)
{
    FchState *s = FCH(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);
    
    return ldn_le_p(s->storage + addr, size);
}

static void fch_io_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    FchState *s = FCH(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, addr, size, value);
    
    stn_le_p(s->storage + addr, size, value);

    if(range_covers_byte(addr, size, A_DevD3Ctl_eSPI)) {
        update_aoac_espi(s);
    }
}


static const MemoryRegionOps fch_io_ops = {
    .read = fch_io_read,
    .write = fch_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fch_init(Object *obj)
{
}

static void fch_initialize_registers(FchState *s)
{
    /* Signal that the reboot was cause by the user (boot from S5 cold) */
    uint32_t s5_reset_stat = 0xffffffff;
    s5_reset_stat = FIELD_DP32(s5_reset_stat, S5ResetStat, UserRst, 1);
    stl_le_p(s->storage + A_S5ResetStat, s5_reset_stat);
}

static void fch_acpi_pm_update_sci_fn(ACPIREGS *regs)
{
}

static void create_acpi_region(FchState *s)
{
    memory_region_init(&s->acpi_region, OBJECT(s), "fch-acpi", 0x0100);

    acpi_pm_tmr_init(&s->acpi_regs, fch_acpi_pm_update_sci_fn, &s->acpi_region);
    acpi_pm1_evt_init(&s->acpi_regs, fch_acpi_pm_update_sci_fn, &s->acpi_region);
    acpi_pm1_cnt_init(&s->acpi_regs, &s->acpi_region, false, false, 2, false);

    memory_region_add_subregion(&s->regs_region, 0x0800, &s->acpi_region);
}

static void create_acpi_ports(FchState *s)
{
    MemoryRegion *acpi_pio_region = g_malloc(sizeof(*acpi_pio_region));
    memory_region_init_alias(acpi_pio_region, OBJECT(s), "acpi-ports", &s->regs_region, 0x800, 12);
    memory_region_add_subregion(get_system_io(), 0x800, acpi_pio_region);
}

static void fch_realize(DeviceState *dev, Error **errp)
{
    FchState *s = FCH(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->regs_region, OBJECT(s),
                          &fch_io_ops, s,
                          "fch", TOTAL_SIZE);
    sysbus_init_mmio(sbd, &s->regs_region);

    fch_initialize_registers(s);
    create_acpi_region(s);
    create_acpi_ports(s);
}

static void fch_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_realize;
    dc->desc = "Zen FCH";
}

static const TypeInfo fch_type_info = {
    .name = TYPE_FCH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchState),
    .instance_init = fch_init,
    .class_init = fch_class_init,
};

static void fch_register_types(void)
{
    type_register_static(&fch_type_info);
}

type_init(fch_register_types)
