#ifndef CCP_H
#define CCP_H

#define TYPE_CCP "ccp"
OBJECT_DECLARE_SIMPLE_TYPE(CcpState, CCP)

#define MAX_HW_QUEUES           5

typedef struct {
    int num;
    uint32_t status;
    uint32_t int_status;
    uint32_t tail;
    uint32_t head;
    uint32_t base;
    uint32_t control;
    uint32_t interrupt_status;
    uint32_t int_enable;
} ccp_queue_t;


struct CcpState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    qemu_irq irq;

    uint32_t queue_prio;
    uint32_t clk_gate_ctl;
    uint32_t reg_5064;

    ccp_queue_t queues[MAX_HW_QUEUES];
    uint8_t sb[0x20 * 0x80];
};

#endif /* CCP_H */
