/*
 * FCH AcpiMmio registers.
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
#include "hw/registerfields.h"
#include "hw/char/serial.h"
#include "sysemu/sysemu.h"
#include "qemu/qemu-print.h"

#include "hw/misc/fch-acpi.h"

/* See "BKDG for AMD Family 15h Models 70h-7Fh Processors" */

#define TOTAL_SIZE  0x2000

#define PMIO_BASE   0x0300
#define PMIO_SIZE   0x0100
REG32(PM_DECODE_EN,     0x00)
REG32(PM_BOOT_TIMER_EN, 0x44)
REG32(PM_ACPI_CONFIG,   0x74)
REG32(PM_S5_RESET_STAT, 0xC0)
REG32(PM_LPC_GATING,    0xEC)

#define ACPI_BASE   0x0800
#define ACPI_SIZE   0x0100
/* Standard registers. Use the already defined hw/acpi/ ? */
REG32(ACPI_PM1_CNT_BLK, 0x04)
    FIELD(ACPI_PM1_CNT_BLK, SCI_EN,     13, 1)
    FIELD(ACPI_PM1_CNT_BLK, BM_RLD,     10, 3)
    FIELD(ACPI_PM1_CNT_BLK, BM_RLS,      2, 1)
    FIELD(ACPI_PM1_CNT_BLK, SLP_TYP,     1, 1)
    FIELD(ACPI_PM1_CNT_BLK, SLP_TYP_EN,  0, 1)

#define SMBUS_BASE  0x0A00
#define SMBUS_SIZE  0x0100
REG32(SMBUS_SLAVE_CONTROL,  0x08)
    FIELD(SMBUS_SLAVE_CONTROL, CLR_EC_SEMAPHORE,    7, 1)
    FIELD(SMBUS_SLAVE_CONTROL, EC_SEMAPHORE,        6, 1)
    FIELD(SMBUS_SLAVE_CONTROL, CLR_HOST_SEMAPHORE,  5, 1)
    FIELD(SMBUS_SLAVE_CONTROL, HOST_SEMAPHORE,      4, 1)
    FIELD(SMBUS_SLAVE_CONTROL, SMBUS_ALERT_EN,      3, 1)
    FIELD(SMBUS_SLAVE_CONTROL, SMBUS_SHADOW2_EN,    2, 1)
    FIELD(SMBUS_SLAVE_CONTROL, SMBUS_SHADOW1_EN,    1, 1)
    FIELD(SMBUS_SLAVE_CONTROL, SLAVE_ENABLE,        0, 1)
REG32(SMBUS_TIMING,         0x0E)

#define IOMUX_BASE  0x0D00
#define IOMUX_SIZE  0x0100
REG32(IOMUX_SPKR_AGPIO91,   0x5b)

#define MISC_BASE   0x0E00
#define MISC_SIZE   0x0100

#define GPIO1_BASE   0x1600
#define GPIO1_SIZE   0x0100

#define AOAC_BASE   0x1e00
#define AOAC_SIZE   0x0100

/*
 * PM registers
 */

