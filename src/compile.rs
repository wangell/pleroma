use crate::ast;
use crate::ast::walk;
use crate::ast::AstNode;
use crate::ast::AstNodeVisitor;
use crate::codegen;
use crate::opcodes;
use crate::parser;
use crate::pbin;

use crate::common::{BTreeMap, Box, HashMap, String, Vec};

use std::fs;

pub struct ConstructSymbolTable {
    pub entity_ids: HashMap<String, u32>,
    pub current_id: u32,
}

impl ConstructSymbolTable {
    fn new() -> Self {
        Self {
            entity_ids: HashMap::new(),
            current_id: 0
        }
    }
}

impl AstNodeVisitor for ConstructSymbolTable {
    fn visit_entity_def(
        &mut self,
        name: &String,
        data_declarations: &Vec<(String, ast::CType)>,
        inoculation_list: &Vec<(String, ast::CType)>,
        functions: &mut HashMap<String, Box<AstNode>>,
        foreign_functions: &HashMap<u8, ast::ForeignFunc>,
    ) {
        self.entity_ids.insert(name.clone(), self.current_id);
        self.current_id += 1;
    }
}

// Visits all function calls and replaces relevant with an Entity/Vat constructor
pub struct EmplaceEntityConstruction {
}

impl AstNodeVisitor for EmplaceEntityConstruction {
    fn visit_function_call(&mut self, fc: &mut ast::FunctionCall) {
        for arg in fc.arguments.iter_mut() {
            walk(self, arg);
        }

        match fc.identifier.original_sym.chars().next() {
            Some(c) => {
                if c.is_uppercase() {
                    fc.call_type = ast::CallType::NewEntity;
                }
            },
            None => ()
        }
    }
}

pub fn compile_from_files(files: Vec<String>, outpath: &str) {
    let mut asts: HashMap<String, ast::AstNode> = HashMap::new();
    for file in files {
        let mod_str = fs::read_to_string(file.clone()).unwrap();
        asts.insert(file.clone(), parser::parse_module(&mod_str));
    }

    compile_from_ast(&asts, outpath);
}

//pub fn link_objects(objects: Vec<(Vec<u8>, Vec)>) {
//    for (ent_id, func_id, loc) in &self.relocations {
//        let new_loc = self.absolute_entity_function_locations[&(*ent_id, *func_id)];
//        // TODO: what is 8?
//        self.code.splice(*loc as usize..(loc+8) as usize, new_loc.to_be_bytes());
//    }
//}

// 1. Parse every file
// 2. Rename all symbols to something unique
// 3. Replace each symbol with the root-based reference -> import foo; foo.bar; -> import root.foo; root.foo.bar
// 4.

pub fn compile_from_ast(asts: &HashMap<String, ast::AstNode>, outpath: &str) {

    let mut new_asts = asts.clone();

    let mut root = &mut new_asts.get_mut("sys/kernel.plm").unwrap();

    let mut ent_ids_visitor = ConstructSymbolTable::new();
    let ent_ids_result = ast::walk(&mut ent_ids_visitor, root);

    let mut ec_visitor = EmplaceEntityConstruction {};

    let mut eft_visitor = codegen::GatherModuleTypes {
        entity_function_types: HashMap::new(),
        entities_and_functions: HashMap::new()
    };

    let ec_result = ast::walk(&mut ec_visitor, root);

    let mut vf_visitor = codegen::VariableFlow {
        local_vars: HashMap::new(),
        entity_vars: HashMap::new(),
        inoc_vars: HashMap::new(),
    };
    let vf_result = ast::walk(&mut vf_visitor, root);

    let eft_result = ast::walk(&mut eft_visitor, root);

    let mut cg_visitor = codegen::GenCode {
        header: Vec::new(),
        code: Vec::new(),
        symbol_table: HashMap::new(),
        entities_and_functions: eft_visitor.entities_and_functions,
        entity_function_locations: BTreeMap::new(),
        entity_data_values: HashMap::new(),
        entity_inoculation_values: HashMap::new(),
        entity_function_types: eft_visitor.entity_function_types,
        current_entity_id: 0,
        current_func_id: 0,
        absolute_entity_function_locations: BTreeMap::new(),
        function_num: HashMap::new(),
        relocations: Vec::new(),
        entity_table: ent_ids_visitor.entity_ids.clone(),
    };

    // Apply passes
    //let gen_con_result = ast::walk(&mut gen_con_visitor, root);
    let cg_result = ast::walk(&mut cg_visitor, root);

    println!("Ent ids {:#?}", ent_ids_visitor.entity_ids);

    // TODO: these should be run inside GenCode automatically - add "pre/post" methods to all AstNodeVisitors
    cg_visitor.build_entity_data_table();
    cg_visitor.build_entity_inoculation_table();
    cg_visitor.build_entity_function_location_table();
    cg_visitor.relocate_functions();

    // We gather all code, relocations, data, and inoculation tables, combine them, rename/number symbols, and relocate all functions

    let mut complete_output: Vec<u8> = cg_visitor.header.clone();
    complete_output.append(&mut cg_visitor.code);

    pbin::disassemble(&complete_output);

    fs::write(outpath, &complete_output).unwrap();
}
