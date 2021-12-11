/*
 * PSP CCP (cryptographic processor) PCI device.
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
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "qemu/qemu-print.h"

#include "hw/misc/ccp-pci.h"

#define CCP_PCI_MMIO_IDX    2
#define CCP_PCI_MMIO_SIZE   (1 * MiB)

static void ccp_pci_int_handler(void *opaque, int irq, int level)
{
    PCIDevice *s = PCI_DEVICE(opaque);

    if(level && msi_enabled(s)) {
        msi_notify(s, 0);
    }
}

static void ccp_pci_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    CcpPciState *s = CCP_PCI(pci_dev);

    qdev_init_gpio_in(DEVICE(pci_dev), ccp_pci_int_handler, 1);

    memory_region_init(&s->mmio, OBJECT(s), "ccp-pci-mmio", 0x100000);
    memory_region_add_subregion(&s->mmio, 0, sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->ccp), 0));

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->ccp), errp))
        return;
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->ccp), 0, qdev_get_gpio_in(DEVICE(pci_dev), 0));
    pci_register_bar(pci_dev, CCP_PCI_MMIO_IDX, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    if (pcie_endpoint_cap_init(pci_dev, 0x64) < 0) {
        hw_error("Failed to initialize PCIe capability");
    }

    if(msi_init(PCI_DEVICE(s), 0xa0, 1, true, false, NULL)) {
        hw_error("Failed to initialize MSI");
    }
}

static void ccp_pci_pci_uninit(PCIDevice *pci_dev)
{
}

static void ccp_pci_qdev_reset(DeviceState *dev)
{
}

static void ccp_pci_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    PCIDeviceClass *c = PCI_DEVICE_CLASS(class);

    c->realize = ccp_pci_pci_realize;
    c->exit = ccp_pci_pci_uninit;
    c->vendor_id = PCI_VENDOR_ID_AMD;
    c->device_id = 0x1456;
    c->revision = 1;
    c->class_id = PCI_CLASS_CRYPT_OTHER;

    dc->desc = "AMD CCP PCI";
    dc->reset = ccp_pci_qdev_reset;
}

static void ccp_pci_instance_init(Object *obj)
{
    CcpPciState *s = CCP_PCI(obj);

    object_initialize_child(obj, "ccp", &s->ccp, TYPE_CCP);
}


static const TypeInfo ccp_pci_info = {
    .name = TYPE_CCP_PCI,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(CcpPciState),
    .class_init = ccp_pci_class_init,
    .instance_init = ccp_pci_instance_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void ccp_pci_register_types(void)
{
    type_register_static(&ccp_pci_info);
}

type_init(ccp_pci_register_types)

