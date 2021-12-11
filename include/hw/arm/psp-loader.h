#ifndef PSP_LOADER_H
#define PSP_LOADER_H

typedef enum {
    PSP_LOAD_BOOTLOADER,
    PSP_LOAD_SECURED_OS,
} psp_load_type_e;

bool psp_load(const char *filename, psp_load_type_e load_type, uint32_t target_id);

#endif
