use crate::architecture::x86_64::cpu::mem_fence;
use crate::bin_helpers::*;
use crate::common::Vec;
use crate::native;
use crate::net;
use crate::pci;
use core::mem;

use crate::drivers::virtio;

use enumflags2::{bitflags, make_bitflags, BitFlags};
use x86_64::structures::paging::FrameAllocator;
use x86_64::structures::paging::Mapper;
use x86_64::structures::paging::PageTableFlags as Flags;
use x86_64::structures::paging::PhysFrame;
use x86_64::structures::paging::Size4KiB;
use x86_64::PhysAddr;

use crate::memory::BootInfoAllocatorTrait;
use crate::memory;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u32)]
enum NetDevFlags {
    CSUM = 0,
    GUEST_CSUM = 1,
    MAC = 5,
    GSO = 6,

    GUEST_TSO4 = 7,
    GUEST_TSO6 = 8,
    GUEST_ECN = 9,
    GUEST_UFO = 10,

    HOST_TSO4 = 11,
    HOST_TSO6 = 12,
    HOST_ECN = 13,
    HOST_UFO = 14,

    VIRTIO_NET_MRG_RXBUF = 15,
    STATUS = 16,
    CTRL_VQ = 17,
    CTRL_RX = 18,

    CTRL_VLAN = 19,
    GUEST_ANNOUNCE = 21,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u32)]
enum NetStatusFlags {
    VIRTIO_NET_S_LINK_UP = 1,
    VIRTIO_NET_S_ANNOUNCE = 2,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u8)]
enum NetHdrFlags {
    NEEDS_CSUM = 1,
    DATA_VALID = 2,
    RSC_INFO = 4,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u32)]
enum GsoFlags {
    NONE = 0,
    TCPV4 = 1,
    UDP = 3,
    TCPV6 = 4,
    ECN = 0x80,
}

#[repr(u16)]
pub enum NetQueueSelector {
    Receive = 0,
    Transmit = 1,
    Control = 2,
}

pub struct VirtioNetDev {
    dev: virtio::VirtioDevice,
}

impl VirtioNetDev {
    pub fn new(io_base: u16, irq: u8) -> Self {
        Self {
            dev: virtio::VirtioDevice::new(io_base, irq),
        }
    }
}

#[derive(Debug, Copy, Clone)]
#[repr(C)]
struct NetHeader {
    flags: u8,
    gso_type: u8,
    hdr_len: u16,
    gso_size: u16,
    csum_start: u16,
    csum_offset: u16,
    //num_buffers: u16
}

pub fn doit(netdev: &mut VirtioNetDev) {
    // The base address of the request buffer
    let rx_buffer_base = memory::create_physical_buffer(5);
    let tx_buffer_base = memory::create_physical_buffer(5);

    let blah = memory::create_physical_buffer(1) as *mut u8;

    // FIXME get from size
    const fixed_queue_size: usize = 256;

    let rx_block_requests = rx_buffer_base as *mut [NetHeader; fixed_queue_size];
    let rx_block_requests_array: &mut [NetHeader; fixed_queue_size] =
        unsafe { &mut *rx_block_requests };

    let tx_block_requests = tx_buffer_base as *mut [NetHeader; fixed_queue_size];
    let tx_block_requests_array: &mut [NetHeader; fixed_queue_size] =
        unsafe { &mut *tx_block_requests };

    // Setup a bunch of requests that write 'A' repeating
    unsafe {
        for i in 0..256 {
            tx_block_requests_array[i].flags |= 1;
            tx_block_requests_array[i].csum_start = 34;
            tx_block_requests_array[i].csum_offset = 4;
        }
    }

    let msg = tx_buffer_base;
    let eth = net::create_arp_broadcast_packet(&[0x00, 0x11, 0x22, 0x33, 0x44, 0x55], &[192, 168, 100, 2],);
    unsafe {
        core::ptr::copy(eth.as_ptr(), blah, eth.len());
    }

    // FIXME: hardcoding body ID
    netdev.dev.push_descriptor(
        NetQueueSelector::Transmit as u16,
        tx_buffer_base as u64,
        mem::size_of::<NetHeader>() as u32,
        virtio::VRING_NEXT,
        1,
        true
    );
    netdev.dev.push_descriptor(
        NetQueueSelector::Transmit as u16,
        blah as u64,
        eth.len() as u32,
        0,
        0,
        false
    );
    netdev
        .dev
        .set_available_index(NetQueueSelector::Transmit as u16, 1);

    netdev.dev.notify_queue(NetQueueSelector::Transmit as u16);
}

pub fn hello_virtio_network(bus: u8, slot: u8, function: u8) {
    let bar0: u32 = pci::pciConfigRead_u32(bus, slot, function, 0x10);
    let io_base: u16 = (bar0 & 0xFFFFFFFC).try_into().unwrap();
    let irq: u8 = (pci::pciConfigRead_u32(bus, slot, function, 0x3c) & 0xff) as u8;

    let mut netdev = VirtioNetDev::new(io_base, irq);

    unsafe {
        let mut status: u8 = 0;

        // Reset
        netdev.dev.registers.device_status.write(status);

        // Acknowledge
        status |= 1;
        netdev.dev.registers.device_status.write(status);

        // Drive
        status |= 2;
        netdev.dev.registers.device_status.write(status);

        netdev.dev.registers.device_features.read();
        let goal_flags =
            NetDevFlags::CSUM as u32 | NetDevFlags::MAC as u32 | NetDevFlags::STATUS as u32;
        println!("Requested flags: {:?}", goal_flags);
        netdev.dev.registers.guest_features.write(goal_flags);
        let end_flags = netdev.dev.registers.device_features.read();
        println!("Flags: {:?}", end_flags);
        //if ((end_flags & goal_flags) != goal_flags) {
        //    panic!("Got different features from device: {:?}", end_flags);
        //}

        status |= 8;
        netdev.dev.registers.device_status.write(status);

        status = netdev.dev.registers.device_status.read();
        if ((status & 8) == 0) {
            panic!("Failed feature negotiation.");
        }
    }

    let mut mac_address: [u8; 6] = [0u8; 6];
    for i in 0..6 {
        mac_address[i] = io_read_le_u8(io_base + 0x14 + i as u16);
    }

    println!("MAC address: {:?}", mac_address);

    println!("Status: {}", io_read_le_u16(io_base + 0x1A));

    netdev.dev.make_queue(NetQueueSelector::Receive as u16);
    netdev.dev.make_queue(NetQueueSelector::Transmit as u16);

    netdev.dev.device_ready();

    doit(&mut netdev);
}
