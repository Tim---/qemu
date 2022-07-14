#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "hw/zen/psp-dirty.h"
#include "hw/zen/psp-fuses.h"


/* Config */

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

static void create_config(uint32_t addr)
{
    MemoryRegion *mr = g_malloc0(sizeof(MemoryRegion));
    memory_region_init_io(mr, NULL, &config_ops, NULL, "config", 0x6);
    memory_region_add_subregion(get_system_memory(), addr, mr);
}

void psp_create_config(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        create_config(0x3fa50);
        break;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        create_config(0x4fd50);
        break;
    default:
        break;
    }
}

/* Fuses */

void psp_dirty_fuses(zen_codename codename, DeviceState *dev)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        /* Needed unless the UMC is marked as emulation/simulation */
        psp_fuses_write32(dev, 0xcc, 0x20);
        break;
    default:
        break;
    }
}
