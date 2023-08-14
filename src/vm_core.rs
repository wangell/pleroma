cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        use std::collections::HashMap as HashMap;
        use std::collections::BTreeMap;
        use crate::ast::{Hvalue, Module, EntityDef};
        use std::rc::Rc;
        use core::cell::RefCell;
        use std::sync::Mutex;
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "native")] {
        extern crate alloc;

        use alloc::collections::BTreeMap as HashMap;
        use alloc::collections::BTreeMap;
        use alloc::string::String;
        use alloc::vec::Vec;
        use alloc::boxed::Box;
        use alloc::rc::Rc;
        use core::cell::RefCell;
        use crate::ast::{Hvalue, Module, EntityDef};
    }
}


use crate::common::Arc;
use crate::ffi;

#[derive(Debug, Clone, Copy)]
pub struct EntityAddress {
    pub node_id: u32,
    pub vat_id: u32,
    pub entity_id: u32,
}

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
pub enum MsgContents {
    Request {
        args: Vec<Hvalue>,
        function_id: u32,
        src_promise: Option<u32>,
        no_response: bool
    },
    Response {
        result: Hvalue,
        dst_promise: Option<u32>,
    },
}

#[derive(Debug, Clone)]
pub struct Msg {
    pub src_address: EntityAddress,
    pub dst_address: EntityAddress,
    pub contents: MsgContents,
}

#[derive(Debug)]
pub struct Entity {
    pub address: EntityAddress,
    pub data: BTreeMap<String, Hvalue>,
    pub code: u32,
    pub entity_type: u32,
    pub bound_entity: Option<Box<dyn ffi::BoundEntity>>
}

#[derive(Debug, Clone)]
pub struct Promise {
    pub on_resolve: Vec<usize>,

    pub resolved: bool,
    pub returned_val: Option<Hvalue>,
    pub save_point: (Vec<Hvalue>, Vec<StackFrame>),

    pub var_names: Vec<String>,

    // Create from promise ID src_promise
    pub src_promise: Option<u32>,

    pub src_entity: Option<EntityAddress>
}

impl Promise {
    pub fn new(src_promise: Option<u32>, src_entity: Option<EntityAddress>) -> Self {
        Self {
            resolved: false,
            returned_val: None,
            on_resolve: Vec::new(),
            save_point: (Vec::new(), Vec::new()),
            var_names: Vec::new(),
            src_promise: src_promise,
            src_entity: src_entity
        }
    }
}

#[derive(Debug)]
pub struct Vat {
    pub inbox: Vec<Msg>,
    pub outbox: Vec<Msg>,

    pub entities: HashMap<u32, Entity>,

    pub vat_id: u32,
    last_entity_id: u32,

    // Execution
    // FIXME: move inside stackframe
    pub op_stack: Vec<Hvalue>,
    pub call_stack: Vec<StackFrame>,
    pub promise_stack: HashMap<u8, Promise>,
    pub code: Arc::<Vec<u8>>
}

pub struct EvalContext<'a> {
    pub frame: StackFrame,
    pub vat: &'a mut Vat,
    pub module: &'a Module,
}

#[derive(Debug, Clone)]
pub struct StackFrame {
    pub entity_id: u32,
    pub locals: HashMap<String, Hvalue>,
    pub return_address: Option<usize>,
    // Store source promise in here
}

pub struct PleromaNode {}

impl Vat {
    pub fn new(vat_id: u32, code: Arc<Vec<u8>>) -> Vat {
        Vat {
            inbox: Vec::new(),
            outbox: Vec::new(),
            entities: HashMap::new(),

            vat_id: vat_id,
            last_entity_id: 0,

            op_stack: Vec::new(),
            call_stack: Vec::new(),
            promise_stack: HashMap::new(),
            code
        }
    }

    pub fn load_local(&mut self, s: &String) -> Hvalue {
        let last_ind = self.call_stack.len() - 1;
        let mut last_frame = &mut self.call_stack[last_ind];

        last_frame.locals[s].clone()
    }

    pub fn store_local(&mut self, s: &String, val: &Hvalue) {
        let last_ind = self.call_stack.len() - 1;
        let mut last_frame = &mut self.call_stack[last_ind];
        last_frame.locals.insert(s.clone(), val.clone());
    }

    pub fn create_entity_code(&mut self, entity_type: u32, code: u32, data: &BTreeMap<String, Hvalue>) -> &Entity {
        self.create_entity(entity_type, code, data, None)
    }

    pub fn create_entity(&mut self, entity_type: u32, code: u32, data: &BTreeMap<String, Hvalue>, bound_entity: Option<Box<dyn ffi::BoundEntity>>) -> &Entity {
        let entity_id = self.last_entity_id;
        let mut ent = Entity {
            address: EntityAddress {
                node_id: 0,
                vat_id: self.vat_id,
                entity_id: entity_id,
            },
            data: data.clone(),
            code: code,
            entity_type: entity_type,
            bound_entity: bound_entity
        };

        ent.data.insert(String::from("self"), Hvalue::EntityAddress(ent.address));

        self.entities.insert(entity_id, ent);

        self.last_entity_id += 1;

        &self.entities[&entity_id]
    }

    pub fn send_msg(&mut self, ent: u32) {
        let msg = Msg {
            src_address: EntityAddress::new(0, 0, 0),
            dst_address: EntityAddress::new(0, ent, 0),
            contents: MsgContents::Request{ function_id: 2, args: Vec::new(), src_promise: None, no_response: false }
        };

        self.outbox.push(msg);
    }
}
