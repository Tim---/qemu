/*
 * FCH SMBUS registers.
 *
 * Copyright (C) 2021 Timothée Cocault <timothee.cocault@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/qdev-properties.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "qemu-common.h"
#include "qemu/bitops.h"
#include "hw/register.h"
#include "hw/i2c/smbus_master.h"

#include "hw/i2c/fch-smbus.h"

#define R_STATUS              0x00
    FIELD(STATUS, HOST_BUSY, 0, 1)
    FIELD(STATUS, SMBUS_INT, 1, 1)
    FIELD(STATUS, DEVICE_ERR, 2, 1)
    FIELD(STATUS, BUS_COLLISION, 3, 1)
    FIELD(STATUS, FAILED, 4, 1)
#define R_SLAVE_STATUS        0x01
    FIELD(SLAVE_STATUS, SLAVE_BUSY, 0, 1)
    FIELD(SLAVE_STATUS, SLAVE_INIT, 1, 1)
    FIELD(SLAVE_STATUS, SLAVE_STATUS, 2, 1)
    FIELD(SLAVE_STATUS, SHADOW1_STATUS, 3, 1)
    FIELD(SLAVE_STATUS, SHADOW2_STATUS, 4, 1)
    FIELD(SLAVE_STATUS, ALERT_STATUS, 5, 1)
#define R_CONTROL             0x02
    FIELD(CONTROL, INT_ENABLE, 0, 1)
    FIELD(CONTROL, KILL, 1, 1)
    FIELD(CONTROL, SMBUS_PROTOCOL, 2, 3)
    FIELD(CONTROL, START, 6, 1)
    FIELD(CONTROL, RESET, 7, 1)
#define R_HOST_CMD            0x03
    FIELD(HOST_CMD, HOST_CMD, 0, 8)
#define R_ADDRESS             0x04
    FIELD(ADDRESS, RDWR, 0, 1)
    FIELD(ADDRESS, ADDR, 1, 7)
#define R_DATA0               0x05
#define R_SLAVE_CONTROL       0x08
    FIELD(SLAVE_CONTROL, HOST_SEMAPHORE, 4, 1)
    FIELD(SLAVE_CONTROL, CLEAR_HOST_SEMAPHORE, 5, 1)

#define RDWR_READ       1
#define RDWR_WRITE      0

#define PROT_BYTE       1
#define PROT_BYTE_DATA  2

const char *commands[] = {"write", "read"};


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
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};


static void fch_smbus_realize(DeviceState *dev, Error **errp)
{
    FchSmbusState *s = FCH_SMBUS(dev);
    s->bank = 0;

    qdev_prop_set_uint8(DEVICE(&s->spd_bank0), "address", 0x36);
    s->spd_bank0.bank = &s->bank;
    if (!qdev_realize_and_unref(DEVICE(&s->spd_bank0), (BusState *)s->bus, errp)) {
        return;
    }

    qdev_prop_set_uint8(DEVICE(&s->spd_bank1), "address", 0x37);
    s->spd_bank1.bank = &s->bank;
    if (!qdev_realize_and_unref(DEVICE(&s->spd_bank1), (BusState *)s->bus, errp)) {
        return;
    }

    qdev_prop_set_uint8(DEVICE(&s->spd_data0), "address", 0x50);
    s->spd_data0.bank = &s->bank;
    if (!qdev_realize_and_unref(DEVICE(&s->spd_data0), (BusState *)s->bus, errp)) {
        return;
    }

    qdev_prop_set_uint8(DEVICE(&s->spd_data1), "address", 0x51);
    s->spd_data1.bank = &s->bank;
    if (!qdev_realize_and_unref(DEVICE(&s->spd_data1), (BusState *)s->bus, errp)) {
        return;
    }
}

static void fch_smbus_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    FchSmbusState *s = FCH_SMBUS(obj);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_smbus_io_ops,
                          s, "fch-smbus-regs", 0x100);
    sysbus_init_mmio(sbd, &s->regs_region);

    /* Create the smbus bus */
    s->bus = i2c_init_bus(DEVICE(s), "smbus");

    object_initialize_child(obj, "spd-bank0", &s->spd_bank0, TYPE_FCH_SPD_BANK);
    object_initialize_child(obj, "spd-bank1", &s->spd_bank1, TYPE_FCH_SPD_BANK);
    object_initialize_child(obj, "spd-data0", &s->spd_data0, TYPE_FCH_SPD_DATA);
    object_initialize_child(obj, "spd-data1", &s->spd_data1, TYPE_FCH_SPD_DATA);
}

static void fch_smbus_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = fch_smbus_realize;
}

static const TypeInfo fch_smbus_info = {
    .name          = TYPE_FCH_SMBUS,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchSmbusState),
    .instance_init = fch_smbus_init,
    .class_init    = fch_smbus_class_init,
};


static void fch_smbus_register_types(void)
{
    type_register_static(&fch_smbus_info);
}


type_init(fch_smbus_register_types)
