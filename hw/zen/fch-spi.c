#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "hw/registerfields.h"
#include "qemu/range.h"
#include "hw/ssi/ssi.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "hw/zen/fch-spi.h"

//#define DEBUG_FCH_SPI

#ifdef DEBUG_FCH_SPI
#define DPRINTF(fmt, ...) \
    do { qemu_log_mask(LOG_UNIMP, "%s: " fmt, __func__, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

#define REGS_SIZE 0x20000

OBJECT_DECLARE_SIMPLE_TYPE(FchSpiState, FCH_SPI)

struct FchSpiState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion regs_region;
    MemoryRegion direct_access;
    uint8_t storage[REGS_SIZE];
    SSIBus *spi;

    uint8_t tx_byte_count;
    uint8_t rx_byte_count;

    qemu_irq cs[1];
};

REG32(SPI_CNTRL0,       0x00)
    FIELD(SPI_CNTRL0, FIFO_PTR_CLR,             20, 1)
    FIELD(SPI_CNTRL0, EXECUTE_OPCODE,           16, 1)
    FIELD(SPI_CNTRL0, SPI_OPCODE,                0, 8)
REG32(SPI_CNTRL1,       0x0C)
    FIELD(SPI_CNTRL1, FIFO_PTR,                  8, 3)
    FIELD(SPI_CNTRL1, SPI_PARAMETERS,            0, 8)
REG8(SPI_EXT_REG_INDX,  0x1E)
REG8(SPI_EXT_REG_DATA,  0x1F)
REG8(SPI_CMD_CODE,      0x45)
REG8(SPI_CMD_TRIGGER,   0x47)
    FIELD(SPI_CMD_TRIGGER, EXECUTE,              7, 1)
REG8(SPI_TX_BYTE_COUNT, 0x48)
REG8(SPI_RX_BYTE_COUNT, 0x4B)
REG32(SPI_STATUS,       0x4C)
    FIELD(SPI_STATUS, FIFO_WR_PTR,               8, 6)
    FIELD(SPI_STATUS, FIFO_RD_PTR,              16, 6)
#define SPI_FIFO_SIZE 70
REG8(SPI_FIFO,          0x80)

REG8(ESPI_MASTER_CAP,   0x1002C)

static inline uint8_t get_reg8(FchSpiState *s, hwaddr offset)
{
    return ldub_p(s->storage + offset);
}

static inline void set_reg8(FchSpiState *s, hwaddr offset, uint8_t value)
{
    stb_p(s->storage + offset, value);
}

static inline uint32_t get_reg32(FchSpiState *s, hwaddr offset)
{
    return ldl_le_p(s->storage + offset);
}

static inline void set_reg32(FchSpiState *s, hwaddr offset, uint32_t value)
{
    stl_le_p(s->storage + offset, value);
}

DeviceState *fch_spi_add_flash(DeviceState *fch_spi, BlockBackend *blk, const char *flash_type)
{
    FchSpiState *s = FCH_SPI(fch_spi);

    qemu_irq cs_line;
    DeviceState *dev;
    BusState *bus = BUS(s->spi);

    dev = qdev_new(flash_type);
    qdev_prop_set_drive(dev, "drive", blk);
    qdev_realize_and_unref(dev, bus, &error_fatal);

    cs_line = qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0);
    sysbus_connect_irq(SYS_BUS_DEVICE(s), 0, cs_line);

    return dev;
}

enum {
    EXT_TX_BYTE_COUNT = 0x05,
    EXT_RX_BYTE_COUNT = 0x06,
};

static void fch_spi_ext_write(FchSpiState *s)
{
    uint8_t ext_indx = get_reg8(s, A_SPI_EXT_REG_INDX);
    uint8_t ext_data = get_reg8(s, A_SPI_EXT_REG_DATA);

    switch(ext_indx) {
    case EXT_TX_BYTE_COUNT:
        set_reg8(s, A_SPI_TX_BYTE_COUNT, ext_data);
        break;
    case EXT_RX_BYTE_COUNT:
        set_reg8(s, A_SPI_RX_BYTE_COUNT, ext_data);
        break;
    }
}

