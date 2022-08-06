#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/zen/fch-smbus.h"
#include "hw/registerfields.h"
#include "qemu/range.h"
#include "hw/i2c/smbus_master.h"

#define REGS_SIZE 0x20

REG8(STATUS,        0x00)
    FIELD(STATUS, FAILED,           4, 1)
    FIELD(STATUS, BUS_COLLISION,    3, 1)
    FIELD(STATUS, DEVICE_ERR,       2, 1)
    FIELD(STATUS, SMBUS_INT,        1, 1)
    FIELD(STATUS, HOST_BUSY,        0, 1)
REG8(SLAVE_STATUS,  0x01)
    FIELD(SLAVE_STATUS, ALERT_STATUS,   5, 1)
    FIELD(SLAVE_STATUS, SHADOW2_STATUS, 4, 1)
    FIELD(SLAVE_STATUS, SHADOW1_STATUS, 3, 1)
    FIELD(SLAVE_STATUS, SLAVE_STATUS,   2, 1)
    FIELD(SLAVE_STATUS, SLAVE_INIT,     1, 1)
    FIELD(SLAVE_STATUS, SLAVE_BUSY,     0, 1)
REG8(CONTROL,       0x02)
    FIELD(CONTROL, RESET,           7, 1)
    FIELD(CONTROL, START,           6, 1)
    FIELD(CONTROL, SMBUS_PROTOCOL,  2, 3)
    FIELD(CONTROL, KILL,            1, 1)
    FIELD(CONTROL, INT_ENABLE,      0, 1)
REG8(HOST_CMD,      0x03)
REG8(ADDRESS,       0x04)
    FIELD(ADDRESS, ADDR,    1, 7)
    FIELD(ADDRESS, RDWR,    0, 1)
REG8(DATA0,         0x05)
REG8(SLAVE_CONTROL, 0x08)
    FIELD(SLAVE_CONTROL, CLEAR_HOST_SEMAPHORE,  5, 1)
    FIELD(SLAVE_CONTROL, HOST_SEMAPHORE,        4, 1)

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
    SysBusDevice parent_obj;

    MemoryRegion regs_region;
    I2CBus *bus;

    uint8_t status;
    uint8_t slave_status;
    uint8_t control;
    uint8_t host_cmd;
    uint8_t address;
    uint8_t data0;
    uint8_t slave_control;
};

static void fch_smbus_start(FchSmbusState *s)
{
    uint8_t proto = FIELD_EX8(s->control, CONTROL, SMBUS_PROTOCOL);

    uint8_t addr = FIELD_EX8(s->address, ADDRESS, ADDR);
    uint8_t rdwr = FIELD_EX8(s->address, ADDRESS, RDWR);

    int res = 0;


    assert(proto == PROT_BYTE || proto == PROT_BYTE_DATA);
    switch (proto) {
    case PROT_BYTE:
        switch (rdwr) {
        case RDWR_READ:
            res = smbus_receive_byte(s->bus, addr);
            s->data0 = res;
            break;
        case RDWR_WRITE:
            res = smbus_send_byte(s->bus, addr, s->data0);
            break;
        }
        break;
    case PROT_BYTE_DATA:
        switch (rdwr) {
        case RDWR_READ:
            res = smbus_read_byte(s->bus, addr, s->host_cmd);
            s->data0 = res;
            break;
        case RDWR_WRITE:
            res = smbus_write_byte(s->bus, addr, s->host_cmd, s->data0);
            break;
        }
        break;
    }
    if (res < 0) {
        s->status = FIELD_DP8(s->status, STATUS, DEVICE_ERR, 1);
    } else {
        s->status = FIELD_DP8(s->status, STATUS, SMBUS_INT, 1);
    }
}

static uint64_t fch_smbus_io_read(void *opaque, hwaddr addr, unsigned size)
{
    FchSmbusState *s = FCH_SMBUS(opaque);

    switch (addr) {
    case R_STATUS:
        return s->status;
    case R_SLAVE_STATUS:
        return s->slave_status;
    case R_CONTROL:
        return s->control;
    case R_DATA0:
        return s->data0;
    case R_SLAVE_CONTROL:
        return s->slave_control;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                    "%s: Bad read offset 0x%"HWADDR_PRIx"\n",
                    __func__, addr);
        break;
    }
    return 0;
}

static void fch_smbus_io_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    FchSmbusState *s = FCH_SMBUS(opaque);

    switch (addr) {
    case R_STATUS:
        if (FIELD_EX8(value, STATUS, SMBUS_INT)) {
            s->status = FIELD_DP8(s->status, STATUS, SMBUS_INT, 0);
        }
        if (FIELD_EX8(value, STATUS, DEVICE_ERR)) {
            s->status = FIELD_DP8(s->status, STATUS, DEVICE_ERR, 0);
        }
        if (FIELD_EX8(value, STATUS, BUS_COLLISION)) {
            s->status = FIELD_DP8(s->status, STATUS, BUS_COLLISION, 0);
        }
        if (FIELD_EX8(value, STATUS, FAILED)) {
            s->status = FIELD_DP8(s->status, STATUS, FAILED, 0);
        }
        return;
    case R_SLAVE_STATUS:
        /* TODO: if(FIELD_EX8(value, SLAVE_STATUS, SLAVE_INIT)) */

        if (FIELD_EX8(value, SLAVE_STATUS, SLAVE_STATUS)) {
            s->slave_status = FIELD_DP8(s->slave_status, SLAVE_STATUS,
                                        SLAVE_STATUS, 0);
        }
        if (FIELD_EX8(value, SLAVE_STATUS, SHADOW1_STATUS)) {
            s->slave_status = FIELD_DP8(s->slave_status, SLAVE_STATUS,
                                        SHADOW1_STATUS, 0);
        }
        if (FIELD_EX8(value, SLAVE_STATUS, SHADOW2_STATUS)) {
            s->slave_status = FIELD_DP8(s->slave_status, SLAVE_STATUS,
                                        SHADOW2_STATUS, 0);
        }
        if (FIELD_EX8(value, SLAVE_STATUS, ALERT_STATUS)) {
            s->slave_status = FIELD_DP8(s->slave_status, SLAVE_STATUS,
                                        ALERT_STATUS, 0);
        }
        return;
    case R_CONTROL:
        s->control = value;

        if (FIELD_EX8(value, CONTROL, START)) {
            fch_smbus_start(s);
        }

        return;
    case R_HOST_CMD:
        s->host_cmd = value;
        return;
    case R_ADDRESS:
        s->address = value;
        return;
    case R_DATA0:
        s->data0 = value;
        return;
    case R_SLAVE_CONTROL:
        assert(value == 0x10 || value == 0x30);

        if (FIELD_EX8(value, SLAVE_CONTROL, HOST_SEMAPHORE)) {
            s->slave_control = FIELD_DP8(s->slave_control, SLAVE_CONTROL,
                                            HOST_SEMAPHORE, 1);
        }
        if (FIELD_EX8(value, SLAVE_CONTROL, CLEAR_HOST_SEMAPHORE)) {
            s->slave_control = FIELD_DP8(s->slave_control, SLAVE_CONTROL,
                                            HOST_SEMAPHORE, 0);
        }

        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                    "%s: Bad write offset 0x%"HWADDR_PRIx"\n",
                    __func__, addr);
        break;
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
