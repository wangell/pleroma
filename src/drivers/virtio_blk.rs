use crate::architecture::x86_64::cpu::mem_fence;
use crate::bin_helpers::*;
use crate::common::Vec;
use crate::drivers::virtio;
use crate::memory;
use crate::native;
use crate::pci;
use core::mem;
use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess};

const fixed_queue_size: usize = 256;

const BLOCK_IN : u32 = 0;
const BLOCK_OUT : u32 = 1;

pub struct VirtioBlkDev {
    dev: virtio::VirtioDevice,
    request_base: *mut u8,
    data_base: *mut u8,
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct BlockRequest {
    vtype: u32,
    reserved: u32,
    sector: u64,
}

impl VirtioBlkDev {
    pub fn new(io_base: u16, irq: u8, request_base: *mut u8, data_base: *mut u8) -> Self {
        Self {
            dev: virtio::VirtioDevice::new(io_base, irq),
            request_base,
            data_base,
        }
    }

    pub fn write_buffer(&mut self, sector: u64, buffer: &[u8; 512]) {

        unsafe {
            core::ptr::copy(buffer.as_ptr(), self.data_base, 512);
        }

        let block_requests = self.request_base as *mut [BlockRequest; fixed_queue_size];
        let block_requests_array: &mut [BlockRequest; fixed_queue_size] =
            unsafe { &mut *block_requests };

        unsafe {
            // TODO: multiple buffers
            block_requests_array[0].vtype = BLOCK_OUT;
            block_requests_array[0].reserved = 0;
            block_requests_array[0].sector = sector;
        }

        let status_byte = (self.data_base as u64 + 4096) as *mut u8;
        unsafe {
            core::ptr::write_volatile(status_byte, 0xff);
        }

        unsafe {
            let header_id: u16 = 0;
            let body_id: u16 = 1;
            let footer_id: u16 = 2;

            let mut dc = Vec::<virtio::VqBuffer>::new();
            dc.push(virtio::VqBuffer{
                address: self.request_base as u64,
                length: mem::size_of::<BlockRequest>() as u32,
                flags: virtio::VRING_NEXT,
                next: 0
            });

            dc.push(virtio::VqBuffer{
                address: self.data_base as u64,
                length: 512,
                flags: virtio::VRING_NEXT,
                next: 0
            });

            dc.push(virtio::VqBuffer{
                address: status_byte as u64,
                length: 1,
                flags: virtio::VRING_WRITE,
                next: 0
            });

            self.dev.push_descriptor_chain(0, dc);

            self.dev.notify_queue(0);
        }

        unsafe { while (core::ptr::read_volatile(status_byte) == 0xff) {} }
    }
}

pub fn hello_virtio_block(bus: u8, slot: u8, function: u8) {
    let bar0: u32 = pci::pciConfigRead_u32(bus, slot, function, 0x10);
    let io_base: u16 = (bar0 & 0xFFFFFFFC).try_into().unwrap();

    let irq: u8 = (pci::pciConfigRead_u32(bus, slot, function, 0x3c) & 0xff) as u8;

    let mut blkdev = VirtioBlkDev::new(
        io_base,
        irq,
        memory::create_physical_buffer(5) as *mut u8,
        memory::create_physical_buffer(5) as *mut u8,
    );

    unsafe {
        let mut status: u8 = 0;

        // Reset
        blkdev.dev.registers.device_status.write(status);

        // Acknowledge
        status |= 1;
        blkdev.dev.registers.device_status.write(status);

        // Drive
        status |= 2;
        blkdev.dev.registers.device_status.write(status);

        blkdev.dev.registers.device_features.read();
        let goal_flags = 0;
        println!("Requested flags: {:?}", goal_flags);
        blkdev.dev.registers.guest_features.write(goal_flags);
        let end_flags = blkdev.dev.registers.device_features.read();
        println!("Flags: {:?}", end_flags);

        status |= 8;
        blkdev.dev.registers.device_status.write(status);

        status = blkdev.dev.registers.device_status.read();
        if ((status & 8) == 0) {
            panic!("Failed feature negotiation.");
        }
    }

    blkdev.dev.make_queue(0);
    blkdev.dev.device_ready();

    blkdev.write_buffer(0, &[b'Q'; 512]);
    blkdev.write_buffer(1, &[b'G'; 512]);
    blkdev.write_buffer(2, &[b'Z'; 512]);
}
