cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        use std::collections::HashMap as HashMap;
        use crate::ast::{Hvalue, Module, EntityDef};
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "native")] {
        extern crate alloc;

        use alloc::collections::BTreeMap as HashMap;
        use alloc::string::String;
        use alloc::vec::Vec;
        use alloc::boxed::Box;
        use crate::ast::{Hvalue, Module, EntityDef};
    }
}

#[derive(Debug, Clone)]
pub struct EntityAddress {
    pub node_id: u32,
    pub vat_id: u32,
    pub entity_id: u32,
}

pub type SystemFunction = fn(&mut Entity, Hvalue) -> Hvalue;

impl EntityAddress {
    pub fn new(node_id: u32, vat_id: u32, entity_id: u32) -> EntityAddress {
        EntityAddress {
            node_id: node_id,
            vat_id: vat_id,
            entity_id: entity_id,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Msg {
    pub src_address: EntityAddress,
    pub dst_address: EntityAddress,

    pub promise_id: Option<u32>,

    pub is_response: bool,

    pub function_name: String,
    pub function_id: u32,
    pub values: Vec<Hvalue>,
}

#[derive(Debug)]
pub struct Entity {
    address: EntityAddress,
    pub data: HashMap<String, Hvalue>,
}

#[derive(Debug)]
pub struct Vat {
    pub inbox: Vec<Msg>,
    pub outbox: Vec<Msg>,

    pub entities: HashMap<u32, Entity>,

    pub vat_id: u32,
    last_entity_id: u32,

    // Execution
    pub op_stack: Vec<Hvalue>,
    pub call_stack: Vec<StackFrame>,
    pub promise_stack: Vec<u32>
}

pub struct EvalContext<'a> {
    pub frame: StackFrame,
    pub vat: &'a mut Vat,
    pub module: &'a Module,
}

#[derive(Debug)]
pub struct StackFrame {
    pub locals: HashMap<String, Hvalue>,
    pub return_address: Option<usize>,
}

pub struct PleromaNode {}

impl Vat {
    pub fn new() -> Vat {
        Vat {
            inbox: Vec::new(),
            outbox: Vec::new(),
            entities: HashMap::new(),

            vat_id: 0,
            last_entity_id: 0,

            op_stack: Vec::new(),
            call_stack: Vec::new(),
            promise_stack: Vec::new()
        }
    }

    pub fn create_entity(&mut self, def: &EntityDef) -> &Entity {
        let entity_id = self.last_entity_id;
        self.entities.insert(
            entity_id,
            Entity {
                address: EntityAddress {
                    node_id: 0,
                    vat_id: self.vat_id,
                    entity_id: entity_id,
                },
                data: HashMap::new(),
            },
        );

        self.last_entity_id += 1;

        &self.entities[&entity_id]
    }
}
