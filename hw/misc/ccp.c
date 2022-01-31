/*
 * PSP CCP (cryptographic processor).
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
#include "hw/pci/pci.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "qemu/qemu-print.h"
#include "hw/irq.h"
#include "hw/register.h"

#include "hw/misc/ccp.h"
#include "ccp-internal.h"
#include "hw/misc/ccp-ops.h"

REG32(QUEUE_MASK,           0x0000)
REG32(QUEUE_PRIO,           0x0004)
REG32(REQID_CONFIG,         0x0008)
REG32(TRNG_OUT,             0x000C)
REG32(LSB_PUBLIC_MASK_LO,   0x0018)
REG32(LSB_PUBLIC_MASK_HI,   0x001C)
REG32(LSB_PRIVATE_MASK_LO,  0x0020)
REG32(LSB_PRIVATE_MASK_HI,  0x0024)

REG32(CONFIG_0,             0x6000)
REG32(TRNG_CTL,             0x6008)
REG32(ZLIB_MAX_SIZE,        0x6020)
REG32(REG_6024,             0x6024)
REG32(REG_6028,             0x6028)
REG32(ZLIB_SIZE,            0x602C)
REG32(CLK_GATE_CTL,         0x603C)
REG32(REG_6054,             0x6054)

REG32(Q_CONTROL,            0x000)
    FIELD(Q_CONTROL,    RUN,        0,  1)
    FIELD(Q_CONTROL,    HALT,       1,  1)
    FIELD(Q_CONTROL,    QUEUE_SIZE, 3,  5)
REG32(Q_TAIL_LO,            0x004)
REG32(Q_HEAD_LO,            0x008)
REG32(Q_INT_ENABLE,         0x00C)
REG32(Q_INTERRUPT_STATUS,   0x010)
REG32(Q_STATUS,             0x100)
REG32(Q_INT_STATUS,         0x104)

#define INT_COMPLETION          1
#define INT_ERROR               2
#define SUPPORTED_INTERRUPTS    (INT_COMPLETION | INT_ERROR)

static void check_sanity(ccp_queue_t *queue)
{
    assert((queue->head & 0x1f) == 0);
    assert((queue->tail & 0x1f) == 0);

    uint32_t queue_size = 2 << FIELD_EX32(queue->control, Q_CONTROL, QUEUE_SIZE);
    uint32_t head_idx = (queue->head - queue->base) >> 5;
    uint32_t tail_idx = (queue->tail - queue->base) >> 5;

    assert(head_idx <= queue_size);
    assert(tail_idx <= queue_size);
}

static int ccp_execute_command(CcpState *s, struct ccp5_desc *desc)
{
    switch (desc->dw0.engine) {
    case CCP_ENGINE_AES:
        return ccp_op_aes(s, desc);
    case CCP_ENGINE_PASSTHRU:
        return ccp_op_passthru(s, desc);
    case CCP_ENGINE_RSA:
        return ccp_op_rsa(s, desc);
    case CCP_ENGINE_SHA:
        return ccp_op_sha(s, desc);
    case CCP_ENGINE_ZLIB_DECOMPRESS:
        return ccp_op_zlib(s, desc);
    default:
        g_assert_not_reached();
    }
}

static void ccp_run(CcpState *s, ccp_queue_t *queue)
{
    if (FIELD_EX32(queue->control, Q_CONTROL, RUN) == 0) {
        return; /* Not running */
    }

    check_sanity(queue);

    uint32_t queue_size = 2 << FIELD_EX32(queue->control, Q_CONTROL, QUEUE_SIZE);
    uint32_t head_idx = (queue->head - queue->base) >> 5;
    uint32_t tail_idx = (queue->tail - queue->base) >> 5;

    while (head_idx != tail_idx) {
        struct ccp5_desc desc;
        MemTxResult mem_res = address_space_read(&address_space_memory, queue->head, MEMTXATTRS_UNSPECIFIED, &desc, sizeof(desc));
        assert(mem_res == MEMTX_OK);

        int res = ccp_execute_command(s, &desc);

        /* Handle interrupt */
        if (desc.dw0.ioc) {
            int int_num = res ? INT_ERROR : INT_COMPLETION;
            if (queue->int_enable & int_num) {
                queue->interrupt_status |= int_num;
                qemu_irq_raise(s->irq);
            }
        }

        /* Update head */
        head_idx = (head_idx + 1) % queue_size;
        queue->head = queue->base + head_idx * 0x20;

        /* Stop if requested */
        if (desc.dw0.soc) {
            queue->control = FIELD_DP32(queue->control, Q_CONTROL, RUN, 0);
            queue->control = FIELD_DP32(queue->control, Q_CONTROL, HALT, 1);
            break;
        }
    }
}

