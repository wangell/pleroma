use crate::ast;
use crate::ast::{AstNode, Identifier, Module, Value};
use crate::common::{HashMap, String, Box, Vec, vec, str};

pub struct ScopeTree {
    pub symbols: HashMap<String, Box<AstNode>>,
    pub parent: Option<Box<ScopeTree>>,
}

impl ScopeTree {
    pub fn new(seed_parent: Option<Box<ScopeTree>>) -> ScopeTree {
        ScopeTree {
            symbols: HashMap::new(),
            parent: seed_parent,
        }
    }

    pub fn find_symbol(&self, s: &String) -> &Box<AstNode> {
        return &self.symbols[s];
    }
}

//pub fn scope_and_bind(scope_ctx: &ScopeTree, node: &mut AstNode) {
//    let res = match node {
//        AstNode::Function{name: name, parameters: p, return_type: rt, body: ref mut b} => b.iter_mut().map(|x| randomize_identifiers(n, x)).collect(),
//        AstNode::FunctionCall(ref mut id, b) => {
//            id.unique_sym = format!("{}_{}", id.original_sym, n);
//
//            for q in b {
//                randomize_identifiers(n, q);
//            }
//        },
//        AstNode::Index(a, b) => {
//            randomize_identifiers(n, a);
//            randomize_identifiers(n, b);
//        },
//        AstNode::Identifier(ref mut id) => {
//            id.unique_sym = format!("{}_{}", id.original_sym, n)
//        },
//        x => ()
//    };
//}
