#include "qemu/osdep.h"
#include "hw/misc/unimp.h"
#include "hw/zen/zen-utils.h"

void create_unimplemented_device_generic(MemoryRegion *region, const char *name,
                                         hwaddr base, hwaddr size)
{
    DeviceState *dev = qdev_new(TYPE_UNIMPLEMENTED_DEVICE);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    qdev_prop_set_string(dev, "name", name);
    qdev_prop_set_uint64(dev, "size", size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    memory_region_add_subregion_overlap(region, base, sbd->mmio[0].memory,
                                        -1000);

}

void create_region_with_unimpl(MemoryRegion *region, Object *owner,
                                      const char *name, uint64_t size)
{
    g_autofree char *unimp_name = g_strdup_printf("%s-unimp", name);

    memory_region_init(region, owner, name, size);
    create_unimplemented_device_generic(region, unimp_name, 0, size);
}
