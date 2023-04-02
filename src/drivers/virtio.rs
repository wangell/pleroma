use crate::architecture::x86_64::cpu::mem_fence;
use crate::bin_helpers::*;
use crate::common::HashMap;
use crate::common::Vec;
use crate::native;
use crate::pci;
use core::mem;
use enumflags2::{bitflags, make_bitflags, BitFlags};
use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess, WriteOnlyAccess};
use x86_64::structures::paging::FrameAllocator;
use x86_64::structures::paging::Mapper;
use x86_64::structures::paging::PageTableFlags as Flags;

const fixed_queue_size: usize = 256;

// Device Status
const DEV_ACKNOWLEDGE: u8 = 1;
const DEV_DRIVER: u8 = 2;
const DEV_DRIVER_OK: u8 = 4;
const DEV_FAILED: u8 = 128;

// Descriptor flags
pub const VRING_NEXT: u16 = 1;
pub const VRING_WRITE: u16 = 2;
pub const VRING_INDIRECT: u16 = 4;

// FIXME: needs custom drop to release physical memory back to kernel
pub struct VirtioDevice {
    io_base: u16,
    irq: u8,
    pub registers: VirtioRegisters,
    pub vqs: HashMap<u16, VirtualQueue>,
}

impl VirtioDevice {
    pub fn new(io_base: u16, irq: u8) -> Self {
        Self {
            io_base,
            irq,
            registers: VirtioRegisters::new(io_base),
            vqs: HashMap::new(),
        }
    }
}

pub struct VirtioRegisters {
    pub device_features: PortGeneric<u32, ReadOnlyAccess>,
    pub guest_features: PortGeneric<u32, ReadWriteAccess>,
    pub queue_address: PortGeneric<u32, WriteOnlyAccess>,
    pub queue_size: PortGeneric<u16, ReadWriteAccess>,
    pub queue_select: PortGeneric<u16, ReadWriteAccess>,
    pub queue_notify: PortGeneric<u16, ReadWriteAccess>,
    pub device_status: PortGeneric<u8, ReadWriteAccess>,
    pub isr_status: PortGeneric<u8, ReadWriteAccess>,
}

impl VirtioRegisters {
    pub fn new(io_base: u16) -> Self {
        Self {
            device_features: PortGeneric::new(io_base + 0x00),
            guest_features: PortGeneric::new(io_base + 0x04),
            queue_address: PortGeneric::new(io_base + 0x08),
            queue_size: PortGeneric::new(io_base + 0x0C),
            queue_select: PortGeneric::new(io_base + 0x0E),
            queue_notify: PortGeneric::new(io_base + 0x10),
            device_status: PortGeneric::new(io_base + 0x12),
            isr_status: PortGeneric::new(io_base + 0x13),
        }
    }
}

#[derive(Debug, Copy, Clone)]
#[repr(C, align(16))]
pub struct VqBuffer {
    pub address: u64,
    pub length: u32,
    pub flags: u16,
    pub next: u16,
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

pub struct VirtualQueue {
    id: u16,
    size: u16,
    next_idx: u32,
    avail_idx: u16,
    vq_data: *mut VirtualQueueData,
}

#[repr(C)]
struct VirtualQueueData {
    buffers: [VqBuffer; fixed_queue_size],
    available: Available,
    _padding: [u8; 3568],
    used: Used,
}

impl VirtioDevice {
    pub fn notify_queue(&mut self, queue_n: u16) {
        unsafe {
            self.registers.queue_notify.write(queue_n);
        }
    }

