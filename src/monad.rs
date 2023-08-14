use crate::ast;
use crate::ast::AstNode;
use crate::nested_hashmap;
use crate::codegen;
use crate::pbin;
use crate::compile;
use crate::opcodes;
use crate::parser;
use crate::vm_core;
use crate::vm_core::Vat;
use crate::common::BTreeMap;
use crate::common::HashMap;
use crate::common::Arc;
use crate::{node, nodeman};

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

    fn kprint(vat: &mut vm_core::Vat, entity_id: u32, args: ast::Hvalue) -> ast::Hvalue {

        if let ast::Hvalue::List(ls) = args {
            for n in ls {
                let out_str = match n {
                    ast::AstNode::ValueNode(ast::Hvalue::PString(s)) => s,
                    _ => todo!()
                };
                println!("[Monad => kprint] {:?}", out_str);
            }
        }
        return ast::Hvalue::None;
    }

    fn hello(vat: &mut vm_core::Vat, entity_id: u32, args: ast::Hvalue) -> ast::Hvalue {
        println!(r#"
            ,,
`7MM"""Mq.`7MM
  MM   `MM. MM
  MM   ,M9  MM  .gP"Ya `7Mb,od8 ,pW"Wq.`7MMpMMMb.pMMMb.   ,6"Yb.
  MMmmdM9   MM ,M'   Yb  MM' "'6W'   `Wb MM    MM    MM  8)   MM
  MM        MM 8M""""""  MM    8M     M8 MM    MM    MM   ,pm9MM
  MM        MM YM.    ,  MM    YA.   ,A9 MM    MM    MM  8M   MM
.JMML.    .JMML.`Mbmmd'.JMML.   `Ybmd9'.JMML  JMML  JMML.`Moo9^Yo.
"#);

        println!("\x1b[0;32mHello, welcome to Pleroma!\x1b[0m");

        return ast::Hvalue::None;
    }
}

pub fn load_monad(monad_path: &str) -> Vat {

    let monad_bindings = nested_hashmap! {
        "Monad" => {
            "hello" => (Monad::hello, Vec::new()),
            "kprint" => (Monad::kprint, vec![(String::from("p0"), ast::CType::Void)])
        }
    };


    let monad_code = compile::compile_from_files(vec![String::from("sys/kernel.plm")], "sys/kernel.plmb", monad_bindings);
    let mut z = 0;
    let data_table = pbin::load_entity_data_table(&mut z, &monad_code);

    let mut vat = vm_core::Vat::new(1, Arc::new(monad_code.clone()));
    vat.create_entity_code(0, 0, &data_table[&0]);

    vat
}
