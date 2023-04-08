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



pub fn read_u8_sz(vblock: &[u8]) -> (usize, u8) {
    let v = vblock[0];
    return (1, v);
}

pub fn read_u16_sz(vblock: &[u8]) -> (usize, u16) {
    let v = ((vblock[0] as u16) << 8) | vblock[1] as u16;
    return (2, v);
}

pub fn read_u32_sz(vblock: &[u8]) -> (usize, u32) {
    let v = ((vblock[0] as u32) << 24) | ((vblock[1] as u32) << 16) | ((vblock[2] as u32) << 8) | vblock[3] as u32;
    return (4, v);
}

pub fn read_u64_sz(vblock: &[u8]) -> (usize, u64) {
    let v = ((vblock[0] as u64) << 56) | ((vblock[1] as u64) << 48) | ((vblock[2] as u64) << 40) | ((vblock[3] as u64) << 32) | ((vblock[4] as u64) << 24) | ((vblock[5] as u64) << 16) | ((vblock[6] as u64) << 8) | vblock[7] as u64;
    return (8, v);
}

pub fn read_s32_sz(vblock : &[u8]) -> (usize, i32) {
    let (sv, v) = read_u32_sz(vblock);
    return (sv, v as i32);
}

pub fn read_s64_sz(vblock : &[u8]) -> (usize, i64) {
    let (sv, v) = read_u64_sz(vblock);
    return (sv, v as i64);
}

pub fn read_float32_sz(vblock : &[u8]) -> (usize, f32) {
    let (sv, v) = read_u32_sz(vblock);
    return (sv, v as f32);
}

pub fn read_float64_sz(vblock : &[u8]) -> (usize, f64) {
    let (sv, v) = read_u64_sz(vblock);
    return (sv, v as f64);
}

pub fn read_utf8_str_sz(vblock: &[u8]) -> (usize, String) {

    let mut i = 0;
    while vblock[i] != 0 {
        i += 1;
    }

    let s = match str::from_utf8(&vblock[0..i]) {
        Ok(v) => v,
        Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
    };

    // Include +1 to account for null delimiter
    return (i + 1, String::from(s))
}
