#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/fch-smbus.h"
#include "hw/registerfields.h"
#include "qemu/range.h"
#include "hw/i2c/smbus_master.h"

#define REGS_SIZE 0x20

REG8(STATUS,        0x00)
    FIELD(STATUS, DEVICE_ERR, 2, 1)
    FIELD(STATUS, SMBUS_INT, 1, 1)
REG8(SLAVE_STATUS,  0x01)
REG8(CONTROL,       0x02)
    FIELD(CONTROL, START,           6, 1)
    FIELD(CONTROL, SMBUS_PROTOCOL,  2, 3)
REG8(HOST_CMD,      0x03)
REG8(ADDRESS,       0x04)
    FIELD(ADDRESS, ADDR,            1, 7)
    FIELD(ADDRESS, RDWR,            0, 1)
REG8(DATA0,         0x05)
REG8(SLAVE_CONTROL, 0x08)

typedef enum {
    PROT_BYTE       = 1,
    PROT_BYTE_DATA  = 2,
} SmbusProtocol;

typedef enum {
    RDWR_WRITE = 0,
    RDWR_READ  = 1,
} SmbusRdWr;

OBJECT_DECLARE_SIMPLE_TYPE(FchSmbusState, FCH_SMBUS)

struct FchSmbusState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    uint8_t storage[REGS_SIZE];
    I2CBus *bus;
};

static inline uint8_t get_reg8(FchSmbusState *s, hwaddr offset)
{
    return ldub_p(s->storage + offset);
}

static inline void set_reg8(FchSmbusState *s, hwaddr offset, uint8_t value)
{
    stb_p(s->storage + offset, value);
}

static void smbus_control(FchSmbusState *s)
{
    uint8_t control = get_reg8(s, A_CONTROL);

    if(FIELD_EX8(control, CONTROL, START) == 0) {
        return;
    }

    uint8_t address = get_reg8(s, A_ADDRESS);
    uint8_t host_cmd = get_reg8(s, A_HOST_CMD);
    uint8_t data0 = get_reg8(s, A_DATA0);
    uint8_t addr = FIELD_EX8(address, ADDRESS, ADDR);
    SmbusRdWr rdwr = FIELD_EX8(address, ADDRESS, RDWR);

    SmbusProtocol proto = FIELD_EX8(control, CONTROL, SMBUS_PROTOCOL);
    int res;

    switch (proto) {
    case PROT_BYTE:
        switch (rdwr) {
        case RDWR_READ:
            res = smbus_receive_byte(s->bus, addr);
            set_reg8(s, A_DATA0, res);
            break;
        case RDWR_WRITE:
            res = smbus_send_byte(s->bus, addr, data0);
            break;
        }
        break;
    case PROT_BYTE_DATA:
        switch (rdwr) {
        case RDWR_READ:
            res = smbus_read_byte(s->bus, addr, host_cmd);
            set_reg8(s, A_DATA0, res);
            break;
        case RDWR_WRITE:
            res = smbus_write_byte(s->bus, addr, host_cmd, data0);
            break;
        }
        break;
    default:
        g_assert_not_reached();
    }

    uint8_t status = get_reg8(s, A_STATUS);
    if (res < 0) {
        status = FIELD_DP8(status, STATUS, DEVICE_ERR, 1);
    } else {
        status = FIELD_DP8(status, STATUS, SMBUS_INT, 1);
    }
    set_reg8(s, A_STATUS, status);
}

static uint64_t fch_smbus_io_read(void *opaque, hwaddr addr, unsigned size)
{
    FchSmbusState *s = FCH_SMBUS(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);

    return ldn_le_p(s->storage + addr, size);
}

static void fch_smbus_io_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    FchSmbusState *s = FCH_SMBUS(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write  "
                "(offset 0x%lx, size 0x%x, value 0x%lx)\n", __func__, addr, size, value);

    stn_le_p(s->storage + addr, size, value);

    if(range_covers_byte(addr, size, A_CONTROL)) {
        smbus_control(s);
    }
}

static const MemoryRegionOps fch_smbus_io_ops = {
    .read = fch_smbus_io_read,
    .write = fch_smbus_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fch_smbus_init(Object *obj)
{
}

static void fch_smbus_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    FchSmbusState *s = FCH_SMBUS(dev);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_smbus_io_ops, s,
                          "fch-smbus-regs", REGS_SIZE);

    sysbus_add_io(sbd, 0xb00, &s->regs_region);
    sysbus_init_ioports(sbd, 0xb00, REGS_SIZE);

    s->bus = i2c_init_bus(DEVICE(s), "smbus");
}

static void fch_smbus_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_smbus_realize;
    dc->desc = "FCH SMBus";
}

static const TypeInfo fch_smbus_type_info = {
    .name = TYPE_FCH_SMBUS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchSmbusState),
    .instance_init = fch_smbus_init,
    .class_init = fch_smbus_class_init,
};

static void fch_smbus_register_types(void)
{
    type_register_static(&fch_smbus_type_info);
}

type_init(fch_smbus_register_types)
