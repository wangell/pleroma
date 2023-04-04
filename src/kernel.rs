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
        entity.data.insert(String::from("Hi"), ast::Hvalue::Pu8(5));
        return ast::Hvalue::None;
    }

}

pub fn load_nodeman(nodeman: &Nodeman) {
    let contents = fs::read_to_string("./test_examples/basic_entity.plm")
        .expect("Should have been able to read the file");
    let mut module = parser::parse(contents.as_str());

    let mut cg_visitor = codegen::GenCode {
        header: Vec::new(),
        code: Vec::new(),
        entity_function_locations: HashMap::new(),
        current_entity_id: 0,
        current_func_id: 0,
    };

    let mut nodedef = nodeman.def.clone();


    //module
    //    .entity_defs
    //    .insert(String::from("Nodeman"), ast::AstNode::EntityDef(nodedef));
    let real_def = module.entity_defs.get_mut("Nodeman").unwrap();
    if let ast::AstNode::EntityDef(d) = real_def {
        //d.register_foreign_function(&String::from("test"), Nodeman::hello);
    }

    for (entity_name, entity_def) in &module.entity_defs {
        let result = entity_def.visit(&mut cg_visitor);
        println!("{:?}", cg_visitor.entity_function_locations);

        cg_visitor.build_entity_function_location_table();

        let mut complete_output: Vec<u8> = cg_visitor.header.clone();
        complete_output.append(&mut cg_visitor.code);
        complete_output.push(opcodes::Op::RetExit as u8);
        fs::write(format!("kernel.plmb"), &complete_output).unwrap();
    }
}

pub fn load_kernel(monad: &Monad) {
    compile::compile_to_file(
        &String::from("./test_examples/basic_entity.plm"),
        &String::from("kernel.plmb"),
    );
}