    pub fn make_queue(&mut self, queue_n: u16) {
        let queue_size = unsafe { self.registers.queue_size.read() };

        let mut phys_add: Vec<u64> = Vec::new();
        let requested_pages = (mem::size_of::<VirtualQueueData>() / 4096) + 1;

        let mut ft = native::FRAME_ALLOCATOR.lock();
        let mut mt = native::MAPPER.lock();
        for i in 0..requested_pages {
            let mut frame_allocator = ft.as_mut().unwrap();

            let mut mapper = mt.as_mut().unwrap();

            let frame = frame_allocator.allocate_frame().unwrap();
            unsafe {
                mapper.identity_map(
                    frame,
                    Flags::PRESENT | Flags::NO_CACHE | Flags::WRITABLE,
                    frame_allocator,
                );
            }

            phys_add.push(frame.start_address().as_u64());
        }

        // Zero all the pages
        let bb_add = phys_add[0] as *mut u8;
        for i in 0..(requested_pages * 4096) {
            unsafe {
                core::ptr::write_volatile(bb_add.offset(i.try_into().unwrap()), 0);
            }
        }

        let base_addr = phys_add[0];
        let base_page_idx = (base_addr / 4096).try_into().unwrap();

        // Notify device of queue address
        unsafe {
            self.registers.queue_select.write(queue_n);
            self.registers.queue_address.write(base_page_idx);
        }

        //self.vqs.insert(queue_n, Box::from_raw(base_addr));
        self.vqs.insert(
            queue_n,
            VirtualQueue {
                id: queue_n,
                next_idx: 0,
                size: queue_size,
                avail_idx: 0,
                vq_data: base_addr as *mut VirtualQueueData,
            },
        );
    }

    pub fn push_descriptor_chain(&mut self, queue_n: u16, buffers: Vec<VqBuffer>) {
        let idx = self.vqs[&queue_n].next_idx as usize;

        for (rel_idx, buf) in buffers.iter().enumerate() {

            let next_idx = if rel_idx == buffers.len() - 1 {
                0
            } else {
                (idx + rel_idx + 1) % self.vqs[&queue_n].size as usize
            };

            self.push_descriptor(queue_n, buf.address, buf.length, buf.flags, next_idx as u16, rel_idx == 0);
        }
    }

    pub fn push_descriptor(
        &mut self,
        queue_n: u16,
        address: u64,
        length: u32,
        flags: u16,
        next: u16,
        head: bool,
    ) {
        let idx = self.vqs[&queue_n].next_idx as usize;
        let queue_size = self.vqs[&queue_n].size;

        unsafe {
            (*self.vqs[&queue_n].vq_data).buffers[idx].address = address;
            (*self.vqs[&queue_n].vq_data).buffers[idx].length = length;
            (*self.vqs[&queue_n].vq_data).buffers[idx].flags = flags;
            (*self.vqs[&queue_n].vq_data).buffers[idx].next = next;
        }

        let next_idx = (idx + 1) % self.vqs[&queue_n].size as usize;

        self.vqs.get_mut(&queue_n).unwrap().next_idx = next_idx as u32;

        if head {
            unsafe {
                (*self.vqs[&queue_n].vq_data).available.ring [(self.vqs[&queue_n].avail_idx % queue_size) as usize] = idx as u16;
                self.increment_available(queue_n);
            }
        }
    }

    pub fn increment_available(&mut self, queue_n: u16) {
        let vq = self.vqs.get_mut(&queue_n).unwrap();
        // This can be removed when compiling to release
        if vq.avail_idx == core::u16::MAX {
            vq.avail_idx = 0;
        } else {
            vq.avail_idx = vq.avail_idx + 1;
        }
        let aidx = vq.avail_idx;
        self.set_available_index(queue_n, aidx);
    }

    pub fn set_available_index(&mut self, queue_n: u16, n: u16) {
        mem_fence();

        unsafe {
            let index_ptr = &mut (*self.vqs[&queue_n].vq_data).available.index as *mut u16;
            core::ptr::write_volatile(index_ptr, n);
        }

        mem_fence();
    }

    // Called after setting up queues, before device goes live
    pub fn device_ready(&mut self) {
        unsafe {
            self.registers
                .device_status
                .write(DEV_ACKNOWLEDGE | DEV_DRIVER | DEV_DRIVER_OK);
        }
    }
}
