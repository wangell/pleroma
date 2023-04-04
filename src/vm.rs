use crate::ast;
use crate::ast::Hvalue;
use crate::common::{vec, Box, HashMap, String, Vec};
use core;
use crate::vm_core;

use crate::pbin::{
    decode_instruction, disassemble, load_entity_function_table, read_u16, read_u32, read_u64,
    read_u8, Inst,
};
use crate::vm_core::{Msg, StackFrame, Vat};

pub fn run_msg(code: &Vec<u8>, vat: &mut Vat, msg: &Msg, tx_msg: &mut crossbeam::channel::Sender<vm_core::Msg>) {
    disassemble(code);

    let mut x = 0;

    let mut target_entity = vat.entities.get_mut(&msg.dst_address.entity_id).unwrap();

    let table = load_entity_function_table(&mut x, &code[..]);

    vat.call_stack.push(StackFrame {
        locals: HashMap::new(),
        return_address: None,
    });

    x = table[&0][&msg.function_id].0 as usize;

    loop {
        if x >= code.len() {
            break;
        }

        let (q, inst) = decode_instruction(x, &code[..]);
        x = q;

        match inst {
            Inst::Push(a0) => {
                vat.op_stack.push(Hvalue::Pu8(a0.into()));
            }
            Inst::Addi => {
                if let (Hvalue::Pu8(a0), Hvalue::Pu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 + a1;
                    println!("Calling add {} + {} : {}", a0, a1, res);
                    vat.op_stack.push(Hvalue::Pu8(res));
                } else {
                    panic!();
                }
            }
            Inst::Subi => {
                if let (Hvalue::Pu8(a0), Hvalue::Pu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 - a1;
                    vat.op_stack.push(Hvalue::Pu8(res));
                } else {
                    panic!();
                }
            }
            Inst::Muli => {
                if let (Hvalue::Pu8(a0), Hvalue::Pu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 * a1;
                    vat.op_stack.push(Hvalue::Pu8(res));
                } else {
                    panic!();
                }
            }
            Inst::Divi => {
                if let (Hvalue::Pu8(a0), Hvalue::Pu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 / a1;
                    vat.op_stack.push(Hvalue::Pu8(res));
                } else {
                    panic!();
                }
            }
            Inst::Message => {
                let msg = vm_core::Msg {
                    src_address: vm_core::EntityAddress::new(0, 0, 0),
                    dst_address: vm_core::EntityAddress::new(0, 0, 0),
                    promise_id: None,
                    is_response: false,
                    function_name: String::from("main"),
                    values: Vec::new(),
                    function_id: 0,
                };
                //tx_ml_box.send(msg).unwrap();
                //vat.outbox.push(msg);
                tx_msg.send(msg).unwrap();
                vat.op_stack.push(Hvalue::Promise);
                println!("added a message");
            }
            Inst::Ret => {
                //let ret_val = vat.op_stack.pop().unwrap();
                let sf = vat.call_stack.pop().unwrap();

                if let Some(ret_addr) = sf.return_address {
                    x = ret_addr as usize;
                } else {
                    break;
                }
            }
            Inst::Call(a0) => {
                println!("Calling pos {} from {}", a0, x);
                vat.call_stack.push(StackFrame {
                    locals: HashMap::new(),
                    return_address: Some(x),
                });
                x = a0 as usize;
            }
            Inst::ForeignCall(a0) => {
                // TODO: if kernel is recompiled, this won't point to the right memory address - need to create a table for sys modules
                let ptr = a0 as *const ();
                // TODO: create a type FFI for this
                let foreign_func: vm_core::SystemFunction = unsafe { core::mem::transmute(ptr) };
                let res = foreign_func(target_entity, Hvalue::None);

                println!("Data {:?}", target_entity.data);
                //let res = target_entity.def.foreign_functions[&a0](ast::Hvalue::None);
                vat.op_stack.push(res);
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

    if vat.op_stack.len() != 1 {
        println!("{:?}", vat.op_stack);
        panic!("Invalid program, left with {} operands on stack", vat.op_stack.len());
    }

    let res = vat.op_stack.pop();
    println!("Result: {:?}", res.unwrap());
}
