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
use crate::nested_hashmap;

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
    pub node_control_queue: crossbeam::channel::Sender<NodeControlMsg>,

    pub last_vat_id: u32
}

impl ffi::BoundEntity for Nodeman {
    fn as_any(&mut self) -> &mut dyn Any {
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

            // Leave the first 100 for system vats
            last_vat_id: 100,
            node_control_queue

        };

        n
    }

    fn get_entity(vat: &mut vm_core::Vat, entity_id: u32) -> &mut Self {
        let mut target_entity = vat.entities.get_mut(&entity_id).unwrap();
        let binding = target_entity.bound_entity.as_mut().unwrap();
        let nman: &mut Self = match binding.as_any().downcast_mut::<Self>() {
            Some(b) => b,
            None => panic!("&a isn't a B!"),
        };
        nman
    }

    fn nmup(vat: &mut vm_core::Vat, entity_id: u32, args: ast::Hvalue) -> ast::Hvalue {
        let nman: &mut Nodeman = Self::get_entity(vat, entity_id);

        nman.new_vat(0, 0);

        //vat.send_msg();
        vat.send_msg(1);

        return ast::Hvalue::None;
    }

    pub fn new_vat(&mut self, code_idx: u64, entity_idx: u64) {
        println!("Running from inside a bound entity!");

        let mut vat = vm_core::Vat::new(self.last_vat_id + 1, self.code[&code_idx].clone());
        self.last_vat_id += 1;

        //self.new_vat_queue.send(monad_vat).unwrap();
        self.node_control_queue.send(NodeControlMsg::CreateVat(vat)).unwrap();
    }

    pub fn load_code_bytes(&mut self, code: &Vec<u8>) -> u64 {
        let code_idx = self.next_code_idx;
        self.code.insert(code_idx, Arc::new(code.clone()));
        self.next_code_idx += 1;

        code_idx
    }

}

pub fn load_nodeman(node: Node, nodeman_path: &str, new_vat_queue: crossbeam::channel::Sender<NodeControlMsg>) -> Vat {

    let nodeman_bindings = nested_hashmap! {
        "Nodeman" => {
            "nmup" => (Nodeman::nmup, Vec::new())
        }
    };

    let mut nman = Nodeman::new(node, new_vat_queue);

    let nm_code = compile::compile_from_files(vec![String::from("sys/nodeman.plm")], "sys/nodeman.plmb", nodeman_bindings);

    nman.load_code_bytes(&nm_code);

    let mut vat = vm_core::Vat::new(0, Arc::new(nm_code.clone()));
    vat.create_entity(0, 0, Some(Box::new(nman)));

    vat
}
