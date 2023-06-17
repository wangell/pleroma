use crate::common::{vec, Box, HashMap, String, Vec, BTreeMap};
use crate::vm_core;

#[derive(Debug, Clone)]
pub struct Module {
    pub imports: Vec<AstNode>,
    pub entity_defs: BTreeMap<String, AstNode>,
}

#[derive(Debug, Clone)]
pub enum AstNode {
    Module(Module),

    ImportModule(String),

    EntityDef(EntityDef),

    Function {
        name: String,
        parameters: Vec<(String, CType)>,
        return_type: CType,
        body: Vec<Box<AstNode>>,
    },

    DefinitionNode(Identifier, Box<AstNode>),
    AssignmentNode(Identifier, Box<AstNode>),

    FunctionCall(Identifier, Vec<AstNode>),
    Message(Identifier, String, Vec<AstNode>),

    ForeignCall(ForeignFunc, Vec<AstNode>),

    EntityConstruction(Identifier, Box<Option<AstNode>>),

    Return(Box<AstNode>),

    Await(Box<AstNode>),

    Print(Box<AstNode>),

    OperatorNode(Box<AstNode>, BinOp, Box<AstNode>),

    Index(Box<AstNode>, Box<AstNode>),

    Identifier(Identifier),

    ValueNode(Hvalue),

    Error,
}

#[derive(Clone)]
pub struct ForeignFunc {
    pub fc_fn: fn(&mut vm_core::Entity, Hvalue) -> Hvalue
}

#[derive(Clone, Debug)]
pub enum IdentifierTarget {
    Undecided,
    LocalVar,
    EntityVar,
    InocVar
}

#[derive(Clone, Debug)]
pub struct Identifier {
    pub original_sym: String,
    pub unique_sym: String,

    pub target: IdentifierTarget,
}

#[derive(Debug, Clone)]
pub struct EntityDef {
    pub name: String,
    pub data_declarations: Vec<(String, CType)>,
    pub inoculation_list: Vec<(String, CType)>,
    pub functions: HashMap<String, Box<AstNode>>,
    pub foreign_functions: HashMap<u8, ForeignFunc>
}

use core::fmt;
use core::fmt::Debug;
impl Debug for ForeignFunc {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Hi")
    }
}

impl EntityDef {
    pub fn register_foreign_function(
        &mut self,
        name: &String,
        func: fn(&mut vm_core::Entity, Hvalue) -> Hvalue,
    ) {
        let idx = self.foreign_functions.len() as u8;
        self.foreign_functions.insert(idx, ForeignFunc{fc_fn: func});
        self.functions.insert(
            name.clone(),
            Box::new(AstNode::Function {
                name: name.clone(),
                parameters: Vec::new(),
                return_type: CType::Loc(PType::Pu32),
                body: vec![Box::new(AstNode::ForeignCall(ForeignFunc{fc_fn: func}, Vec::new()))],
            }),
        );
    }
}

#[derive(Clone, Debug)]
pub enum Hvalue {
    None,
    PString(String),
    Hu8(u8),
    EntityAddress(vm_core::EntityAddress),
    List(Vec<AstNode>),
    Promise(u8),
}

#[derive(Copy, Clone, Debug)]
pub enum BinOp {
    Mul,
    Div,
    Add,
    Sub,
}

#[derive(Clone, Debug)]
pub enum Distance {
    Local,
    Far,
    Alien,
}

#[derive(Clone, Debug)]
pub enum PType {
    Void,
    Pu32,

    Pi32,

    Pstr,
    Pbool,
    Entity,
    List(Box<CType>),
    Promise(Box<CType>),
}

#[derive(Clone, Debug)]
pub enum CType {
    Loc(PType),
    Far(PType),
    Alien(PType),
    Void,
}

//pub struct EntityDef {
//    name: String,
//    inoculation_list: Vec<(String, CType)>
//}

