use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess, WriteOnlyAccess};

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

        setup_virtio(bus, slot, 0);
    }

    return Some(vendor);
}

pub fn setup_virtio(bus: u8, slot: u8, function: u8) {
    let bar0: u32 = pciConfigRead_u32(bus, slot, function, 0x10);
    let io_base: u16 = (bar0 & 0xFFFFFFFC).try_into().unwrap();

    let irq: u8 = (pciConfigRead_u32(bus, slot, function, 0x3c) & 0xff) as u8;

    let mut device_features: PortGeneric<u32, ReadOnlyAccess> = PortGeneric::new(io_base + 0x00);
    let mut guest_features: PortGeneric<u32, ReadWriteAccess> = PortGeneric::new(io_base + 0x04);
    let mut queue_address: PortGeneric<u32, ReadWriteAccess> = PortGeneric::new(io_base + 0x08);
    let mut queue_size: PortGeneric<u16, ReadWriteAccess> = PortGeneric::new(io_base + 0x0C);
    let mut queue_select: PortGeneric<u16, ReadWriteAccess> = PortGeneric::new(io_base + 0x0E);
    let mut queue_notify: PortGeneric<u16, ReadWriteAccess> = PortGeneric::new(io_base + 0x10);
    let mut device_status: PortGeneric<u8, ReadWriteAccess> = PortGeneric::new(io_base + 0x12);
    let mut isr_status: PortGeneric<u8, ReadWriteAccess> = PortGeneric::new(io_base + 0x13);

    unsafe {
        let mut status: u8 = 0;
        // Acknowledge
        status |= 1;
        device_status.write(status);

        // Drive
        status |= 2;
        device_status.write(status);

        // Ready
        status |= 4;
        device_status.write(status);

        setup_virtio_block_device(irq, io_base);
    }
}

pub fn io_read_le_u64(io_base: u16) -> u64 {
    let mut z: u64 = 0;
    for i in 0..8 {
        unsafe {
            let mut mac_addy: PortGeneric<u8, ReadOnlyAccess> =
                PortGeneric::new(io_base + i);
            let blah = mac_addy.read() as u64;
            z |= blah << (i * 8);
        }
    }
    return z;
}

pub fn io_read_le_u32(io_port: u16) -> u32 {
    let mut z: u32 = 0;
    for i in 0..4 {
        unsafe {
            let mut mac_addy: PortGeneric<u8, ReadOnlyAccess> = PortGeneric::new(io_port + i);
            let blah = mac_addy.read() as u32;
            z |= blah << (i * 8);
        }
    }
    return z;
}

pub fn io_read_le_u16(io_port: u16) -> u16 {
    let mut z: u16 = 0;
    for i in 0..2 {
        unsafe {
            let mut mac_addy: PortGeneric<u8, ReadOnlyAccess> = PortGeneric::new(io_port + i);
            let blah = mac_addy.read() as u16;
            z |= blah << (i * 8);
        }
    }
    return z;
}

pub fn io_read_le_u8(io_port: u16) -> u8 {
    unsafe {
        let mut mac_addy: PortGeneric<u8, ReadOnlyAccess> = PortGeneric::new(io_port);
        let blah = mac_addy.read();
        return blah;
    }
}

pub fn setup_virtio_block_device(irq: u8, io_base: u16) {
    let total_sectors = io_read_le_u64(io_base + 0x14);
    let size_max = io_read_le_u32(io_base + 0x1C);
    let seg_max = io_read_le_u32(io_base + 0x20);
    let cylinder_count = io_read_le_u16(io_base + 0x24);
    let head_count = io_read_le_u8(io_base + 0x26);
    let sector_count = io_read_le_u8(io_base + 0x27);
    let block_length = io_read_le_u32(io_base + 0x28);

    println!("Total sectors: {}", total_sectors);
    println!("Max size: {}", size_max);
    println!("Seg max: {}", seg_max);
    println!("Cylinder count: {}", cylinder_count);
    println!("Head count: {}", head_count);
    println!("Sector count: {}", sector_count);
    println!("Block length: {}", block_length);
}

pub fn pciScanBus() {
    for bus in 0..255 {
        for slot in 0..31 {
            let vendor: Option<u16> = pciCheckVendor(bus, slot);
        }
    }
}
