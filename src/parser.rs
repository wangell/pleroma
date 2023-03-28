lalrpop_mod!(pub hylic_lalr);

use crate::ast;
use crate::ast::AstNode;
use crate::lexer;

pub fn parse(s: &str) -> ast::Module {
    let expr = hylic_lalr::ModuleParser::new()
        .parse(lexer::Lexer::new(s))
        .unwrap();

    return expr;
}