static void fch_spi_execute2(FchSpiState *s)
{
    uint8_t cmd_code = get_reg8(s, A_SPI_CMD_CODE);
    uint8_t cmd_trigger = get_reg8(s, A_SPI_CMD_TRIGGER);
    uint8_t rx_count = get_reg8(s, A_SPI_RX_BYTE_COUNT);
    uint8_t tx_count = get_reg8(s, A_SPI_TX_BYTE_COUNT);
    uint8_t status = get_reg32(s, A_SPI_STATUS);
    uint8_t *fifo = s->storage + A_SPI_FIFO;

    if(FIELD_EX8(cmd_trigger, SPI_CMD_TRIGGER, EXECUTE) == 0)
        return;
    cmd_trigger = FIELD_DP8(cmd_trigger, SPI_CMD_TRIGGER, EXECUTE, 0);
    set_reg8(s, A_SPI_CMD_TRIGGER, cmd_trigger);

    qemu_irq_lower(s->cs[0]);

    ssi_transfer(s->spi, cmd_code);

    int fifo_wr_ptr = FIELD_EX32(status, SPI_STATUS, FIFO_WR_PTR);
    int fifo_rd_ptr = FIELD_EX32(status, SPI_STATUS, FIFO_RD_PTR);

    for(int i = 0; i < tx_count; i++) {
        ssi_transfer(s->spi, fifo[fifo_wr_ptr]);
        fifo_wr_ptr = (fifo_wr_ptr + 1) % SPI_FIFO_SIZE;
    }

    fifo_rd_ptr = fifo_wr_ptr;

    for(int i = 0; i < rx_count; i++) {
        fifo[fifo_rd_ptr] = ssi_transfer(s->spi, 0);
        fifo_rd_ptr = (fifo_rd_ptr + 1) % SPI_FIFO_SIZE;
    }

    status = FIELD_DP32(status, SPI_STATUS, FIFO_WR_PTR, fifo_wr_ptr);
    status = FIELD_DP32(status, SPI_STATUS, FIFO_RD_PTR, fifo_rd_ptr);
    set_reg32(s, A_SPI_STATUS, status);

    qemu_irq_raise(s->cs[0]);
}

static void fch_spi_execute(FchSpiState *s)
{
    uint32_t cntrl0 = get_reg32(s, A_SPI_CNTRL0);
    uint32_t cntrl1 = get_reg32(s, A_SPI_CNTRL1);
    uint8_t *fifo = s->storage + A_SPI_FIFO;

    uint8_t rx_count = get_reg8(s, A_SPI_RX_BYTE_COUNT);
    uint8_t tx_count = get_reg8(s, A_SPI_TX_BYTE_COUNT);

    // Clear the fifo pointer
    if(FIELD_EX32(cntrl0, SPI_CNTRL0, FIFO_PTR_CLR) == 1) {
        cntrl1 = FIELD_DP32(cntrl1, SPI_CNTRL1, FIFO_PTR, 0);
        cntrl0 = FIELD_DP32(cntrl0, SPI_CNTRL0, FIFO_PTR_CLR, 0);
    }

    if(FIELD_EX32(cntrl0, SPI_CNTRL0, EXECUTE_OPCODE) == 1) {
        uint8_t op = FIELD_EX32(cntrl0, SPI_CNTRL0, SPI_OPCODE);

        qemu_irq_lower(s->cs[0]);

        ssi_transfer(s->spi, op);

        int fifo_ptr = FIELD_EX32(cntrl1, SPI_CNTRL1, FIFO_PTR);

        for(int i = 0; i < tx_count; i++) {
            ssi_transfer(s->spi, fifo[fifo_ptr]);
            fifo_ptr = (fifo_ptr + 1) % 8;
        }

        for(int i = 0; i < rx_count; i++) {
            fifo[fifo_ptr] = ssi_transfer(s->spi, 0);
            fifo_ptr = (fifo_ptr + 1) % 8;
        }

        qemu_irq_raise(s->cs[0]);

        cntrl1 = FIELD_DP32(cntrl1, SPI_CNTRL1, FIFO_PTR, fifo_ptr);
        cntrl0 = FIELD_DP32(cntrl0, SPI_CNTRL0, EXECUTE_OPCODE, 0);
    }

    set_reg32(s, A_SPI_CNTRL0, cntrl0);
    set_reg32(s, A_SPI_CNTRL1, cntrl1);
}

