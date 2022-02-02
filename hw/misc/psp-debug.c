#include "qemu/osdep.h"
#include "qemu/log.h"
#include "sysemu/sysemu.h"
#include "hw/misc/psp-debug.h"
#include "trace.h"

#define WRITE_PORT 0x00
#define READ_PORT  0x04

static void psp_debug_postcode(uint32_t code)
{
    trace_psp_debug_postcode(code);
}

static void psp_debug_dead_entries(uint32_t num, struct psp_debug_dead_entry *entries)
{
    for(int i = 0; i < num; i++) {
        trace_psp_debug_dead_entry(entries[i].which, entries[i].major,
                              entries[i].minor, entries[i].data_a,
                              entries[i].data_b);
    }
}

static uint64_t psp_debug_read(void *opaque, hwaddr offset, unsigned size)
{
    assert(size == 4);
    return 0xdead5555;
}

static void psp_debug_write(void *opaque, hwaddr offset,
                                 uint64_t data, unsigned size)
{
    PspDebugState *s = PSP_DEBUG(opaque);

    assert(offset == WRITE_PORT);
    if(size == 1) {
        assert(s->state == PSPD_READ_STR);
        assert(s->str_idx < sizeof(s->str_buf));
        s->str_buf[s->str_idx] = data;
        s->str_idx++;
        return;
    }

    switch(s->state) {
    case PSPD_IDLE:
        if(data == 0xdeadeaaa) {
            s->state = PSPD_DEAD_WAIT_NUM;
        } else if (data == 0x5f535452) {
            /* "_STR" */
            s->state = PSPD_READ_STR;
            s->str_idx = 0;
        } else {
            psp_debug_postcode(data);
        }
        break;
    case PSPD_DEAD_WAIT_NUM:
        assert(data <= 0x20);
        s->dead_num = data;
        s->dead_idx = 0;
        s->state = PSPD_DEAD_WAIT_DATA;
        break;
    case PSPD_DEAD_WAIT_DATA:
        assert(s->dead_idx < s->dead_num * sizeof(struct psp_debug_dead_entry) / sizeof(uint32_t));
        ((uint32_t *)(s->dead_entries))[s->dead_idx] = data;
        s->dead_idx += 1;
        if(s->dead_idx == s->dead_num * sizeof(struct psp_debug_dead_entry) / sizeof(uint32_t))
            s->state = PSPD_DEAD_WAIT_END;
        break;
    case PSPD_DEAD_WAIT_END:
        assert(data == 0xdeadca5e);
        psp_debug_dead_entries(s->dead_num, s->dead_entries);
        s->state = PSPD_IDLE;
        break;
    case PSPD_READ_STR:
        /* _END */
        assert(data == 0x5f454e44);
        trace_psp_debug_str(s->str_idx, s->str_buf);
        s->state = PSPD_IDLE;
        break;
    }
}

/* Note that the firmware could also send the log entries in smaller chunks */
const MemoryRegionOps psp_debug_ops = {
    .read = psp_debug_read,
    .write = psp_debug_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

static void psp_debug_init(Object *obj)
{
    PspDebugState *s = PSP_DEBUG(obj);

    memory_region_init_io(&s->regs, OBJECT(s), &psp_debug_ops, s,
                          "psp-debug-regs", 0x8);
}

static void psp_debug_realize(DeviceState *dev, Error **errp)
{
    PspDebugState *s = PSP_DEBUG(dev);
    ISADevice *isadev = ISA_DEVICE(dev);

    isa_register_ioport(isadev, &s->regs, 0x80);

}

static void psp_debug_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = psp_debug_realize;
    dc->desc = "AMD PSP debug port";
}

static const TypeInfo psp_debug_type_info = {
    .name = TYPE_PSP_DEBUG,
    .parent = TYPE_ISA_DEVICE,
    .instance_size = sizeof(PspDebugState),
    .instance_init = psp_debug_init,
    .class_init = psp_debug_class_init,
};

static void psp_debug_register_types(void)
{
    type_register_static(&psp_debug_type_info);
}

type_init(psp_debug_register_types)
