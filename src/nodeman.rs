use crate::ast;
use crate::ast::AstNode;
use crate::codegen;
use crate::pbin;
use crate::compile;
use crate::opcodes;
use crate::parser;
use crate::vm_core;
use crate::vm_core::Vat;
use crate::common::BTreeMap;
use crate::common::HashMap;
use crate::node::Node;
use crate::common::Arc;
use std::{fs};

pub struct Nodeman {
    pub node: Node,
    pub def: ast::EntityDef,

    pub code: HashMap<u64, Arc::<Vec<u8>>>,
    pub next_code_idx: u64
}

impl Nodeman {
    pub fn new(node: Node) -> Nodeman {
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
        };

        n
    }

    pub fn hello(entity: &mut vm_core::Entity, args: ast::Hvalue) -> ast::Hvalue {
        println!("\x1b[0;32mHello, welcome to Pleroma!\x1b[0m");
        entity.data.insert(String::from("Hi"), ast::Hvalue::Hu8(5));
        return ast::Hvalue::None;
    }

    pub fn load_code_bytes(&mut self, code: Vec<u8>) -> u64 {
        let code_idx = self.next_code_idx;
        self.code.insert(code_idx, Arc::new(code.clone()));
        self.next_code_idx += 1;
        return code_idx;
    }

}

pub fn load_nodeman(node: Node, modpath: &str) -> Nodeman {
    let mut our_nodeman = Nodeman::new(node);
    our_nodeman.load_code_bytes(fs::read(modpath).unwrap());
    return our_nodeman;
}
