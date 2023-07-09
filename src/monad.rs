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

use std::fs;

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

pub fn load_monad(monad_path: &str, vat: &mut Vat) {
    vat.code.insert(0, fs::read("kernel.plmb").unwrap());
    let mut z = 0;
    let data_table = pbin::load_entity_data_table(&mut z, &fs::read("kernel.plmb").unwrap());
    vat.create_entity_code(0, 0, &data_table[&0]);
}
