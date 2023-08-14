use crate::ast;
use crate::ast::walk;
use crate::ast::{AstNode, AstNodeVisitor, BinOp, Hvalue, Identifier, IdentifierTarget, CType};
use crate::opcodes::{encode_instruction, encode_value, Op};
use crate::vm_core;

use crate::common::{Box, HashMap, String, Vec, BTreeMap};

pub fn derive_type(node: &AstNode) -> CType {
    //println!("{:?}", node);
    let stype = match node {
        AstNode::FunctionCall(ast::FunctionCall{call_type, identifier, func_name, arguments}) => match call_type {
            ast::CallType::NewEntity => CType::Loc(ast::PType::Entity(identifier.original_sym.clone())),
            _ => panic!()
        }
        _ => CType::Void
    };

    return stype;
}

// This pass gathers all Entity types + function parameter/return types
pub struct GatherModuleTypes {
    pub entity_function_types: HashMap<String, HashMap<String, (Vec<CType>, CType)>>,
    pub entities_and_functions: HashMap<String, HashMap<String, u32>>
}

pub struct VariableFlow {
    pub inoc_vars: HashMap<String, ()>,
    pub entity_vars: HashMap<String, ()>,
    pub local_vars: HashMap<String, ()>,
}

pub struct GenCode {
    pub header: Vec<u8>,
    pub code: Vec<u8>,

    pub entities_and_functions: HashMap<String, HashMap<String, u32>>,

    pub entity_function_locations: BTreeMap<u32, HashMap<u32, (usize, usize)>>,
    pub entity_data_values: HashMap<u32, HashMap<String, Hvalue>>,
    pub entity_inoculation_values: HashMap<u32, HashMap<String, Hvalue>>,

    pub entity_function_types: HashMap<String, HashMap<String, (Vec<CType>, CType)>>,

    pub current_entity_id: u32,
    pub current_func_id: u32,

    pub function_num: HashMap<String, u32>,

    pub symbol_table: HashMap<String, CType>,

    pub absolute_entity_function_locations: BTreeMap<(u32, u32), usize>,
    pub relocations: Vec<(u32, u32, u64)>,

    // Stores entity root name -> unique u32
    pub entity_table: HashMap<String, u32>
}

impl AstNodeVisitor for GatherModuleTypes {
    fn visit_entity_def(
        &mut self,
        entity_name: &String,
        data_declarations: &Vec<(String, ast::CType)>,
        inoculation_list: &Vec<(String, ast::CType)>,
        functions: &mut HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, ast::ForeignFunc>,
    ) {
        self.entity_function_types.insert(entity_name.clone(), HashMap::new());
        self.entities_and_functions.insert(entity_name.clone(), HashMap::new());

        for (func_name, func_def) in functions.iter_mut() {
            if let ast::AstNode::Function{name, parameters, return_type, body} = &**func_def {
                self.entity_function_types.get_mut(entity_name).unwrap().insert(func_name.clone(), (Vec::new(), return_type.clone()));
            }
            walk(self, func_def);
        }

        {
            let mut sorted_functions: Vec<(&String, &mut Box<AstNode>)> = functions.iter_mut().collect();
            sorted_functions.sort_by(compare_function_names);

            let mut fid = 0;
            for (func_name, func_def) in sorted_functions {
                self.entities_and_functions.get_mut(entity_name).unwrap().insert(func_name.clone(), fid);
                fid += 1;
            }
        }
    }
}

