use crate::ast;
use crate::ast::Hvalue;
use crate::common::{vec, Box, HashMap, String, Vec};
use crate::vm_core;
use core;

use crate::opcodes::{decode_instruction, Op};
use crate::pbin::{disassemble, load_entity_data_table, load_entity_function_table};
use crate::vm_core::{Msg, StackFrame, Vat};

pub fn run_expr(
    start_addr: usize,
    vat: &mut Vat,
    msg: &Msg,
    tx_msg: &mut crossbeam::channel::Sender<vm_core::Msg>,
    code: &Vec<u8>,
    src_promise_id: Option<u32>,
) -> Option<Hvalue> {
    let mut x = start_addr;

    let mut yield_op = false;

    loop {
        // TODO: this should never happen, need implicit return
        if x >= code.len() {
            //panic!("Code overrun: x was {}, code is len {}", x, code.len());
            break;
        }

        let last_ip = x;

        let (q, inst) = decode_instruction(x, &code[..]);

        x = q;

        match inst {
            Op::Push(a0) => {
                vat.op_stack.push(a0);
            }
            Op::Add => {
                let (a0, a1) = (vat.op_stack.pop().unwrap(), vat.op_stack.pop().unwrap());
                if let (Hvalue::Hu8(b0), Hvalue::Hu8(b1)) = (a0.clone(), a1.clone()) {
                    let res = b0 + b1;
                    println!("Calling add {} + {} : {}", b0, b1, res);
                    vat.op_stack.push(Hvalue::Hu8(res));
                } else {
                    println!("Instruction: {:?}", inst);
                    println!("a0: {:?}, a1: {:?}", a0, a1);
                    println!("Vat state: {:?}", vat);
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
            Op::Lload(s0) => {
                let local_var = vat.load_local(&s0);
                vat.op_stack.push(local_var);
            }
            Op::Lstore(s0) => {
                let store_val = vat.op_stack.pop().unwrap();
                if let Hvalue::Promise(promise_id) = store_val {
                    vat.promise_stack
                        .get_mut(&promise_id)
                        .unwrap()
                        .var_names
                        .push(s0.clone());
                }
                vat.store_local(&s0, &store_val);
            }
            Op::Eload(s0) => {
                println!("s0 : {}", s0);
                let mut target_entity = vat.entities.get_mut(&msg.dst_address.entity_id).unwrap();
                let local_var = target_entity.data[&s0].clone();
                vat.op_stack.push(local_var);
            }
            Op::Estore(s0) => {
                let mut target_entity = vat.entities.get_mut(&msg.dst_address.entity_id).unwrap();
                let store_val = vat.op_stack.pop().unwrap();
                target_entity.data.insert(s0, store_val);
            }
            Op::Call(a0, a1) => {
                // #a1 rguments should already be on the stack
                let current_promise_id = vat.call_stack[vat.call_stack.len() - 1].promise_id;
                let mut nf = StackFrame {
                    locals: HashMap::new(),
                    return_address: Some(x),
                    promise_id: current_promise_id,
                };

                // TODO: Add back once we remove the argument/op stack method
                for i in 0..a1 {
                    //let tv = vat.op_stack.pop();
                    //nf.locals.insert(String::from("f"), tv.unwrap());
                }

                // Need to load / store current entity in StackFrame

                vat.call_stack.push(nf);
                x = a0 as usize;
            }
            Op::Message(a0, a1) => {
                let mut args = Vec::new();
                for i in 0..a1 {
                    args.push(vat.op_stack.pop().unwrap());
                }

                let next_prom_id = vat.promise_stack.len() as u8;
                let target_entity = vat.op_stack.pop().unwrap();

                let mut target_address: Option<vm_core::EntityAddress> = None;
                if let Hvalue::EntityAddress(add) = target_entity {
                    target_address = Some(add);
                }

                let msg = vm_core::Msg {
                    src_address: msg.dst_address,
                    dst_address: target_address.unwrap(),

                    contents: vm_core::MsgContents::Request {
                        args: args,
                        // TODO: u64
                        function_id: a0 as u32,
                        function_name: String::from("main"),
                        src_promise: Some(next_prom_id.into())
                    },
                };

                tx_msg.send(msg).unwrap();
                vat.promise_stack.insert(next_prom_id, vm_core::Promise::new(src_promise_id));
                vat.op_stack.push(Hvalue::Promise(next_prom_id));
                println!("Created promise {} from source promise ID {:?}", next_prom_id, src_promise_id);
            }
            Op::Await => {
                // If promise is resolved, run code, else push onto on_resolve
                let op = vat.op_stack.pop().unwrap();
                if let Hvalue::Promise(target_promise) = op {
                    if vat.promise_stack[&target_promise].resolved {
                        println!("Already resolved!");
                    } else {
                        vat.promise_stack
                            .get_mut(&target_promise)
                            .unwrap()
                            .save_point = (vat.op_stack.clone(), vat.call_stack.clone());
                        vat.promise_stack
                            .get_mut(&target_promise)
                            .unwrap()
                            .on_resolve
                            .push(x);
                    }
                }
                yield_op = true;
                break;
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
                let mut target_entity = vat.entities.get_mut(&msg.dst_address.entity_id).unwrap();
                let foreign_func: vm_core::SystemFunction = unsafe { core::mem::transmute(ptr) };
                let res = foreign_func(target_entity, Hvalue::None);

                println!("Data {:?}", target_entity.data);
                //let res = target_entity.def.foreign_functions[&a0](ast::Hvalue::None);
                vat.op_stack.push(res);
            }
            Op::Nop => {}
            _ => panic!(),
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
        return None;
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
    let mut out_msg: Option<Msg> = None;
    println!("Message {:?}", msg);
    println!("");

    match &msg.contents {
        vm_core::MsgContents::Request {
            args,
            function_id,
            function_name,
            src_promise,
        } => {
            let mut z = 0;
            let data_table = load_entity_data_table(&mut z, &code[..]);
            let q = z.clone();
            z = 0;
            let table = load_entity_function_table(&mut z, &code[q..]);

            println!("src prom : {:?}", src_promise);

            vat.call_stack.push(StackFrame {
                locals: HashMap::new(),
                return_address: None,
                // remove this
                promise_id: *src_promise
            });

            for i in args {
                vat.op_stack.push(i.clone());
            }

            z = table[&0][&function_id].0 as usize;

            let poss_res = run_expr(z, vat, msg, tx_msg, code, src_promise.clone());

            if let Some(res) = poss_res {
                out_msg = Some(Msg {
                    src_address: msg.dst_address,
                    dst_address: msg.src_address,
                    contents: vm_core::MsgContents::Response {
                        result: res,
                        dst_promise: *src_promise,
                    },
                });
            }
        }
        vm_core::MsgContents::Response {
            result,
            dst_promise,
        } => {
            // FIXME: there is a logic error here when doing promise chaining
            if let Some(promise_id) = dst_promise {
                // We want to execute from top of stack down
                let fix_id = *promise_id as u8;
                let prom = &mut vat.promise_stack.get_mut(&fix_id).unwrap();
                let mut promise;
                {
                    promise = prom.clone();
                    //prom.on_resolve.clear();
                }

                {
                    promise.resolved = true;
                }

                let resolutions = promise.on_resolve.clone();
                promise.on_resolve.clear();

                println!("{:?}", resolutions);
                for i in resolutions {
                    vat.op_stack = promise.save_point.0.clone();
                    vat.call_stack = promise.save_point.1.clone();

                    vat.store_local(&promise.var_names[0], result);
                    println!("Got value, setting {} to {:?}", promise.var_names[0], result);
                    //let poss_res = run_expr(i, vat, msg, tx_msg, code, *dst_promise);
                    let poss_res = run_expr(i, vat, msg, tx_msg, code, promise.src_promise);
                    if let (Some(res), Some(src_prom_real)) = (poss_res.clone(), promise.src_promise) {
                        println!("sending to {:?} Response expr: {:?}", promise.src_promise, poss_res.clone());
                        out_msg = Some(Msg {
                            src_address: msg.dst_address,
                            dst_address: msg.src_address,
                            contents: vm_core::MsgContents::Response {
                                result: res,
                                dst_promise: promise.src_promise
                            },
                        });
                    }
                }

            }
        }
        vm_core::MsgContents::BigBang {
            args,
            function_id,
            function_name,
        } => {
            let mut z = 0;
            let data_table = load_entity_data_table(&mut z, &code[..]);
            let q = z.clone();
            z = 0;
            let table = load_entity_function_table(&mut z, &code[q..]);

            vat.call_stack.push(StackFrame {
                locals: HashMap::new(),
                return_address: None,
                promise_id: None,
            });

            z = table[&0][&function_id].0 as usize;

            run_expr(z, vat, msg, tx_msg, code, None);
        }
    }

    out_msg
}
