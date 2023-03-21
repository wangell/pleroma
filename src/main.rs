#![cfg_attr(feature = "native", no_std)]
#![cfg_attr(feature = "native", no_main)]
#![cfg_attr(feature = "native", feature(abi_x86_interrupt))]

#[macro_use]
extern crate lalrpop_util;
use std::fs;

#[macro_use]
#[cfg(feature = "native")]
pub mod io;
#[cfg(feature = "native")]
pub mod interrupts;
#[cfg(feature = "native")]
pub mod native;

pub mod ast;
pub mod codegen;
pub mod kernel;
pub mod opcodes;
pub mod parser;
pub mod pbin;
pub mod pnet;
pub mod sem;
pub mod vm;
pub mod vm_core;
pub mod compile;

fn run_program() {
    //pnet::setup_network();

    let mut node = kernel::Node::new();
    node.code.insert(0, fs::read("kernel.plmb").unwrap());

    let nodeman = kernel::Nodeman::new(&mut node);
    kernel::load_nodeman(&nodeman);

    //let mut monad = kernel::Monad::new();
    //kernel::load_kernel(&mut monad);

    let mut vat = vm_core::Vat::new();

    vat.create_entity(&nodeman.def);
    nodeman.node.vats.insert(0, vat);
    nodeman.node.vats.get_mut(&0).unwrap().inbox.push(vm_core::Msg {
        src_address: vm_core::EntityAddress::new(0, 0, 0),
        dst_address: vm_core::EntityAddress::new(0, 0, 0),
        promise_id: None,
        is_response: false,
        function_name: String::from("main"),
        values: Vec::new(),
        function_id: 0,
    });

    let mut vat_idx = 0;

    loop {
        while nodeman.node.vats[&vat_idx].inbox.len() > 0 {
            let msg = nodeman.node.vats.get_mut(&vat_idx).unwrap().inbox.pop().unwrap();
            let mut current_vat = nodeman.node.vats.get_mut(&vat_idx).unwrap();
            let code = &nodeman.node.code[&current_vat.entities[&msg.dst_address.entity_id].code_id];
                vm::run_msg(
                    &code,
                    current_vat,
                &msg,
            );
        }

        vat_idx = (vat_idx + 1) % nodeman.node.vats.len() as u64;
    }
}

fn main() {
    run_program();
}
