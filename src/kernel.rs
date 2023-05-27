use crate::ast;
use crate::ast::AstNode;
use crate::codegen;
use crate::compile;
use crate::opcodes;
use crate::parser;
use crate::vm_core;

use std::fs;

use std::collections::HashMap;

pub struct Program {
    name: String,
    code: Vec<u8>,
}

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

    pub fn start_nodeman(&mut self) {
        //let mut nodeman = kernel::Nodeman::new();
        //load_nodeman(&mut nodeman);
    }

    // Enlists current Monad into the cluster, becomes passive until election
    pub fn enlist() {}
}

pub struct Nodeman<'a> {
    pub node: &'a mut Node,
    pub def: ast::EntityDef,
}

impl Nodeman<'_> {
    pub fn new(node: &mut Node) -> Nodeman {
        let mut n = Nodeman {
            node: node,
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

}

pub fn load_nodeman(nodeman: &Nodeman) {
    let contents = fs::read_to_string("./test_examples/basic_entity.plm")
        .expect("Should have been able to read the file");
    let mut module = parser::parse_module(contents.as_str());

    let mut nodedef = nodeman.def.clone();

    //let real_def = module.entity_defs.get_mut("Nodeman").unwrap();
    //if let ast::AstNode::EntityDef(d) = real_def {
    //    //d.register_foreign_function(&String::from("test"), Nodeman::hello);
    //}

    //let mut root = ast::Root{
    //    modules : HashMap::new(),
    //    external_modules: HashMap::new()
    //};

    //compile::compile(&mut root);
}

pub fn load_kernel(monad: &Monad) {
    compile::compile_to_file(
        &String::from("./test_examples/basic_entity.plm"),
        &String::from("kernel.plmb"),
    );
}
