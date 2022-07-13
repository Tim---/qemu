#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "hw/zen/psp-dirty.h"

/*
typedef struct {
    uint8_t PhysDieId;
    uint8_t SocketId;
    uint8_t PackageType;
    uint8_t SystemSocketCount;
    uint8_t unknown0;
    uint8_t DiesPerSocket;
} PspBootRomServices_t;
*/
static uint64_t config_read(void *opaque, hwaddr addr, unsigned size)
{
    switch(addr) {
    case 3:
    case 5:
        return 1;
    }
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(offset 0x%lx, size 0x%x)\n", __func__, addr, size);
    return 0;
}

static void config_write(void *opaque, hwaddr addr,
                                uint64_t value, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device write "
                  "(offset 0x%lx, size 0x%x, value 0x%lx)\n",
                  __func__, addr, size, value);
}

static const MemoryRegionOps config_ops = {
    .read = config_read,
    .write = config_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


void psp_create_config(zen_codename codename)
{
    switch(codename) {
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        MemoryRegion *mr = g_malloc0(sizeof(MemoryRegion));
        memory_region_init_io(mr, NULL, &config_ops, NULL, "config", 0x6);
        memory_region_add_subregion(get_system_memory(), 0x4FD50, mr);
        break;
    default:
        break;
    }
}
