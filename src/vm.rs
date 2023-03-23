//use chrono;
cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        use std::collections::HashMap as HashMap;
        use crate::ast::{Value, Module, EntityDef};
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "native")] {
        extern crate alloc;

        use alloc::collections::BTreeMap as HashMap;
        use alloc::string::String;
        use alloc::vec::Vec;
        use alloc::boxed::Box;
        use crate::ast::{Value, Module, EntityDef};
    }
}

use crate::vm_core::{Vat, Msg};
use crate::pbin::{load_entity_function_table, decode_instruction, read_u8, read_u16, read_u32, read_u64, Inst, disassemble};

pub fn run_msg(code: &Vec<u8>, vat: &mut Vat, msg: &Msg) {

    println!("{:?}", code);
    disassemble(&code);

    let mut x = 7;

    //let mut target_entity = &vat.entities[&msg.dst_address.entity_id];

    //let table = load_entity_function_table(&mut x, &code[..]);

    //x = table[&0][&msg.function_id].0 as usize;

    loop {
        if x >= code.len() {
            break;
        }

        let (q, inst) = decode_instruction(x, &code[..]);
        x = q;

        match inst {
            Inst::Push(a0) => {
                vat.op_stack.push(a0.into());
            }
            Inst::Addi => {
                let a1 = vat.op_stack.pop().unwrap();
                let a0 = vat.op_stack.pop().unwrap();
                let res = a0 + a1;
                println!("Calling add {} + {} : {}", a0, a1, res);
                vat.op_stack.push(res);
            }
            Inst::Subi => {
                let a1 = vat.op_stack.pop().unwrap();
                let a0 = vat.op_stack.pop().unwrap();
                let res = a0 - a1;
                vat.op_stack.push(res);
            }
            Inst::Muli => {
                let a1 = vat.op_stack.pop().unwrap();
                let a0 = vat.op_stack.pop().unwrap();
                let res = a0 * a1;
                vat.op_stack.push(res);
            }
            Inst::Divi => {
                let a1 = vat.op_stack.pop().unwrap();
                let a0 = vat.op_stack.pop().unwrap();
                let res = a0 / a1;
                vat.op_stack.push(res);
            }
            Inst::Ret => {
                let ret_val = vat.op_stack.pop().unwrap();
                let ret_addr = vat.op_stack.pop().unwrap();

                // FIXME make sure this returns the object
                if vat.op_stack.len() == 0 {
                    vat.op_stack.push(ret_val);
                    break;
                } else {
                    x = ret_addr as usize;
                    vat.op_stack.push(ret_val);
                }
            }
            Inst::Call(a0) => {
                println!("Calling pos {} from {}", a0, x);
                vat.op_stack.push(x as i64);
                x = a0 as usize;
            }
            Inst::ForeignCall(a0) => {
                //println!("{:?}", target_entity.def);
                //let res = target_entity.def.foreign_functions[&a0]();
                //vat.op_stack.push(res);
            }
            //Inst::CallFunc(n, args) => {
            //    for arg_n in 0..usize::from(n) {
            //        match &args[arg_n] {
            //            Pval::Pstr(z) => {println!("Called func {}", z)}
            //            Pval::Pu8(z) => {println!("Called func with u8 var {}", z)}
            //            _ => {}
            //        }
            //    }
            //}
            Inst::Nop => {}
        }
    }

    if vat.op_stack.len() > 1 {
        println!("{:?}", vat.op_stack);
        panic!("Invalid program, left with more operands on stack");
    }

    let res = vat.op_stack.pop();
    println!("Result: {}", res.unwrap());
}
