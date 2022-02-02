#ifndef FCH_ACPI_H
#define FCH_ACPI_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/isa/isa.h"

#define TYPE_PSP_DEBUG "psp-debug"
OBJECT_DECLARE_SIMPLE_TYPE(PspDebugState, PSP_DEBUG)

enum psp_debug_current_state {
    PSPD_IDLE,
    PSPD_DEAD_WAIT_NUM,
    PSPD_DEAD_WAIT_DATA,
    PSPD_DEAD_WAIT_END,
    PSPD_READ_STR,
};

struct psp_debug_dead_entry {
    uint16_t which;
    uint16_t major;
    uint32_t minor;
    uint32_t data_a;
    uint32_t data_b;
};

struct PspDebugState {
    /*< private >*/
    ISADevice parent_obj;

    /*< public >*/
    MemoryRegion regs;
    enum psp_debug_current_state state;

    /* Dead state handling */
    struct psp_debug_dead_entry dead_entries[0x20];
    uint32_t dead_num;
    uint32_t dead_idx;

    /* Str state handling */
    char str_buf[0x80];
    uint32_t str_idx;
};


#endif
