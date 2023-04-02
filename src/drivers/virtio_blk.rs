use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess};
use crate::bin_helpers::*;
use crate::pci;
use crate::native;
use crate::architecture::x86_64::cpu::mem_fence;
use crate::common::Vec;
use core::mem;

pub static virtio_list: spin::Mutex<VirtIODev> = spin::Mutex::new(VirtIODev { io_base: 0, irq: 0 });

const fixed_queue_size: usize = 256;

pub struct VirtIODev {
    pub io_base: u16,
    pub irq: u8
}

#[derive(Debug, Copy, Clone)]
#[repr(C, align(16))]
struct VqBuffer {
    address: u64,
    length: u32,
    flags: u16,
    next: u16,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct BlockRequest {
    vtype: u32,
    reserved: u32,
    sector: u64,
}

#[repr(C, align(2))]
struct Available {
    flags: u16,
    index: u16,
    ring: [u16; fixed_queue_size],
    event_index: u16,
}

#[repr(C)]
struct UsedRingEntry {
    index: u32,
    length: u32,
}

#[repr(C, align(4))]
struct Used {
    flags: u16,
    index: u16,
    ring: [UsedRingEntry; fixed_queue_size],
    avail_event: u16,
}

#[repr(C)]
struct VirtualQueue {
    buffers: [VqBuffer; fixed_queue_size],
    available: Available,
    // FIXME: separate pages
    _padding: [u8; 3568],
    used: Used,
}

pub fn setup_virtio() {
    let mut phys_add: Vec<u64> = Vec::new();

    for i in 0..20 {
        use x86_64::structures::paging::FrameAllocator;
        //println!("{:?}", frame_allocator.allocate_frame().unwrap().start_address().as_u64() / 4096);

        let mut ft = native::FRAME_ALLOCATOR.lock();
        let mut frame_allocator = ft.as_mut().unwrap();

        let mut mt = native::MAPPER.lock();
        let mut mapper = mt.as_mut().unwrap();

        let frame = frame_allocator.allocate_frame().unwrap();
        use x86_64::structures::paging::PageTableFlags as Flags;
        //let start_page = Page::containing_address(VirtAddr::new(0x_5555_4444_0000));
        //let end_page = Page::containing_address(heap_end);
        //mapper.map_to(page, frame, Flags::PRESENT | Flags::NO_CACHE | Flags::WRITABLE, frame_allocator);
        use x86_64::structures::paging::Mapper;
        unsafe {
            mapper.identity_map(
                frame,
                Flags::PRESENT | Flags::NO_CACHE | Flags::WRITABLE,
                frame_allocator,
            );
        }

        phys_add.push(frame.start_address().as_u64());
    }

    println!("Allocated frames: {:?}", phys_add);

    let virtio = virtio_list.lock();
    use x86_64::instructions::port::ReadWriteAccess;

    let mut queue_address_physical_idx: u32 = (phys_add[2] / 4096).try_into().unwrap();
    let mut queue_address: PortGeneric<u32, ReadWriteAccess> =
        PortGeneric::new(virtio.io_base + 0x08);
    let mut queue_size: PortGeneric<u16, ReadWriteAccess> = PortGeneric::new(virtio.io_base + 0x0C);
    let mut queue_select: PortGeneric<u16, ReadWriteAccess> =
        PortGeneric::new(virtio.io_base + 0x0E);

    println!("Size withotu padding: {}", mem::size_of::<VirtualQueue>());

    // Zero all the pages
    let bb_add = phys_add[0] as *mut u64;
    for i in 0..(20 * 4096) {
        unsafe {
            core::ptr::write_volatile(bb_add.offset(i), 0);
        }
    }

    unsafe {
        queue_select.write(0);
    }

    println!("Writing queue address: {}", queue_address_physical_idx);
    unsafe {
        println!("Queue size: {:#06x}", queue_size.read());
        queue_address.write(queue_address_physical_idx);
    }

    // The base address of VirtioQueue
    let queue_base_address = phys_add[2] as *mut u8;

    // The base address of the request buffer
    let buffer_base = phys_add[15] as *mut u8;

    unsafe {
        let mut device_status: PortGeneric<u8, ReadWriteAccess> =
            PortGeneric::new(virtio.io_base + 0x12);
        device_status.write(1 | 2 | 4);
        println!("Final status {}", device_status.read());
    }

    let block_requests = buffer_base as *mut [BlockRequest; fixed_queue_size];
    let block_requests_array: &mut [BlockRequest; fixed_queue_size] =
        unsafe { &mut *block_requests };

    // Setup a bunch of requests that write 'A' repeating
    let BLOCK_IN = 0;
    let BLOCK_OUT = 1;
    unsafe {
        for i in 0..fixed_queue_size {
            block_requests_array[i].vtype = 1;
            block_requests_array[i].reserved = 0;
            block_requests_array[i].sector = 0;
        }
    }

    let vq = phys_add[2] as *mut VirtualQueue;

    let blah = phys_add[13] as *mut u8;
    for z in 0..512 {
        unsafe {
            core::ptr::write_volatile(blah.offset(z), b'A');
        }
    }

    let blah2 = phys_add[14] as *mut u8;
    unsafe {
        core::ptr::write_volatile(blah2, 0xff);
    }

    unsafe {
        println!("Status : {}", *blah2);
    }

    unsafe {
        let VRING_NEXT = 1;
        let VRING_WRITE = 2;

        let header_id: usize = 0;
        let body_id: usize = 1;
        let footer_id: usize = 2;

        (*vq).buffers[header_id].address = (buffer_base as u64);
        (*vq).buffers[header_id].length = mem::size_of::<BlockRequest>() as u32;
        (*vq).buffers[header_id].flags = VRING_NEXT;
        (*vq).buffers[header_id].next = body_id as u16;

        (*vq).buffers[body_id].address = blah as u64;
        (*vq).buffers[body_id].length = 512;
        (*vq).buffers[body_id].flags = VRING_NEXT;
        (*vq).buffers[body_id].next = footer_id as u16;

        let status_ptr = blah2;
        (*vq).buffers[footer_id].address = status_ptr as u64;
        (*vq).buffers[footer_id].length = 1;
        (*vq).buffers[footer_id].flags = VRING_WRITE;
        (*vq).buffers[footer_id].next = 0;

        (*vq).available.ring[0] = header_id as u16;

        mem_fence();

        let index_ptr = &mut (*vq).available.index as *mut u16;
        core::ptr::write_volatile(index_ptr, 1);

        mem_fence();
    }

    let mut queue_notify: PortGeneric<u16, ReadWriteAccess> =
        PortGeneric::new(virtio.io_base + 0x10);

    unsafe {
        queue_notify.write(0);
    }

    unsafe { while (core::ptr::read_volatile(blah2) == 0xff) {} }

    unsafe {
        println!("Status : {}", core::ptr::read_volatile(blah2));
        println!("Buf: {}", *blah);
    }
}

pub fn hello_virtio(bus: u8, slot: u8, function: u8) {
    let bar0: u32 = pci::pciConfigRead_u32(bus, slot, function, 0x10);
    let io_base: u16 = (bar0 & 0xFFFFFFFC).try_into().unwrap();

    let irq: u8 = (pci::pciConfigRead_u32(bus, slot, function, 0x3c) & 0xff) as u8;

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

        // Reset
        device_status.write(status);

        // Acknowledge
        status |= 1;
        device_status.write(status);

        // Drive
        status |= 2;
        device_status.write(status);

        device_features.read();
        guest_features.write(0);

        status |= 8;
        device_status.write(status);

        status = device_status.read();
        if ((status & 8) == 0) {
            panic!("Failed feature negotiation.");
        }

        setup_virtio_block_device(irq, io_base);
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

    use crate::native;

    let mut blah = virtio_list.lock();
    blah.io_base = io_base;
    blah.irq = irq;

}
