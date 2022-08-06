#include "qemu/osdep.h"
#include "hw/i2c/smbus_slave.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/zen/ddr4-spd.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"

const uint8_t sample_eeprom[] = {
    0x23, 0x11, 0x0c, 0x02, 0x84, 0x19, 0x00, 0x08, 0x00, 0x00, 0x00, 0x03, 0x09, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x0d, 0xfc, 0x2b, 0x00, 0x00, 0x6b, 0x6b, 0x6b, 0x11, 0x00, 0x6b, 0x20, 0x08,
    0x00, 0x05, 0x70, 0x03, 0x00, 0xa8, 0x1b, 0x28, 0x28, 0x00, 0x78, 0x00, 0x14, 0x3c, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x36, 0x16, 0x36,
    0x16, 0x36, 0x16, 0x36, 0x00, 0x20, 0x2b, 0x0c, 0x2b, 0x0c, 0x2b, 0x0c, 0x2b, 0x0c, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xed, 0x9d, 0xb5, 0xca, 0xca, 0xca, 0xca, 0xe7, 0xd6, 0xf6, 0x6f,
    0x11, 0x11, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xde, 0x27,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x85, 0x9b, 0x00, 0x00, 0x00, 0x52, 0x71, 0x00, 0x5a, 0x42, 0x4c, 0x53, 0x38, 0x47, 0x34, 0x44,
    0x32, 0x34, 0x30, 0x46, 0x53, 0x45, 0x2e, 0x31, 0x36, 0x46, 0x42, 0x44, 0x32, 0x00, 0x80, 0x2c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x4a, 0x05, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x07, 0xff, 0x3f, 0x00,
    0x00, 0x6a, 0x6a, 0x6a, 0x11, 0x00, 0x6b, 0x20, 0x08, 0x00, 0x05, 0x70, 0x03, 0x00, 0xa8, 0x1b,
    0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9d, 0xb5, 0xca, 0xca, 0xca, 0xca, 0xd6,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

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

    memcpy(s->data, sample_eeprom, 512);
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
    assert(addr >= 0 && addr < 512);

    return s->data[s->offset++];
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
    qdev_prop_set_uint8(dev, "address", 0x50);
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
