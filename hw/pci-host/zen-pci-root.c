/*
 * AMD PCI root device.
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
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "hw/pci-host/zen-pci-root.h"

/****************************************************************************
 * ZEN host
 */

static void zen_host_realize(DeviceState *dev, Error **errp)
{
    PCIHostState *pci = PCI_HOST_BRIDGE(dev);
    ZenHost *s = ZEN_HOST(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    PCIExpressHost *pex = PCIE_HOST_BRIDGE(dev);

    pcie_host_mmcfg_init(pex, PCIE_MMCFG_SIZE_MAX);
    sysbus_init_mmio(sbd, &pex->mmio);

    pci->bus = pci_register_root_bus(dev, "pcie.0", NULL,
                                     pci_swizzle_map_irq_fn, s, get_system_memory(), get_system_io(), 0, 4, TYPE_PCIE_BUS);

    qdev_realize(DEVICE(&s->zen_root), BUS(pci->bus), &error_fatal);
}

static const char *zen_host_root_bus_path(PCIHostState *host_bridge,
                                          PCIBus *rootbus)
{
    return "0000:00";
}

static void zen_host_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(klass);

    hc->root_bus_path = zen_host_root_bus_path;
    dc->realize = zen_host_realize;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->fw_name = "pci";
}

static void zen_host_initfn(Object *obj)
{
    ZenHost *s = ZEN_HOST(obj);
    ZenRootState *root = &s->zen_root;

    object_initialize_child(obj, "zen_root", root, TYPE_ZEN_ROOT_DEVICE);
    qdev_prop_set_int32(DEVICE(root), "addr", PCI_DEVFN(0, 0));
    qdev_prop_set_bit(DEVICE(root), "multifunction", false);
}

static const TypeInfo zen_pci_host_info = {
    .name       = TYPE_ZEN_HOST,
    .parent     = TYPE_PCIE_HOST_BRIDGE,
    .instance_size = sizeof(ZenHost),
    .instance_init = zen_host_initfn,
    .class_init = zen_host_class_init,
};

/****************************************************************************
 * FCH Root Complex D0:F0
 */

static void zen_pci_root_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->desc = "AMD Zen PCIe host bridge";
    k->vendor_id = PCI_VENDOR_ID_AMD;
    k->device_id = 0x15D0;
    k->revision = 0;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    /*
     * PCI-facing part of the host bridge, not usable without the
     * host-facing part, which can't be device_add'ed, yet.
     */
    dc->user_creatable = false;
}

static const TypeInfo zen_pci_root_info = {
    .name = TYPE_ZEN_ROOT_DEVICE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(ZenRootState),
    .class_init = zen_pci_root_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void zen_pci_register(void)
{
    type_register_static(&zen_pci_root_info);
    type_register_static(&zen_pci_host_info);
}

type_init(zen_pci_register)
