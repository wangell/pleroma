use crate::ast;
use crate::ast::Hvalue;
use crate::common::{vec, Box, HashMap, String, Vec};
use crate::vm_core;
use crate::common::Arc;
use core;

use crate::opcodes::{decode_instruction, Op};
use crate::pbin::{
    disassemble, load_entity_data_table, load_entity_function_table, load_entity_inoculation_table,
};
use crate::vm_core::{Msg, StackFrame, Vat};

const debug_msg_flow : bool = false;

pub fn run_expr(
    start_addr: usize,
    vat: &mut Vat,
    msg: &Msg,
    tx_msg: &mut crossbeam::channel::Sender<vm_core::Msg>,
    src_promise_id: Option<u32>,
) -> Option<Hvalue> {

    // TODO: code can just be referenced from the vat instead of cloning the ref
    let code = vat.code.clone();

    let mut x = start_addr;

    let mut yield_op = false;

    if !vat.entities.contains_key(&msg.dst_address.entity_id) {
        panic!("Attempted to run expr on entity {}, but entity doesn't exist in vat {}", msg.dst_address.entity_id, vat.vat_id);
    }

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
                // Load an entity variable from the current entity onto the op stack

                let current_entity_id = vat.call_stack.last().unwrap().entity_id;

                let mut target_entity = vat.entities.get_mut(&current_entity_id).unwrap();
                let local_var = target_entity.data[&s0].clone();
                vat.op_stack.push(local_var);
            }
            Op::Estore(s0) => {
                // Stores an entity variable to the current entity from the op stack

                let current_entity_id = vat.call_stack.last().unwrap().entity_id;

                let mut target_entity = vat.entities.get_mut(&current_entity_id).unwrap();
                let store_val = vat.op_stack.pop().unwrap();
                target_entity.data.insert(s0, store_val);
            }
            Op::ConstructEntity(a0, a1) => {
                // Pop off arguments from stack
                let mut args = Vec::new();
                for i in 0..a1 {
                    args.push(vat.op_stack.pop().unwrap());
                }

                let mut zz = 0;
                let data_table = load_entity_data_table(&mut zz, &code[..]);

                let mut qq = 0;
                let inoc_table = load_entity_inoculation_table(&mut qq, &code[zz..]);

                let mut xx = 0;
                let table = load_entity_function_table(&mut xx, &code[qq + zz..]);

                let ent = vat.create_entity_code(a0, 0, &data_table[&a0]);

                let mut nf = StackFrame {
                    entity_id: ent.address.entity_id,
                    locals: HashMap::new(),
                    return_address: Some(x),
                };

                // Push the new call stack and jump to next location
                vat.call_stack.push(nf);
                x = table[&a0][&0].0;
            },
            Op::Call(a0, a1) => {
                // Get the target entity from stack
                let target_entity = vat.op_stack.pop().unwrap();
                // Pop off arguments from stack
                let mut args = Vec::new();
                for i in 0..a1 {
                    args.push(vat.op_stack.pop().unwrap());
                }

                let mut nf = StackFrame {
                    entity_id: 0,
                    locals: HashMap::new(),
                    return_address: Some(x),
                };


                if let Hvalue::EntityAddress(add) = target_entity {
                    nf.entity_id = add.entity_id;
                } else {
                    panic!("Invalid entity address found on stack while doing call");
                }

                vat.op_stack.extend(args);

                // Push the new call stack and jump to next location
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

                let out_msg = vm_core::Msg {
                    src_address: msg.dst_address,
                    dst_address: target_address.unwrap(),

                    contents: vm_core::MsgContents::Request {
                        args: args,
                        // TODO: u64
                        function_id: a0 as u32,
                        src_promise: Some(next_prom_id.into()),
                        no_response: false
                    },
                };

                let ent_addr = msg.src_address.clone();
                tx_msg.send(out_msg).unwrap();
                vat.promise_stack.insert(
                    next_prom_id,
                    vm_core::Promise::new(src_promise_id, Some(ent_addr)),
                );
                vat.op_stack.push(Hvalue::Promise(next_prom_id));
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
            Op::ForeignCall(a0, a1) => {
                // TODO: if kernel is recompiled, this won't point to the right memory address - need to create a table for sys modules
                let ptr = a0 as *const ();

                let mut args = Vec::new();
                for i in 0..a1 {
                    let local_var = vat.load_local(&("p".to_owned() + &i.to_string()));
                    args.push(ast::AstNode::ValueNode(local_var));
                }

                let mut target_entity = vat.entities.get_mut(&msg.dst_address.entity_id).unwrap();
                let foreign_func: ast::RawFF = unsafe { core::mem::transmute(ptr) };
                let res = foreign_func(vat, msg.dst_address.entity_id, Hvalue::List(args));

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
    vat: &mut Vat,
    msg: &Msg,
    tx_msg: &mut crossbeam::channel::Sender<vm_core::Msg>,
) -> Option<Msg> {
    let mut out_msg: Option<Msg> = None;

    if debug_msg_flow {
        println!("Message {:?}", msg);
        println!("");
    }

    let code = &*vat.code;

    match &msg.contents {
        vm_core::MsgContents::Request {
            args,
            function_id,
            src_promise,
            no_response
        } => {


            let mut z = 0;
            let data_table = load_entity_data_table(&mut z, &code[..]);

            let mut q = 0;
            let inoc_table = load_entity_inoculation_table(&mut q, &code[z..]);

            let mut x = 0;
            let table = load_entity_function_table(&mut x, &code[q + z..]);

            // FIXME: Why is entity ID zero and does that matter?
            vat.call_stack.push(StackFrame {
                entity_id: 0,
                locals: HashMap::new(),
                return_address: None,
                // remove this
            });

            for i in args {
                vat.op_stack.push(i.clone());
            }

            let entity_type_id = vat.entities.get(&msg.dst_address.entity_id).unwrap().entity_type;

            z = table[&entity_type_id][&function_id].0 as usize;

            let poss_res = run_expr(z, vat, msg, tx_msg, src_promise.clone());

            if !no_response {
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
        }
        vm_core::MsgContents::Response {
            result,
            dst_promise,
        } => {
            // FIXME: there is a logic error here when doing promise chaining
            if let Some(promise_id) = dst_promise {
                // We want to execute from top of stack down
                let fix_id = *promise_id as u8;
                //println!("{:?}", vat);
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

                for i in resolutions {
                    vat.op_stack = promise.save_point.0.clone();
                    vat.call_stack = promise.save_point.1.clone();

                    vat.store_local(&promise.var_names[0], result);
                    let poss_res = run_expr(i, vat, msg, tx_msg, promise.src_promise);

                    // If this message has a promise that it needs to respond to, do it
                    if let (Some(res), Some(src_prom_real)) =
                        (poss_res.clone(), promise.src_promise)
                    {
                        //println!("{:#?}", vat);
                        println!(
                            "Sending back res: {:?} to {:?}",
                            res,
                            promise.src_entity.unwrap()
                        );
                        out_msg = Some(Msg {
                            src_address: msg.dst_address,
                            dst_address: promise.src_entity.unwrap(),
                            contents: vm_core::MsgContents::Response {
                                result: res,
                                dst_promise: promise.src_promise,
                            },
                        });
                        println!("Final msg: {:#?}", out_msg);
                    }
                }
            }
        }
    }

    out_msg
}
