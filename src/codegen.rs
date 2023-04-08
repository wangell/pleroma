use crate::ast;
use crate::ast::walk;
use crate::ast::{AstNode, AstNodeVisitor, BinOp, Hvalue, Identifier, IdentifierTarget};
use crate::opcodes::{encode_instruction, Op};
use crate::vm_core;

use crate::common::{Box, HashMap, String, Vec};

pub struct GenCode {
    pub header: Vec<u8>,
    pub code: Vec<u8>,

    pub entity_function_locations: HashMap<u32, HashMap<u32, (usize, usize)>>,
    pub current_entity_id: u32,
    pub current_func_id: u32,
}

pub struct VariableFlow {
    pub local_vars: HashMap<String, ()>,
}

impl AstNodeVisitor for VariableFlow {
    fn visit_definition(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        self.local_vars.insert(symbol.original_sym.clone(), ());

        symbol.target = IdentifierTarget::LocalVar;

        walk(self, expr);
    }

    fn visit_identifier(&mut self, symbol: &mut Identifier) {
        if self.local_vars.contains_key(&symbol.original_sym) {
            symbol.target = IdentifierTarget::LocalVar;
        } else {
            symbol.target = IdentifierTarget::EntityVar;
        }
    }
}

impl GenCode {
    fn emit_bytes_vec(l: &mut Vec<u8>, bytes: &mut Vec<u8>) {
        l.append(bytes);
    }

    fn emit_bytes_slice(l: &mut Vec<u8>, bytes: &[u8]) {
        l.extend_from_slice(bytes);
    }

    fn emit_usize(l: &mut Vec<u8>, v: &usize) {
        Self::emit_bytes_slice(l, &v.to_be_bytes());
    }

    fn emit_u64(l: &mut Vec<u8>, v: &u64) {
        Self::emit_bytes_slice(l, &v.to_be_bytes());
    }

    fn emit_u32(l: &mut Vec<u8>, v: &u32) {
        Self::emit_bytes_slice(l, &v.to_be_bytes());
    }

    fn emit_u16(l: &mut Vec<u8>, v: &u16) {
        Self::emit_bytes_slice(l, &v.to_be_bytes());
    }

    fn emit_u8(l: &mut Vec<u8>, v: u8) {
        l.push(v);
    }

    pub fn build_entity_function_location_table(&mut self) {
        let mut sz = 0;
        for (ent_id, ftab) in &self.entity_function_locations {
            for (func_id, loc) in ftab {
                sz += 1;
            }
        }

        for (ent_id, ftab) in &self.entity_function_locations {
            for (func_id, (loc_start, fun_size)) in ftab {
                self.header.push(*ent_id as u8);
                self.header.push(*func_id as u8);
                // Table size byte + table size
                // TODO: make this u64
                Self::emit_u16(&mut self.header, &((*loc_start + 1 + sz * 6) as u16));
                Self::emit_u16(&mut self.header, &(*fun_size as u16));
            }
        }

        // Insert size of table at the start
        self.header.insert(0, sz as u8);
    }

    fn emit_op(&mut self, op: Op) {
        Self::emit_bytes_vec(&mut self.code, &mut encode_instruction(&op));
    }
}

impl AstNodeVisitor for GenCode {
    fn visit_entity_def(
        &mut self,
        name: &String,
        data_declarations: &Vec<(String, ast::CType)>,
        inoculation_list: &Vec<(String, ast::CType)>,
        functions: &mut HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, fn(&mut vm_core::Entity, Hvalue) -> Hvalue>,
    ) {
        //let mut sorted_functions: Vec<(&String, &Box<AstNode>)> = functions.iter().collect();
        //sorted_functions.sort_by(|a, b| a.0.cmp(b.0));

        for (func_name, func_def) in functions.iter_mut() {
            walk(self, func_def);
        }

        self.current_entity_id += 1;
    }

    fn visit_foreign_call(
        &mut self,
        func: &fn(&mut vm_core::Entity, Hvalue) -> Hvalue,
        params: &mut Vec<AstNode>,
    ) {
        self.emit_op(Op::ForeignCall(*func as u64));
        self.emit_op(Op::Ret);
    }

    fn visit_function(&mut self, name: &String, body: &mut Vec<Box<AstNode>>) {
        let mut fun_size: (usize, usize) = (self.code.len(), 0);

        for n in body.iter_mut() {
            walk(self, n);
        }

        fun_size.1 = self.code.len() - fun_size.0;

        self.entity_function_locations
            .entry(self.current_entity_id)
            .or_insert(HashMap::new());
        self.entity_function_locations
            .get_mut(&self.current_entity_id)
            .unwrap()
            .insert(self.current_func_id, fun_size);

        self.current_func_id += 1;
    }

    fn visit_return(&mut self, expr: &mut AstNode) {
        //self.emit_op(Op::Lb8);
        //self.emit_u8(0);

        //expr.visit(self);
        walk(self, expr);
        self.emit_op(Op::Ret);
    }

    fn visit_message(&mut self) {
        self.emit_op(Op::Message);
    }

    fn visit_operator(&mut self, left: &mut AstNode, op: &BinOp, right: &mut AstNode) {
        //left.visit(self);
        //right.visit(self);

        walk(self, left);
        walk(self, right);

        match op {
            BinOp::Add => self.emit_op(Op::Add),
            BinOp::Sub => self.emit_op(Op::Sub),
            BinOp::Mul => self.emit_op(Op::Mul),
            BinOp::Div => self.emit_op(Op::Div),
        }
    }

    fn visit_resolve(&mut self) {
        //self.emit_op(Op::Resolve);
    }

    fn visit_definition(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        walk(self, expr);

        if let IdentifierTarget::LocalVar = symbol.target {
            self.emit_op(Op::Lstore(symbol.original_sym.clone()));
        } else {
            self.emit_op(Op::Estore(symbol.original_sym.clone()));
        }
    }

    fn visit_assignment(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {}

    fn visit_print(&mut self, node: &mut AstNode) {
        //self.emit_op(Op::Print);
        //Self::emit_u8(&mut self.code, 4);
    }

    fn visit_value(&mut self, v: &Hvalue) {
        self.emit_op(Op::Push(v.clone()));
    }
}
