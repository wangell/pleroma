use crate::common::{vec, Box, HashMap, String, Vec};
use crate::vm_core;

#[derive(Debug)]
pub struct Module {
    pub imports: Vec<String>,
    pub entity_defs: HashMap<String, AstNode>,
}

#[derive(Clone)]
pub enum AstNode {
    EntityDef(EntityDef),

    Function {
        name: String,
        parameters: Vec<(String, CType)>,
        return_type: CType,
        body: Vec<Box<AstNode>>,
    },

    DefinitionNode(String, Box<AstNode>),
    AssignmentNode(Identifier, Box<AstNode>),

    FunctionCall(Identifier, Vec<AstNode>),
    Message(Identifier, Box<Option<AstNode>>),

    ForeignCall(fn(&mut vm_core::Entity, Hvalue) -> Hvalue, Vec<AstNode>),

    EntityConstruction(Identifier, Box<Option<AstNode>>),

    Return(Box<AstNode>),

    OperatorNode(Box<AstNode>, BinOp, Box<AstNode>),

    Index(Box<AstNode>, Box<AstNode>),

    Identifier(Identifier),

    ValueNode(Hvalue),

    Error,
}

#[derive(Clone, Debug)]
pub struct Identifier {
    pub original_sym: String,
    pub unique_sym: String,
}

#[derive(Clone)]
pub struct EntityDef {
    pub name: String,
    pub data_declarations: Vec<(String, CType)>,
    pub inoculation_list: Vec<(String, CType)>,
    pub functions: HashMap<String, Box<AstNode>>,
    pub foreign_functions: HashMap<u8, fn(&mut vm_core::Entity, Hvalue) -> Hvalue>,
}

use core::fmt;
use core::fmt::Debug;
impl Debug for AstNode {
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
        self.foreign_functions.insert(idx, func);
        self.functions.insert(
            name.clone(),
            Box::new(AstNode::Function {
                name: name.clone(),
                parameters: Vec::new(),
                return_type: CType::Loc(PType::Pu32),
                body: vec![Box::new(AstNode::ForeignCall(func, Vec::new()))],
            }),
        );
    }
}

#[derive(Clone, Debug)]
pub enum Hvalue {
    None,
    PString(String),
    Pu8(u8),
    EntityAddress(vm_core::EntityAddress),
    List(Vec<AstNode>),
    Promise
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

pub trait AstNodeVisitor<T> {
    fn visit_entity_def(
        &mut self,
        name: &String,
        data_declarations: &Vec<(String, CType)>,
        inoculation_list: &Vec<(String, CType)>,
        functions: &HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, fn(&mut vm_core::Entity, Hvalue) -> Hvalue>,
    ) -> T;
    fn visit_function(&mut self, name: &String, body: &Vec<Box<AstNode>>) -> T;
    fn visit_return(&mut self, expr: &AstNode) -> T;
    fn visit_value(&mut self, value: &Hvalue) -> T;
    fn visit_operator(&mut self, left: &AstNode, op: &BinOp, right: &AstNode) -> T;
    fn visit_message(&mut self) -> T;
    fn visit_foreign_call(
        &mut self,
        func: &fn(&mut vm_core::Entity, Hvalue) -> Hvalue,
        params: &Vec<AstNode>,
    ) -> T;
    //fn visit_function_call(&mut self, identifier: &Identifier, arguments: &Vec<AstNode>) -> T;
}

impl AstNode {
    pub fn visit<T>(&self, visitor: &mut dyn AstNodeVisitor<T>) -> T {
        match self {
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
            } => visitor.visit_function(name, body),
            AstNode::Return(r) => visitor.visit_return(r),
            AstNode::ValueNode(v) => visitor.visit_value(v),
            AstNode::OperatorNode(a, o, b) => visitor.visit_operator(a, o, b),
            AstNode::ForeignCall(a, b) => visitor.visit_foreign_call(a, b),
            AstNode::Message(_, _) => visitor.visit_message(),
            x => {
                println!("{:?}", x);
                panic!()
            }
        }
    }
}