static void fch_spi_fifo_read(FchSpiState *s)
{
    uint32_t cntrl1 = get_reg32(s, A_SPI_CNTRL1);
    int fifo_ptr = FIELD_EX32(cntrl1, SPI_CNTRL1, FIFO_PTR);
    uint8_t *fifo = s->storage + A_SPI_FIFO;

    uint8_t data = fifo[fifo_ptr];
    fifo_ptr = (fifo_ptr + 1) % 8;

    cntrl1 = FIELD_DP32(cntrl1, SPI_CNTRL1, SPI_PARAMETERS, data);
    cntrl1 = FIELD_DP32(cntrl1, SPI_CNTRL1, FIFO_PTR, fifo_ptr);

    set_reg32(s, A_SPI_CNTRL1, cntrl1);
}

static uint64_t fch_spi_io_read(void *opaque, hwaddr addr, unsigned size)
{
    FchSpiState *s = FCH_SPI(opaque);

    DPRINTF("unimplemented device read  (offset 0x%lx, size 0x%x)\n", addr, size);

    if(range_covers_byte(addr, size, A_SPI_CNTRL1)) {
        fch_spi_fifo_read(s);
    }

    return ldn_le_p(s->storage + addr, size);
}

static void fch_spi_io_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    FchSpiState *s = FCH_SPI(opaque);

    DPRINTF("unimplemented device write (offset 0x%lx, size 0x%x, value 0x%lx)\n", addr, size, value);

    stn_le_p(s->storage + addr, size, value);

    if(range_covers_byte(addr, size, A_SPI_EXT_REG_DATA)) {
        fch_spi_ext_write(s);
    }

    if(ranges_overlap(addr, size, A_SPI_CNTRL0, 4)) {
        fch_spi_execute(s);
    }

    if(range_covers_byte(addr, size, A_SPI_CMD_TRIGGER)) {
        fch_spi_execute2(s);
    }
}

static const MemoryRegionOps fch_spi_io_ops = {
    .read = fch_spi_io_read,
    .write = fch_spi_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static uint64_t fch_spi_da_read(void *opaque, hwaddr offset, unsigned size)
{
    FchSpiState *s = FCH_SPI(opaque);

    qemu_irq_lower(s->cs[0]);

    ssi_transfer(s->spi, 0x03);
    ssi_transfer(s->spi, extract32(offset, 16, 8));
    ssi_transfer(s->spi, extract32(offset,  8, 8));
    ssi_transfer(s->spi, extract32(offset,  0, 8));

    uint64_t res = 0;

    for(int i = 0; i < size; i++) {
        res = deposit64(res, 8 * i, 8, ssi_transfer(s->spi, 0));
    }

    qemu_irq_raise(s->cs[0]);

    return res;
}

static void fch_spi_da_write(void *opaque, hwaddr offset,
                          uint64_t data, unsigned size)
{
    g_assert_not_reached();
}

const MemoryRegionOps fch_spi_da_ops = {
    .read = fch_spi_da_read,
    .write = fch_spi_da_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .valid.unaligned = true,
};

static void fch_spi_init(Object *obj)
{
}

static void fch_spi_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    FchSpiState *s = FCH_SPI(dev);

    /* Init the registers access */
    memory_region_init_io(&s->regs_region, OBJECT(s), &fch_spi_io_ops, s,
                          "fch-spi-regs", REGS_SIZE);
    sysbus_init_mmio(sbd, &s->regs_region);

    memory_region_init_io(&s->direct_access, OBJECT(s), &fch_spi_da_ops, s,
                          "fch-spi-direct-access", 0x01000000);
    sysbus_init_mmio(sbd, &s->direct_access);

    s->spi = ssi_create_bus(DEVICE(s), "spi");
    sysbus_init_irq(sbd, &s->cs[0]);

    set_reg32(s, A_ESPI_MASTER_CAP, 1);
}

static void fch_spi_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = fch_spi_realize;
    dc->desc = "FCH SPI";
}

static const TypeInfo fch_spi_type_info = {
    .name = TYPE_FCH_SPI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FchSpiState),
    .instance_init = fch_spi_init,
    .class_init = fch_spi_class_init,
};

static void fch_spi_register_types(void)
{
    type_register_static(&fch_spi_type_info);
}

type_init(fch_spi_register_types)
