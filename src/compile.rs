use crate::ast::AstNode;
use crate::codegen;
use crate::parser;
use crate::opcodes;
use crate::ast;
use crate::pbin;
use crate::ast::AstNodeVisitor;

use std::collections::HashMap;
use std::fs;

pub fn compile_to_file(path_in: &String, path_out: &String) {
    let contents = fs::read_to_string(path_in).expect("Should have been able to read the file");
    let mut module = parser::parse(contents.as_str());

    //let mut cg_visitor = codegen::GenCode {
    //    header: Vec::new(),
    //    code: Vec::new(),
    //    entity_function_locations: HashMap::new(),
    //    current_entity_id: 0,
    //    current_func_id: 0,
    //};

    //module.entity_defs.insert(
    //    String::from("Kernel"),
    //    ast::AstNode::EntityDef(monad.def.clone()),
    //);

    //for (entity_name, entity_def) in &module.entity_defs {
    //    let result = entity_def.visit(&mut cg_visitor);

    //    cg_visitor.build_entity_function_location_table();

    //    let mut complete_output: Vec<u8> = cg_visitor.header.clone();
    //    complete_output.append(&mut cg_visitor.code);
    //    complete_output.push(opcodes::Op::RetExit as u8);
    //    fs::write(path_out, &complete_output).unwrap();
    //}
}

pub fn compile(module: &mut ast::Module) {
    let mut cg_visitor = codegen::GenCode {
        header: Vec::new(),
        code: Vec::new(),
        entity_function_locations: HashMap::new(),
        entity_data_values: HashMap::new(),
        current_entity_id: 0,
        current_func_id: 0,
        function_num: HashMap::new()
    };

    let mut vf_visitor = codegen::VariableFlow {
        local_vars: HashMap::new(),
        entity_vars: HashMap::new()
    };

    for (entity_name, entity_def) in module.entity_defs.iter_mut() {

        let vf_result = ast::walk(&mut vf_visitor, entity_def);
        let cg_result = ast::walk(&mut cg_visitor, entity_def);

        cg_visitor.build_entity_data_table();
        cg_visitor.build_entity_function_location_table();

        let mut complete_output: Vec<u8> = cg_visitor.header.clone();
        complete_output.append(&mut cg_visitor.code);
        //complete_output.push(opcodes::Op::RetExit as u8);

        pbin::disassemble(&complete_output);

        fs::write(format!("kernel.plmb"), &complete_output).unwrap();
    }

}