impl AstNodeVisitor for VariableFlow {
    fn visit_entity_def(
        &mut self,
        name: &String,
        data_declarations: &Vec<(String, ast::CType)>,
        inoculation_list: &Vec<(String, ast::CType)>,
        functions: &mut HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, ast::ForeignFunc>,
    ) {
        self.entity_vars = HashMap::new();

        // Implicit self var
        self.entity_vars.insert(String::from("self"), ());

        // For each inoc, emit inoc_request message to Monad(Nodeman for caching?) in create preamble
        // Create ->
        //   Inoc Request for each type (need await? Emit await? Handle promise chaining?)
        //   Setup entity data with default/consts
        //   Load arguments
        //   User supplied function body
        for (data_dec, data_type) in inoculation_list.iter() {
            self.inoc_vars.insert(data_dec.clone(), ());
        }

        for (data_dec, data_type) in data_declarations.iter() {
            self.entity_vars.insert(data_dec.clone(), ());
        }

        for (func_name, func_def) in functions.iter_mut() {
            self.local_vars = HashMap::new();
            walk(self, func_def);
        }
    }

    fn visit_function(&mut self, name: &String, parameters: &mut Vec<(String, CType)>, return_type: &mut CType, body: &mut Vec<Box<AstNode>>) {
        for p in parameters {
            self.local_vars.insert(p.0.clone(), ());
        }

        for b in body.iter_mut() {
            walk(self, b);
        }
    }

    fn visit_definition(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        self.local_vars.insert(symbol.original_sym.clone(), ());

        symbol.target = IdentifierTarget::LocalVar;

        walk(self, expr);
    }

