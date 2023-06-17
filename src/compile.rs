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

//pub struct ModuleResolution {
//}
//
//impl AstNodeVisitor for ModuleResolution {
//    fn visit_root(&mut self, root: &mut ast::Root) {
//        for (module_name, module) in root.modules.iter_mut() {
//            walk(self, module);
//        }
//    }
//
//    fn visit_module(&mut self, module: &mut ast::Module) {
//        for (module) in module.imports.iter_mut() {
//            if let AstNode::ImportModule(s) = module {
//                let mod_str = fs::read_to_string("blah/basic_entity.plm").unwrap();
//                *module = parser::parse_module(&mod_str);
//            }
//        }
//    }
//}

pub fn create_dependency_tree(asts: &HashMap<String, ast::AstNode>) -> HashMap<String, Vec<String>> {
    let mut module_deps: HashMap<String, Vec<String>> = HashMap::new();

    for module in asts {
        module_deps.insert(String::from("test"), vec![String::from("test1"), String::from("test2")]);
    }

    return module_deps;
}

pub fn compile_from_files(files: Vec<String>, outpath: &str) {
    let mut asts: HashMap<String, ast::AstNode> = HashMap::new();
    for file in files {
        let mod_str = fs::read_to_string(file.clone()).unwrap();
        asts.insert(file.clone(), parser::parse_module(&mod_str));
    }

    compile_from_ast(&asts, outpath);
}

pub fn link_objects() {
}

pub fn compile_from_ast(asts: &HashMap<String, ast::AstNode>, outpath: &str) {

    let mut new_asts = asts.clone();

    let mut root = &mut new_asts.get_mut("./blah/basic_entity.plm").unwrap();

    let mut cg_visitor = codegen::GenCode {
        header: Vec::new(),
        code: Vec::new(),
        entity_function_locations: BTreeMap::new(),
        entity_data_values: HashMap::new(),
        entity_inoculation_values: HashMap::new(),
        current_entity_id: 0,
        current_func_id: 0,
        absolute_entity_function_locations: BTreeMap::new(),
        function_num: HashMap::new(),
        relocations: Vec::new(),
    };

    let mut vf_visitor = codegen::VariableFlow {
        local_vars: HashMap::new(),
        entity_vars: HashMap::new(),
        inoc_vars: HashMap::new(),
    };

    // Apply passes
    let vf_result = ast::walk(&mut vf_visitor, root);
    let cg_result = ast::walk(&mut cg_visitor, root);

    // TODO: these should be run inside GenCode automatically - add "pre/post" methods to all AstNodeVisitors
    cg_visitor.build_entity_data_table();
    cg_visitor.build_entity_inoculation_table();
    cg_visitor.build_entity_function_location_table();
    cg_visitor.relocate_functions();

    let mut complete_output: Vec<u8> = cg_visitor.header.clone();
    complete_output.append(&mut cg_visitor.code);

    pbin::disassemble(&complete_output);

    fs::write(outpath, &complete_output).unwrap();
}