static uint64_t fch_acpi_pm_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case A_PM_ACPI_CONFIG:
        return 0;
    case A_PM_S5_RESET_STAT:
        return 0;
    case A_PM_DECODE_EN:
    case A_PM_BOOT_TIMER_EN:
    case A_PM_LPC_GATING:
        break; /* TODO */
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static void fch_acpi_pm_write(void *opaque, hwaddr offset, uint64_t data, unsigned size)
{
    switch (offset) {
    case A_PM_ACPI_CONFIG:
        assert(data == 1); /* Enable ACPI decoding */
        return;
    case A_PM_DECODE_EN:
    case A_PM_BOOT_TIMER_EN:
    case A_PM_LPC_GATING:
        break; /* TODO */
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_acpi_pm_ops = {
    .read = fch_acpi_pm_read,
    .write = fch_acpi_pm_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

/*
 * ACPI registers
 */

static uint64_t fch_acpi_acpi_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case A_ACPI_PM1_CNT_BLK:
        return 0;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static void fch_acpi_acpi_write(void *opaque, hwaddr offset,
                                uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_acpi_acpi_ops = {
    .read = fch_acpi_acpi_read,
    .write = fch_acpi_acpi_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

/*
 * IOMux registers
 */

static uint64_t fch_acpi_iomux_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static void fch_acpi_iomux_write(void *opaque, hwaddr offset,
                                 uint64_t data, unsigned size)
{
    switch(offset) {
    case 0x5b:
    case 0x62:
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_acpi_iomux_ops = {
    .read = fch_acpi_iomux_read,
    .write = fch_acpi_iomux_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

/*
 * MISC registers
 */

static uint64_t fch_acpi_misc_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case 0xd8:
    case 0xdc:
    case 0xe0:
    case 0xe4:
    case 0xe8:
    case 0xec:
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static void fch_acpi_misc_write(void *opaque, hwaddr offset,
                                uint64_t data, unsigned size)
{
    switch(offset) {
    case 0xb0:
    case 0xd8:
    case 0xdc:
    case 0xe0:
    case 0xe4:
    case 0xe8:
    case 0xec:
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_acpi_misc_ops = {
    .read = fch_acpi_misc_read,
    .write = fch_acpi_misc_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

/*
 * GPIO1 registers
 */

static uint64_t fch_acpi_gpio1_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                "(offset 0x%lx)\n",
                __func__, offset);

    return 0;
}

static void fch_acpi_gpio1_write(void *opaque, hwaddr offset,
                                uint64_t data, unsigned size)
{
    switch (offset) {
    case 0x8a:
        break;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_acpi_gpio1_ops = {
    .read = fch_acpi_gpio1_read,
    .write = fch_acpi_gpio1_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

/*
 * AOAC registers
 */

static uint64_t fch_acpi_aoac_read(void *opaque, hwaddr offset, unsigned size)
{
    switch (offset) {
    case 0x77:
        return 7;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);

    return 0;
}

static void fch_acpi_aoac_write(void *opaque, hwaddr offset,
                                uint64_t data, unsigned size)
{
    switch (offset) {
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}

const MemoryRegionOps fch_acpi_aoac_ops = {
    .read = fch_acpi_aoac_read,
    .write = fch_acpi_aoac_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};

/*
 * Unimplemented register banks
 */

static uint64_t fch_acpi_unimp_read(void *opaque, hwaddr offset, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx)\n",
                  __func__, offset);

    return 0;
}

static void fch_acpi_unimp_write(void *opaque, hwaddr offset,
                                 uint64_t data, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, value 0x%lx)\n",
                  __func__, offset, data);
}


const MemoryRegionOps fch_acpi_unimp_ops = {
    .read = fch_acpi_unimp_read,
    .write = fch_acpi_unimp_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
};


static void add_bank(FchAcpiState *s, MemoryRegion *subregion,
                     const MemoryRegionOps *ops, const char *name,
                     hwaddr addr, uint64_t size)
{
    memory_region_init_io(subregion, OBJECT(s), ops, s, name, size);
    memory_region_add_subregion(&s->regs, addr, subregion);
}

static void fch_acpi_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    FchAcpiState *s = FCH_ACPI(obj);

    object_initialize_child(obj, "smbus", &s->smbus, TYPE_FCH_SMBUS);

    memory_region_init(&s->regs, OBJECT(s), "fch-acpi", TOTAL_SIZE);

    add_bank(s, &s->regs_pm,    &fch_acpi_pm_ops,
             "fch-acpi-pm",    PMIO_BASE,  PMIO_SIZE);
    add_bank(s, &s->regs_acpi,  &fch_acpi_acpi_ops,
             "fch-acpi-acpi",  ACPI_BASE,  ACPI_SIZE);
    add_bank(s, &s->regs_iomux, &fch_acpi_iomux_ops,
             "fch-acpi-iomux", IOMUX_BASE, IOMUX_SIZE);
    add_bank(s, &s->regs_misc,  &fch_acpi_misc_ops,
             "fch-acpi-misc",  MISC_BASE,  MISC_SIZE);
    add_bank(s, &s->regs_aoac,  &fch_acpi_aoac_ops,
             "fch-acpi-aoac",  AOAC_BASE,  AOAC_SIZE);
    add_bank(s, &s->regs_gpio1, &fch_acpi_gpio1_ops,
             "fch-acpi-aoac",  GPIO1_BASE, GPIO1_SIZE);

    memory_region_init_io(&s->regs_unimp, OBJECT(s), &fch_acpi_unimp_ops, s,
                          "fch-acpi-unimp", TOTAL_SIZE);
    memory_region_add_subregion_overlap(&s->regs, 0, &s->regs_unimp, -1000);

    sysbus_init_mmio(sbd, &s->regs);

    memory_region_init_alias(&s->regs_alias, OBJECT(s), "fch-acpi", &s->regs,
                             0, TOTAL_SIZE);
    sysbus_init_mmio(sbd, &s->regs_alias);
}

static void fch_acpi_realize(DeviceState *dev, Error **errp)
{
    FchAcpiState *s = FCH_ACPI(dev);

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->smbus), errp)) {
        return;
    }
    memory_region_add_subregion(&s->regs, SMBUS_BASE, sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->smbus), 0));
}

static void fch_acpi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_acpi_realize;
    dc->desc = "AMD FCH ACPI";
}

static const TypeInfo fch_acpi_type_info = {
    .name = TYPE_FCH_ACPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchAcpiState),
    .instance_init = fch_acpi_init,
    .class_init = fch_acpi_class_init,
};

static void fch_acpi_register_types(void)
{
    type_register_static(&fch_acpi_type_info);
}

type_init(fch_acpi_register_types)
