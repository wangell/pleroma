extern crate alloc;
use crate::bin_helpers::{read_le_u16, read_le_u32, read_u8, read_utf8_str};
pub use crate::common::vec;
use crate::common::Vec;
pub use alloc::str;
pub use alloc::str::FromStr;
pub use alloc::string::String;

pub struct Ext2Fs<'a> {
    data: &'a [u8],
}
