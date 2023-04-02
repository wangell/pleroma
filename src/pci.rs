use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess, WriteOnlyAccess};

use crate::drivers::virtio_net;

// Ported from https://wiki.osdev.org/PCI
pub fn pciConfigRead_u16(bus: u8, slot: u8, func: u8, offset: u8) -> u16 {
    let address: u32;
    let lbus: u32 = bus.into();
    let lslot: u32 = slot.into();
    let lfunc: u32 = func.into();
    let mut tmp: u16 = 0;

    // Create configuration address as per Figure 1
    address =
        (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) as u32 | (0x80000000 as u32);

    // Write out the address
    let mut pci_writer: PortGeneric<u32, WriteOnlyAccess> = PortGeneric::new(0xCF8);
    let mut pci_reader: PortGeneric<u32, ReadOnlyAccess> = PortGeneric::new(0xCFC);

    unsafe {
        pci_writer.write(address);

        let v = pci_reader.read();
        tmp = ((v >> ((offset & 2) * 8)) & 0xFFFF).try_into().unwrap();
    }
    return tmp;
}

pub fn pciConfigRead_u32(bus: u8, slot: u8, func: u8, offset: u8) -> u32 {
    let address: u32;
    let lbus: u32 = bus.into();
    let lslot: u32 = slot.into();
    let lfunc: u32 = func.into();
    let mut tmp: u16 = 0;

    // Create configuration address as per Figure 1
    address =
        (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) as u32 | (0x80000000 as u32);

    // Write out the address
    let mut pci_writer: PortGeneric<u32, WriteOnlyAccess> = PortGeneric::new(0xCF8);
    let mut pci_reader: PortGeneric<u32, ReadOnlyAccess> = PortGeneric::new(0xCFC);

    let mut v: u32 = 0;
    unsafe {
        pci_writer.write(address);

        v = pci_reader.read();
    }
    return v;
}

// Ported from https://wiki.osdev.org/PCI
pub fn pciCheckVendor(bus: u8, slot: u8) -> Option<u16> {
    let mut vendor: u16;
    let device: u16;

    /* Try and read the first configuration register. Since there are no
     * vendors that == 0xFFFF, it must be a non-existent device. */
    vendor = pciConfigRead_u16(bus, slot, 0, 0);
    if (vendor == 0xFFFF) {
        return None;
    }

    // Continue probing
    device = pciConfigRead_u16(bus, slot, 0, 2);

    println!("PCI Device found: {} {}", vendor, device);

    if (vendor == 0x1AF4 && device >= 0x1000 && device <= 0x103F) {
        let subsystem: u16 = pciConfigRead_u16(bus, slot, 0, 0x2e);
        print!("Found VirtIO device: ");

        match subsystem {
            1 => println!("Network device"),
            2 => println!("Block device"),
            _ => println!("Unsupported"),
        }

        println!("Getting header type");

        let test_type: u32 = pciConfigRead_u32(bus, slot, 0, 0) & 0xFF00;
        println!("test type: {}", test_type);

        let header_type: u32 = pciConfigRead_u32(bus, slot, 0x3, 2) & 0x0F00;
        println!("Header type: {}", header_type);

        if subsystem == 1 {
            virtio_net::hello_virtio_network(bus, slot, 0);
        }
    }

    return Some(vendor);
}

pub fn pciScanBus() {
    for bus in 0..255 {
        for slot in 0..31 {
            let vendor: Option<u16> = pciCheckVendor(bus, slot);
        }
    }
}
