use crate::ast;
use crate::ast::AstNode;
use crate::codegen;
use crate::compile;
use crate::opcodes;
use crate::parser;
use crate::vm_core;
use crate::common::BTreeMap;

use std::fs;

use std::collections::HashMap;

pub struct Program {
    name: String,
    code: Vec<u8>,
}

pub struct Monad {
    pub programs: HashMap<String, Program>,
    pub def: ast::EntityDef,
}

impl Monad {
    pub fn new() -> Monad {
        let mut n = Monad {
            programs: HashMap::new(),
            def: ast::EntityDef {
                name: String::from("Monad"),
                data_declarations: Vec::new(),
                inoculation_list: Vec::new(),
                functions: HashMap::new(),
                foreign_functions: HashMap::new(),
            },
        };

        n
    }
}

pub fn load_kernel(monad: &Monad) {
}
