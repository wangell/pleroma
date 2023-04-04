use crate::ast::{AstNode, Hvalue};
use crate::ast;
use crate::vm_core;

trait SystemModule {
   fn unite(def: &mut ast::EntityDef);
}

fn load_system_module(filepath: String, module: &impl SystemModule) {
}

pub fn register_function(func_name: String, func: fn(&mut vm_core::Entity, Hvalue)->Hvalue) {
}
