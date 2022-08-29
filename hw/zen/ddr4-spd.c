#include "qemu/osdep.h"
#include "hw/i2c/smbus_slave.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/zen/ddr4-spd.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"

#define TYPE_DDR4_SPD_BANK "ddr4-spd-bank"
OBJECT_DECLARE_SIMPLE_TYPE(Ddr4SpdBankState, DDR4_SPD_BANK)

struct Ddr4SpdBankState {
    SMBusDevice parent_obj;

    int *bank;
};

#define TYPE_DDR4_SPD_DATA "ddr4-spd-data"
OBJECT_DECLARE_SIMPLE_TYPE(Ddr4SpdDataState, DDR4_SPD_DATA)

struct Ddr4SpdDataState {
    SMBusDevice parent_obj;

    int *bank;
    int offset;
    uint8_t data[512];
};

OBJECT_DECLARE_SIMPLE_TYPE(Ddr4SpdState, DDR4_SPD)

struct Ddr4SpdState {
    SysBusDevice parent_obj;
    I2CBus *bus;
    uint8_t address;
    int bank;
};

/*
 * Bank switcher
 */

static void ddr4_spd_bank_realize(DeviceState *dev, Error **errp)
{
    Ddr4SpdBankState *s = DDR4_SPD_BANK(dev);
    assert(s->bank != NULL);
}

static int ddr4_spd_bank_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    Ddr4SpdBankState *s = DDR4_SPD_BANK(dev);

    *s->bank = buf[0];

    return 0;
}

static void ddr4_spd_bank_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    sc->write_data = ddr4_spd_bank_write_data;
    dc->realize = ddr4_spd_bank_realize;
}

static const TypeInfo ddr4_spd_bank_info = {
    .name          = TYPE_DDR4_SPD_BANK,
    .parent        = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(Ddr4SpdBankState),
    .class_init    = ddr4_spd_bank_class_init,
};

/*
 * Data holder
 */

static void ddr4_spd_data_realize(DeviceState *dev, Error **errp)
{
    Ddr4SpdDataState *s = DDR4_SPD_DATA(dev);

    assert(s->bank != NULL);

    memset(s->data, 0, 512);
    s->data[0x02] = 0x0c; /* type: DDR4 */
    s->data[0x12] = 0x07; /* t_CKAVG_min */
}

static int ddr4_spd_data_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    Ddr4SpdDataState *s = DDR4_SPD_DATA(dev);

    assert(len == 1);
    s->offset = buf[0];

    return 0;
}

static uint8_t ddr4_spd_data_receive_byte(SMBusDevice *dev)
{
    Ddr4SpdDataState *s = DDR4_SPD_DATA(dev);

    int addr = *s->bank * 256 + s->offset;
    s->offset++;
    assert(addr >= 0 && addr < 512);

    return s->data[addr];
}

static void ddr4_spd_data_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);

    sc->receive_byte = ddr4_spd_data_receive_byte;
    sc->write_data = ddr4_spd_data_write_data;
    dc->realize = ddr4_spd_data_realize;
}

static const TypeInfo ddr4_spd_data_info = {
    .name          = TYPE_DDR4_SPD_DATA,
    .parent        = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(Ddr4SpdDataState),
    .class_init    = ddr4_spd_data_class_init,
};

/*
 * Composite device
 */

static void ddr4_spd_realize(DeviceState *dev, Error **errp)
{
    Ddr4SpdState *s = DDR4_SPD(dev);

    dev = qdev_new(TYPE_DDR4_SPD_BANK);
    qdev_prop_set_uint8(dev, "address", 0x36);
    DDR4_SPD_BANK(dev)->bank = &s->bank;
    qdev_realize_and_unref(dev, BUS(s->bus), &error_fatal);

    dev = qdev_new(TYPE_DDR4_SPD_BANK);
    qdev_prop_set_uint8(dev, "address", 0x37);
    DDR4_SPD_BANK(dev)->bank = &s->bank;
    qdev_realize_and_unref(dev, BUS(s->bus), &error_fatal);

    dev = qdev_new(TYPE_DDR4_SPD_DATA);
    qdev_prop_set_uint8(dev, "address", s->address);
    DDR4_SPD_DATA(dev)->bank = &s->bank;
    qdev_realize_and_unref(dev, BUS(s->bus), &error_fatal);
}

static Property ddr4_spd_props[] = {
    DEFINE_PROP_UINT8("address", Ddr4SpdState, address, 0),
    DEFINE_PROP_LINK("bus", Ddr4SpdState, bus, TYPE_I2C_BUS, I2CBus*),
    DEFINE_PROP_END_OF_LIST(),
};

static void ddr4_spd_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = ddr4_spd_realize;
    device_class_set_props(dc, ddr4_spd_props);
}

static const TypeInfo ddr4_spd_info = {
    .name          = TYPE_DDR4_SPD,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Ddr4SpdState),
    .class_init    = ddr4_spd_class_init,
};


static void ddr4_spd_register_types(void)
{
    type_register_static(&ddr4_spd_bank_info);
    type_register_static(&ddr4_spd_data_info);
    type_register_static(&ddr4_spd_info);
}


type_init(ddr4_spd_register_types)