pub trait AstNodeVisitor {
    fn visit_entity_def(
        &mut self,
        name: &String,
        data_declarations: &Vec<(String, CType)>,
        inoculation_list: &Vec<(String, CType)>,
        functions: &mut HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, ForeignFunc>,
    ) {
        for (func_name, func_def) in functions.iter_mut() {
            walk(self, func_def);
        }
    }

    fn visit_function(&mut self, name: &String, parameters: &mut Vec<(String, CType)>, return_type: &mut CType, body: &mut Vec<Box<AstNode>>) {
        for b in body.iter_mut() {
            walk(self, b);
        }
    }

    fn visit_return(&mut self, expr: &mut AstNode) {
        walk(self, expr);
    }

    fn visit_value(&mut self, value: &Hvalue) {
    }

    fn visit_operator(&mut self, left: &mut AstNode, op: &BinOp, right: &mut AstNode) {
        walk(self, left);
        walk(self, right);
    }

    fn visit_message(&mut self, id: &mut Identifier, func_name: &mut String, args: &mut Vec<AstNode>) {
        self.visit_identifier(id);
        for arg in args.iter_mut() {
            walk(self, arg);
        }
    }

    fn visit_await(&mut self, node: &mut AstNode) {
        walk(self, node);
    }

    fn visit_print(&mut self, node: &mut AstNode) {
        walk(self, node);
    }

    fn visit_module(&mut self, module: &mut Module) {
        for (ename, edef) in module.entity_defs.iter_mut() {
            walk(self, edef);
        }
    }

    fn visit_definition(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        self.visit_identifier(symbol);
        walk(self, expr);
    }

    fn visit_identifier(&mut self, symbol: &mut Identifier) {}

    fn visit_assignment(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        self.visit_identifier(symbol);
        walk(self, expr);
    }

    fn visit_foreign_call(
        &mut self,
        func: &fn(&mut vm_core::Entity, Hvalue) -> Hvalue,
        params: &mut Vec<AstNode>,
    ) {
        for p in params.iter_mut() {
            walk(self, p);
        }
    }

    fn visit_function_call(&mut self, identifier: &mut Identifier, arguments: &mut Vec<AstNode>) {
        for arg in arguments.iter_mut() {
            walk(self, arg);
        }
        self.visit_identifier(identifier);
    }
}

pub fn walk_return<V: AstNodeVisitor>(visitor: &mut V, expr: &mut AstNode) {
    walk(visitor, expr);
}

pub fn walk<V: AstNodeVisitor + ?Sized>(visitor: &mut V, node: &mut AstNode) {
    match node {
        AstNode::EntityDef(EntityDef {
            name,
            data_declarations,
            inoculation_list,
            functions,
            foreign_functions,
        }) => visitor.visit_entity_def(
            name,
            data_declarations,
            inoculation_list,
            functions,
            foreign_functions,
        ),
        AstNode::Function {
            name,
            parameters,
            return_type,
            body,
        } => visitor.visit_function(name, parameters, return_type, body),
        AstNode::Return(r) => visitor.visit_return(r),
        AstNode::ValueNode(v) => visitor.visit_value(v),
        AstNode::OperatorNode(a, o, b) => visitor.visit_operator(a, o, b),
        AstNode::ForeignCall(ForeignFunc{fc_fn}, b) => visitor.visit_foreign_call(fc_fn, b),
        AstNode::Message(a, b, c) => visitor.visit_message(a, b, c),
        AstNode::Await(a) => visitor.visit_await(a),
        AstNode::Print(a) => visitor.visit_print(a),

        AstNode::DefinitionNode(a, b) => visitor.visit_definition(a, b),
        AstNode::AssignmentNode(a, b) => visitor.visit_assignment(a, b),
        AstNode::Identifier(a) => visitor.visit_identifier(a),

        AstNode::Module(a) => visitor.visit_module(a),
        AstNode::FunctionCall(a, b) => visitor.visit_function_call(a, b),
        x => {
            println!("{:?}", x);
            panic!()
        }
    }
}
