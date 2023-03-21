lalrpop_mod!(pub hylic_lalr);

use crate::ast;
use crate::ast::AstNode;

pub fn parse(s: &str) -> ast::Module {
    let expr = hylic_lalr::ModuleParser::new()
        .parse(s)
        .unwrap();

    return expr;
}
