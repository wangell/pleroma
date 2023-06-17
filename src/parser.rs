lalrpop_mod!(pub hylic_lalr);

use crate::ast;
use crate::ast::AstNode;
use crate::lexer;
use std::fs;
use crate::common::{vec, Box, HashMap, String, Vec, BTreeMap};

pub fn parse_module(s: &str) -> ast::AstNode {
    let expr = hylic_lalr::ModuleParser::new()
        .parse(lexer::Lexer::new(s))
        .unwrap();

    return expr;
}
