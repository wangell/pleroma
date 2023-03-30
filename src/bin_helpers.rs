extern crate alloc;

pub use alloc::str;
pub use alloc::str::FromStr;
pub use alloc::string::String;

use crate::common::Vec;

use x86_64::instructions::port::{PortGeneric, ReadOnlyAccess, ReadWriteAccess, WriteOnlyAccess};

pub fn read_utf8_str(vblock: &[u8]) -> String {
    let s = match str::from_utf8(vblock) {
        Ok(v) => v,
        Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
    };

    return String::from(s);
}

pub fn read_u8(vblock: &[u8]) -> u8 {
    let v = vblock[0] as u8;
    return v;
}

pub fn read_le_u16(vblock: &[u8]) -> u16 {
    let v = ((vblock[1] as u16) << 8) | vblock[0] as u16;
    return v;
}

pub fn read_le_u32(vblock: &[u8]) -> u32 {
    let v = ((vblock[3] as u32) << 24)
        | ((vblock[2] as u32) << 16)
        | ((vblock[1] as u32) << 8)
        | vblock[0] as u32;
    return v;
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
