cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        use std::collections::HashMap as HashMap;
        use std::collections::BTreeMap;
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

#[derive(Debug, Clone, Copy)]
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
pub enum MsgContents {
    Request {
        args: Vec<Hvalue>,
        function_id: u32,
        function_name: String,
        src_promise: Option<u32>,
    },
    Response {
        result: Hvalue,
        dst_promise: Option<u32>,
    },

    // Initial message to kick off universe
    BigBang {
        args: Vec<Hvalue>,
        function_id: u32,
        function_name: String,
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
    address: EntityAddress,
    pub data: BTreeMap<String, Hvalue>,
    pub code: u32
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
}

pub struct EvalContext<'a> {
    pub frame: StackFrame,
    pub vat: &'a mut Vat,
    pub module: &'a Module,
}

#[derive(Debug, Clone)]
pub struct StackFrame {
    pub locals: HashMap<String, Hvalue>,
    pub return_address: Option<usize>,
    // Store source promise in here
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
            promise_stack: HashMap::new(),
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

    pub fn create_entity_code(&mut self, code: u32, data: &BTreeMap<String, Hvalue>) -> &Entity {
        let entity_id = self.last_entity_id;
        let mut ent = Entity {
            address: EntityAddress {
                node_id: 0,
                vat_id: self.vat_id,
                entity_id: entity_id,
            },
            data: data.clone(),
            code: code
        };

        ent.data
            .insert(String::from("self"), Hvalue::EntityAddress(ent.address));
        ent.data.insert(
            String::from("io"),
            Hvalue::EntityAddress(EntityAddress {
                node_id: 0,
                vat_id: 0,
                entity_id: 1,
            }),
        );

        self.entities.insert(entity_id, ent);

        self.last_entity_id += 1;

        &self.entities[&entity_id]
    }

    pub fn create_entity(&mut self, def: &EntityDef) -> &Entity {
        // Needs ref to underlying code
        let entity_id = self.last_entity_id;

        let mut ent = Entity {
            address: EntityAddress {
                node_id: 0,
                vat_id: self.vat_id,
                entity_id: entity_id,
            },
            data: BTreeMap::new(),
            code: 0
        };

        ent.data.insert(String::from("self"), Hvalue::EntityAddress(ent.address));

        self.entities.insert(entity_id, ent);
        self.last_entity_id += 1;

        &self.entities[&entity_id]
    }
}
