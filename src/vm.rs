use crate::ast;
use crate::ast::Hvalue;
use crate::common::{vec, Box, HashMap, String, Vec};
use crate::vm_core;
use core;

use crate::pbin::{
    disassemble, load_entity_function_table
};
use crate::opcodes::{decode_instruction, Op};
use crate::vm_core::{Msg, StackFrame, Vat};

pub fn run_expr(
    start_addr: usize,
    vat: &mut Vat,
    msg: &Msg,
    tx_msg: &mut crossbeam::channel::Sender<vm_core::Msg>,
    code: &Vec<u8>,
) -> Option<Hvalue> {
    let mut target_entity = vat.entities.get_mut(&msg.dst_address.entity_id).unwrap();

    let mut x = start_addr;

    let mut yield_op = false;

    loop {
        // TODO: this should never happen, need implicit return
        if x >= code.len() {
            //panic!("Code overrun: x was {}, code is len {}", x, code.len());
            break;
        }

        let (q, inst) = decode_instruction(x, &code[..]);

        x = q;

        match inst {
            Op::Push(a0) => {
                vat.op_stack.push(a0);
            }
            Op::Add => {
                if let (Hvalue::Hu8(a0), Hvalue::Hu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 + a1;
                    println!("Calling add {} + {} : {}", a0, a1, res);
                    vat.op_stack.push(Hvalue::Hu8(res));
                } else {
                    panic!();
                }
            }
            Op::Sub => {
                if let (Hvalue::Hu8(a0), Hvalue::Hu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 - a1;
                    vat.op_stack.push(Hvalue::Hu8(res));
                } else {
                    panic!();
                }
            }
            Op::Mul => {
                if let (Hvalue::Hu8(a0), Hvalue::Hu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 * a1;
                    vat.op_stack.push(Hvalue::Hu8(res));
                } else {
                    panic!();
                }
            }
            Op::Div => {
                if let (Hvalue::Hu8(a0), Hvalue::Hu8(a1)) =
                    (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap())
                {
                    let res = a0 / a1;
                    vat.op_stack.push(Hvalue::Hu8(res));
                } else {
                    panic!();
                }
            }
            Op::Message => {
                let msg = vm_core::Msg {
                    src_address: msg.dst_address,
                    dst_address: vm_core::EntityAddress::new(0, 0, 0),

                    contents: vm_core::MsgContents::Request {
                        args: Vec::new(),
                        function_id: 0,
                        function_name: String::from("main"),
                        src_promise: Some(0),
                    },
                };

                tx_msg.send(msg).unwrap();
                vat.promise_stack.push(vm_core::Promise::new());
                //vat.op_stack.push(Hvalue::Promise(0));
            }
            Op::Ret => {
                //let ret_val = vat.op_stack.pop().unwrap();
                let sf = vat.call_stack.pop().unwrap();

                if let Some(ret_addr) = sf.return_address {
                    x = ret_addr as usize;
                } else {
                    break;
                }
            }
            Op::ForeignCall(a0) => {
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
            //::Print(a0) => {
            //    println!("print {}", a0);
            //}
            //Inst::Resolve => {
            //    println!("Resolve {}!", vat.promise_stack.len());

            //    // If promise is resolved, run code, else push onto on_resolve
            //    if vat.promise_stack[0].resolved {
            //        println!("Already resolved!");
            //    } else {
            //        // Insert into top of stack + block vat
            //        println!("Inserting resolution callback");
            //        vat.promise_stack[0].on_resolve.push(x);
            //        yield_op = true;
            //        break;
            //    }
            //}
            Op::Nop => {}
        }
    }

    if vat.op_stack.len() > 1 {
        println!("{:?}", vat.op_stack);
        panic!(
            "Invalid program, left with {} operands on stack",
            vat.op_stack.len()
        );
    }

    if yield_op {
        return None
    }

    if vat.op_stack.is_empty() {
        Some(Hvalue::None)
    } else {
        Some(vat.op_stack.pop().unwrap())
    }
}

pub fn run_msg(
    code: &Vec<u8>,
    vat: &mut Vat,
    msg: &Msg,
    tx_msg: &mut crossbeam::channel::Sender<vm_core::Msg>,
) -> Option<Msg> {
    println!("Running message: {:?}", msg);

    let mut out_msg: Option<Msg> = None;

    match &msg.contents {
        vm_core::MsgContents::Request {
            args,
            function_id,
            function_name,
            src_promise,
        } => {
            let mut z = 0;

            let table = load_entity_function_table(&mut z, &code[..]);

            vat.call_stack.push(StackFrame {
                locals: HashMap::new(),
                return_address: None,
            });

            z = table[&0][&function_id].0 as usize;

            let res = run_expr(z, vat, msg, tx_msg, code);

            out_msg = Some(Msg {
                src_address: msg.dst_address,
                dst_address: msg.src_address,
                contents: vm_core::MsgContents::Response {
                    result: res.unwrap(),
                    dst_promise: *src_promise,
                },
            });
        }
        vm_core::MsgContents::Response {
            result,
            dst_promise,
        } => {
            if let (promise_id) = dst_promise {
                // We want to execute from top of stack down
                println!("Got response, fulfilling promise");
                {
                    let  promise = &mut vat.promise_stack[dst_promise.unwrap() as usize];
                    promise.resolved = true;
                }

                let resolutions = vat.promise_stack[dst_promise.unwrap() as usize].on_resolve.clone();

                for i in resolutions {
                    run_expr(i, vat, msg, tx_msg, code);
                }
            }
        }
        vm_core::MsgContents::BigBang {
            args,
            function_id,
            function_name,
        } => {
            let mut z = 0;

            let table = load_entity_function_table(&mut z, &code[..]);

            vat.call_stack.push(StackFrame {
                locals: HashMap::new(),
                return_address: None,
            });

            z = table[&0][&function_id].0 as usize;

            run_expr(z, vat, msg, tx_msg, code);
        }
    }

    out_msg
}