    fn visit_identifier(&mut self, symbol: &mut Identifier) {
        if self.local_vars.contains_key(&symbol.original_sym) {
            symbol.target = IdentifierTarget::LocalVar;
        } else if self.entity_vars.contains_key(&symbol.original_sym) {
            symbol.target = IdentifierTarget::EntityVar;
        } else if self.inoc_vars.contains_key(&symbol.original_sym) {
            symbol.target = IdentifierTarget::InocVar;
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

    pub fn relocate_functions(&mut self) {
        for (ent_id, func_id, loc) in &self.relocations {
            let new_loc = self.absolute_entity_function_locations[&(*ent_id, *func_id)];
            self.code.splice(*loc as usize..(loc+8) as usize, new_loc.to_be_bytes());
        }
    }

    // Creates a table listing each inoculation value
    pub fn build_entity_inoculation_table(&mut self) {
        // Outputs binary: size (u8) + each(entity) -> entity_id, name, type
        let mut sz = 0;

        for (ent_id, ftab) in &self.entity_inoculation_values {
            for (data_id, val) in ftab {
                sz += 1;
            }
        }

        let mut edt: Vec<u8> = Vec::new();
        edt.push(sz as u8);

        for (ent_id, ftab) in &self.entity_inoculation_values {
            for (data_id, val) in ftab {
                // TODO: not u8
                edt.push(*ent_id as u8);

                edt.extend_from_slice(data_id.as_bytes());
                edt.push(0x0);

                // Value
                edt.append(&mut encode_value(&Hvalue::Hu8(4)));
            }
        }

        self.header.append(&mut edt);
    }

    // Creates a table for each entity that lists all data values and any compile-time constants assigned
    pub fn build_entity_data_table(&mut self) {
        let mut sz = 0;

        for (ent_id, ftab) in &self.entity_data_values {
            for (data_id, val) in ftab {
                sz += 1;
            }
        }

        let mut edt: Vec<u8> = Vec::new();
        edt.push(sz as u8);

        for (ent_id, ftab) in &self.entity_data_values {
            for (data_id, val) in ftab {
                // TODO: not u8
                edt.push(*ent_id as u8);

                edt.extend_from_slice(data_id.as_bytes());
                edt.push(0x0);

                // Value
                edt.append(&mut encode_value(&Hvalue::Hu8(4)));
            }
        }

        self.header.append(&mut edt);
    }

    // Creates a table listing each entity function and the position-independent code for this object
    // Ouputs entity ID -> (start, size)
    pub fn build_entity_function_location_table(&mut self) {
        let loc_offset = self.header.len();

        let mut sz = 0;
        for (ent_id, ftab) in &self.entity_function_locations {
            for (func_id, loc) in ftab {
                // FIXME u64 = +8
                sz += 1;
            }
        }

        let mut efl: Vec<u8> = Vec::new();
        efl.push(sz as u8);

        for (ent_id, ftab) in &self.entity_function_locations {
            for (func_id, (loc_start, fun_size)) in ftab {
                efl.push(*ent_id as u8);
                efl.push(*func_id as u8);
                // Table size byte + table size
                // TODO: make this u64
                let final_loc_start = (*loc_start + 1 + sz * 6 + loc_offset) as u16;

                self.absolute_entity_function_locations.insert((*ent_id, *func_id), final_loc_start as usize);

                Self::emit_u16(&mut efl, &(final_loc_start));
                Self::emit_u16(&mut efl, &(*fun_size as u16));
            }
        }

        // Insert size of table at the start
        //self.header.insert(0, sz as u8);
        self.header.append(&mut efl);
    }

    fn emit_op(&mut self, op: Op) {
        Self::emit_bytes_vec(&mut self.code, &mut encode_instruction(&op));
    }
}

use core::cmp::Ordering;

// Sort functions so that the constructor always comes first
fn compare_function_names(a: &(&String, &mut Box<AstNode>), b: &(&String, &mut Box<AstNode>)) -> Ordering {
    if a.0 == "create" {
        return Ordering::Less;
    }

    if b.0 == "create" {
        return Ordering::Greater;
    }

    return a.0.cmp(b.0);
}

impl AstNodeVisitor for GenCode {
    fn visit_entity_def(
        &mut self,
        name: &String,
        data_declarations: &Vec<(String, ast::CType)>,
        inoculation_list: &Vec<(String, ast::CType)>,
        functions: &mut HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, ast::ForeignFunc>,
    ) {
        self.symbol_table.clear();

        self.current_func_id = 0;

        self.symbol_table.insert(String::from("self"), ast::CType::Loc(ast::PType::Entity(name.clone())));

        let mut data_map: HashMap<String, Hvalue> = HashMap::new();
        for (data_name, data_type) in data_declarations {
            data_map.insert(data_name.clone(), Hvalue::None);
            self.symbol_table.insert(data_name.clone(), data_type.clone());
        }
        self.entity_data_values
            .insert(self.current_entity_id, data_map);

        let mut inoc_map: HashMap<String, Hvalue> = HashMap::new();
        for (data_name, data_type) in inoculation_list {
            inoc_map.insert(data_name.clone(), Hvalue::None);
        }
        self.entity_inoculation_values.insert(self.current_entity_id, inoc_map);

        {
            let mut sorted_functions: Vec<(&String, &mut Box<AstNode>)> =
                functions.iter_mut().collect();
            sorted_functions.sort_by(compare_function_names);

            let mut fid = 0;
            for (func_name, func_def) in sorted_functions {
                self.function_num.insert(func_name.clone(), fid);
                fid += 1;
            }
        }

        let mut sorted_functions: Vec<(&String, &mut Box<AstNode>)> =
            functions.iter_mut().collect();
        sorted_functions.sort_by(compare_function_names);

        for (func_name, func_def) in sorted_functions {
            walk(self, func_def);
        }

        self.current_entity_id += 1;
    }

    fn visit_foreign_call(
        &mut self,
        func: &ast::RawFF,
        params: &mut Vec<(String, CType)>,
    ) {
        //let mut q = 0;
        //for p in params {
        //    self.emit_op(Op::Lload(p.0.clone()));
        //    q += 1;
        //}

        self.emit_op(Op::ForeignCall(*func as u64, params.len() as u8));
        self.emit_op(Op::Ret);
    }

    fn visit_function(&mut self, name: &String, parameters: &mut Vec<(String, CType)>, return_type: &mut CType, body: &mut Vec<Box<AstNode>>) {
        let mut fun_size: (usize, usize) = (self.code.len(), 0);

        // TODO: Remove when moving to local idx
        // All arguments are pushed onto op stack, pop + estore number of arguments
        for p in parameters {
            self.emit_op(Op::Lstore(p.0.clone()));
        }

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
        walk(self, expr);
        self.emit_op(Op::Ret);
    }

    fn visit_message(
        &mut self,
        id: &mut Identifier,
        func_name: &mut String,
        args: &mut Vec<AstNode>,
    ) {
        let mut f_args = 0;
        self.visit_identifier(id);
        for arg in args.iter_mut() {
            walk(self, arg);
            f_args += 1;
        }

        //println!("{:#?}", self.symbol_table);
        let entity_type = self.symbol_table.get(&id.original_sym).unwrap();

        // We have the entity type + function name, now we need to find which number function to message
        if let ast::CType::Loc(ast::PType::Entity(ename)) = entity_type {

            let fid = self.entities_and_functions.get(ename).unwrap().get(func_name).unwrap();
            //let f_args = self.entity_function_types.get(ename).unwrap().get(func_name).unwrap().0.len();

            self.emit_op(Op::Message(*fid as u64, f_args as u8));

        }

    }

    fn visit_operator(&mut self, left: &mut AstNode, op: &BinOp, right: &mut AstNode) {

        walk(self, left);
        walk(self, right);

        match op {
            BinOp::Add => self.emit_op(Op::Add),
            BinOp::Sub => self.emit_op(Op::Sub),
            BinOp::Mul => self.emit_op(Op::Mul),
            BinOp::Div => self.emit_op(Op::Div),
        }
    }

    fn visit_await(&mut self, node: &mut AstNode) {
        walk(self, node);
        self.emit_op(Op::Await);
    }

    fn visit_definition(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        walk(self, expr);

        self.symbol_table.insert(symbol.original_sym.clone(), derive_type(expr));

        if let IdentifierTarget::LocalVar = symbol.target {
            self.emit_op(Op::Lstore(symbol.original_sym.clone()));
        } else if let IdentifierTarget::EntityVar = symbol.target {
            self.emit_op(Op::Estore(symbol.original_sym.clone()));
        } else {
            panic!();
        }
    }

    fn visit_identifier(&mut self, symbol: &mut Identifier) {
        if let IdentifierTarget::LocalVar = symbol.target {
            self.emit_op(Op::Lload(symbol.original_sym.clone()));
        } else {
            self.emit_op(Op::Eload(symbol.original_sym.clone()));
        }
    }

    fn visit_assignment(&mut self, symbol: &mut Identifier, expr: &mut AstNode) {
        walk(self, expr);

        if let IdentifierTarget::LocalVar = symbol.target {
            self.emit_op(Op::Lstore(symbol.original_sym.clone()));
        } else if let IdentifierTarget::EntityVar = symbol.target {
            self.emit_op(Op::Estore(symbol.original_sym.clone()));
        } else {
            panic!();
        }
    }

    fn visit_function_call(&mut self, fc: &mut ast::FunctionCall) {
        for arg in fc.arguments.iter_mut() {
            walk(self, arg);
        }
        //let floc = self.entity_function_locations[&self.current_entity_id][&fnum].0 as u64;

        if fc.call_type == ast::CallType::NewEntity {
            let fnum = 0;
            self.emit_op(Op::ConstructEntity(self.entity_table[&fc.identifier.original_sym], fc.arguments.len() as u8));
        } else {
            // Entity, function, location
            self.visit_identifier(&mut fc.identifier);
            let fnum = self.function_num[&fc.func_name];
            self.relocations.push((self.current_entity_id, fnum, (self.code.len() + 1) as u64));
            self.emit_op(Op::Call(0, fc.arguments.len() as u8));
        }
    }

    fn visit_print(&mut self, node: &mut AstNode) {
        //self.emit_op(Op::Print);
        //Self::emit_u8(&mut self.code, 4);
    }

    fn visit_value(&mut self, v: &Hvalue) {
        self.emit_op(Op::Push(v.clone()));
    }
}
