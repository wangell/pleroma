extern crate alloc;

pub use alloc::str;
pub use alloc::str::FromStr;
pub use alloc::string::String;

use crate::common::Vec;

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