static uint64_t
ccp_mmio_read_generic(CcpState *s, hwaddr addr)
{
    switch (addr) {
    case A_QUEUE_MASK:
        return (1 << MAX_HW_QUEUES) - 1; /* 5 queues */
    case A_QUEUE_PRIO:
        return s->queue_prio;
    case A_TRNG_OUT:
        return 0;
    case A_LSB_PRIVATE_MASK_LO:
        return 0x39ce0000;
    case A_LSB_PRIVATE_MASK_HI:
        return 0x0000039c;
    case A_CLK_GATE_CTL:
        return s->clk_gate_ctl;
    case A_ZLIB_SIZE:
        return s->zlib_size;
    case A_REG_6028:
        return 1;
    case A_REG_6054:
        return s->reg_5064;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(addr 0x%lx)\n", __func__, addr);
    return 0;
}

static void
ccp_mmio_write_generic(CcpState *s, hwaddr addr, uint64_t val)
{
    switch (addr) {
    case A_QUEUE_PRIO:
        s->queue_prio = val;
        return;
    case A_REQID_CONFIG:
        /* assert(val == 0); */
        return;
    case A_LSB_PUBLIC_MASK_LO:
        /* assert(val == 0x39ce0000); */
        return;
    case A_LSB_PUBLIC_MASK_HI:
        /* assert(val == 0x0000039c); */
        return;
    case A_CONFIG_0:
        assert(val == 1);
        return;
    case A_TRNG_CTL:
        assert(val == 0x12d57);
        return;
    case A_CLK_GATE_CTL:
        s->clk_gate_ctl = val;
        return;
    case A_ZLIB_MAX_SIZE:
        s->zlib_max_size = val;
        return;
    case A_REG_6024:
        assert(val == 0);
        return;
    case A_REG_6054:
        s->reg_5064 = val;
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(addr 0x%lx, value 0x%lx)\n", __func__, addr, val);
}

static uint64_t
ccp_mmio_read_queue(CcpState *s, ccp_queue_t *queue, hwaddr offset)
{
    switch (offset) {
    case A_Q_CONTROL:
        return queue->control;
    case A_Q_STATUS:
        return queue->status;
    case A_Q_INT_STATUS:
        return queue->int_status;
    case A_Q_INTERRUPT_STATUS:
        return queue->interrupt_status;
    case A_Q_HEAD_LO:
        return queue->head;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(queue %x, offset 0x%lx)\n", __func__, queue->num, offset);
    return 0;
}

static void
ccp_mmio_write_queue(CcpState *s, ccp_queue_t *queue, hwaddr offset,
                     uint64_t val)
{
    switch (offset) {
    case A_Q_CONTROL:
        queue->control = val;
        ccp_run(s, queue);
        return;
    case A_Q_TAIL_LO:
        queue->tail = val;
        ccp_run(s, queue);
        return;
    case A_Q_HEAD_LO:
        /* Assume that the head is only written to set the base address */
        queue->base = val;
        queue->head = val;
        ccp_run(s, queue);
        return;
    case A_Q_INT_ENABLE:
        assert((val & ~SUPPORTED_INTERRUPTS) == 0);
        queue->int_enable = val;
        return;
    case A_Q_INTERRUPT_STATUS:
        /* Clear interrupts */
        queue->interrupt_status &= ~val;
        if (!queue->interrupt_status) {
            qemu_irq_lower(s->irq);
        }
        return;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(queue %x, offset 0x%lx, value 0x%lx)\n", __func__,
                  queue->num, offset, val);
}

static uint64_t
ccp_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    CcpState *s = CCP(opaque);

    if (addr >= 0x1000 && addr < 0x6000) {
        return ccp_mmio_read_queue(s, &s->queues[(addr >> 12) - 1], addr & 0xfff);
    } else {
        return ccp_mmio_read_generic(s, addr);
    }
    return 0;
}

static void
ccp_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    CcpState *s = CCP(opaque);

    if (addr >= 0x1000 && addr < 0x6000) {
        ccp_mmio_write_queue(s, &s->queues[(addr >> 12) - 1], addr & 0xfff, val);
    } else {
        ccp_mmio_write_generic(s, addr, val);
    }
}

static const MemoryRegionOps mmio_ops = {
    .read = ccp_mmio_read,
    .write = ccp_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void ccp_instance_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    CcpState *s = CCP(obj);

    memory_region_init_io(&s->mmio, OBJECT(s), &mmio_ops, s, "ccp-mmio", 0x10000);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);
}

static void ccp_realize(DeviceState *dev, Error **errp)
{
    CcpState *s = CCP(dev);

    for (int i = 0; i < MAX_HW_QUEUES; i++) {
        s->queues[i].num = i;
        s->queues[i].control = FIELD_DP32(s->queues[i].control, Q_CONTROL, HALT, 1);
    }
}

static void ccp_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);

    dc->realize = ccp_realize;
    dc->desc = "PSP CCP";
}

static const TypeInfo ccp_info = {
    .name = TYPE_CCP,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CcpState),
    .class_init = ccp_class_init,
    .instance_init = ccp_instance_init,
};

static void ccp_register_types(void)
{
    type_register_static(&ccp_info);
}

type_init(ccp_register_types)

