use crate::{vm_core};
use crate::common::HashMap;
use crate::common::Vec;

#[derive(Debug)]
pub struct Node {
    pub vats: HashMap<u64, vm_core::Vat>,
    pub code: HashMap<u64, Vec<u8>>,
}

impl Node {
    pub fn new() -> Node {
        Node {
            vats: HashMap::new(),
            code: HashMap::new(),
        }
    }
}
