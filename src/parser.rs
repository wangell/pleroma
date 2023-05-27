lalrpop_mod!(pub hylic_lalr);

use crate::ast;
use crate::ast::AstNode;
use crate::lexer;
use std::fs;
use crate::common::{vec, Box, HashMap, String, Vec, BTreeMap};

pub fn parse_root(directory_path: &str) -> AstNode {
    let paths = fs::read_dir(directory_path).unwrap();

    let mut root = ast::Root{
        modules: BTreeMap::new(),
        external_modules: BTreeMap::new(),
    };

    for path in paths {
        println!("Compiling {:?}", path);
        let base_path = path.unwrap().path();
        let mod_str = fs::read_to_string(base_path.clone()).unwrap();
        let module = parse_module(&mod_str);
        //println!("{:#?}", module);
        root.modules.insert(base_path.file_stem().unwrap().to_str().unwrap().to_string(), module);
    }
    println!("Done");

    AstNode::Root(root)
}

pub fn parse_module(s: &str) -> ast::AstNode {
    let expr = hylic_lalr::ModuleParser::new()
        .parse(lexer::Lexer::new(s))
        .unwrap();

    return expr;
}
