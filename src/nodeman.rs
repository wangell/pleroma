use crate::ast;
use crate::ast::AstNode;
use crate::codegen;
use crate::pbin;
use core::any::Any;
use crate::compile;
use crate::opcodes;
use crate::ffi;
use crate::parser;
use crate::vm_core;
use crate::monad;
use crate::vm_core::Vat;
use crate::common::BTreeMap;
use crate::common::HashMap;
use crate::node::Node;
use crate::common::Arc;
use std::{fs};
use crossbeam::channel::{select, unbounded};

#[derive(Debug)]
pub enum NodeControlMsg {
    CreateVat(Vat),
    KillVat(u32)
}


#[derive(Debug)]
pub struct Nodeman {
    pub node: Node,
    pub def: ast::EntityDef,

    pub code: HashMap<u64, Arc::<Vec<u8>>>,
    pub next_code_idx: u64,
    pub node_control_queue: crossbeam::channel::Sender<NodeControlMsg>
}

impl ffi::BoundEntity for Nodeman {
    fn as_any(&self) -> &dyn Any {
        self
    }
}

impl Nodeman {
    pub fn new(node: Node, node_control_queue: crossbeam::channel::Sender<NodeControlMsg>) -> Nodeman {
        let mut n = Nodeman {
            node: node,
            code: HashMap::new(),
            next_code_idx: 0,
            def: ast::EntityDef {
                name: String::from("Nodeman"),
                data_declarations: Vec::new(),
                inoculation_list: Vec::new(),
                functions: HashMap::new(),
                foreign_functions: HashMap::new(),
            },

            node_control_queue

        };

        n
    }

    fn get_entity(vat: &mut vm_core::Vat, entity_id: u32) -> &Self {
        let mut target_entity = vat.entities.get_mut(&entity_id).unwrap();
        let binding = target_entity.bound_entity.as_ref().unwrap();
        let nman: &Self = match binding.as_any().downcast_ref::<Self>() {
            Some(b) => b,
            None => panic!("&a isn't a B!"),
        };
        nman
    }

    fn nmup(vat: &mut vm_core::Vat, entity_id: u32, args: ast::Hvalue) -> ast::Hvalue {
        let nman = Self::get_entity(vat, entity_id);

        nman.new_vat();

        //vat.send_msg();
        vat.send_msg(1);

        return ast::Hvalue::None;
    }

    pub fn new_vat(&self) {
        println!("Running from inside a bound entity!");
        let mut monad_vat = vm_core::Vat::new(2);
        monad::load_monad("sys/kernel.plm", &mut monad_vat);
        //self.new_vat_queue.send(monad_vat).unwrap();
        self.node_control_queue.send(NodeControlMsg::CreateVat(monad_vat)).unwrap();
    }

    pub fn load_code_bytes(&mut self, code: Vec<u8>) -> u64 {
        let code_idx = self.next_code_idx;
        self.code.insert(code_idx, Arc::new(code.clone()));
        self.next_code_idx += 1;
        return code_idx;
    }

}

pub fn load_nodeman(node: Node, nodeman_path: &str, vat: &mut Vat, new_vat_queue: crossbeam::channel::Sender<NodeControlMsg>) {

    let mut fmap = HashMap::new();
    fmap.insert("nmup".to_string(), (Nodeman::nmup as ast::RawFF, Vec::new()));

    let mut monmap = HashMap::new();
    monmap.insert("Nodeman".to_string(), fmap);

    let monad_code = compile::compile_from_files(vec![String::from("sys/nodeman.plm")], "sys/nodeman.plmb", monmap);
    vat.code = Arc::new(monad_code.clone());
    let mut z = 0;
    let data_table = pbin::load_entity_data_table(&mut z, &monad_code);

    let nman = Nodeman::new(node, new_vat_queue);

    vat.create_entity(0, 0, &BTreeMap::new(), Some(Box::new(nman)));
}
