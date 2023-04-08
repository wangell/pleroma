use core;
use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess, WriteOnlyAccess};
use core::arch::asm;

pub fn mem_fence() {
    core::sync::atomic::compiler_fence(core::sync::atomic::Ordering::Acquire);
    unsafe {
        asm!("mfence", options(nomem, nostack));
    }
}

pub fn shutdown() {
    unsafe {
        let mut p: PortGeneric<u16, WriteOnlyAccess> = PortGeneric::new(0x604);
        p.write(0x2000);
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
