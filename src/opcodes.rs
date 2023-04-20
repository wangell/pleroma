use crate::ast;
use crate::ast::Hvalue;
use crate::bin_helpers::{read_u64_sz, read_u32_sz, read_u16_sz, read_u8_sz, read_utf8_str_sz};
use crate::vm_core;

#[derive(Debug)]
pub enum Op {
    Nop,

    Add,
    Sub,
    Mul,
    Div,

    Ret,

    Push(ast::Hvalue),

    ForeignCall(u64),

    // Function #, # of args
    Message(u64, u8),
    Call(u64, u8),

    Lload(String),
    Lstore(String),

    Eload(String),
    Estore(String),

    Await
}

#[repr(u8)]
pub enum SimpleOp {
    Nop,

    Add,
    Sub,
    Mul,
    Div,

    Ret,

    Push,

    ForeignCall,
    Call,
    Message,

    Lload,
    Lstore,

    Eload,
    Estore,

    Await
}

pub fn decode_value(vblock: &[u8]) -> (usize, Hvalue) {

    let x = 0;

    match vblock[x] {
        0x01 => {
            let (y, a0) = read_u8_sz(&vblock[x+1..]);
            return (1 + y, Hvalue::Hu8(a0));
        }
        0x02 => {
            let (y, a0) = read_u8_sz(&vblock[x+1..]);
            return (1 + y, Hvalue::Promise(a0));
        }
        0x03 => {
            let (y0, node_id) = read_u32_sz(&vblock[x+1..]);
            let (y1, vat_id) = read_u32_sz(&vblock[x+1+y0..]);
            let (y2, entity_id) = read_u32_sz(&vblock[x+1+y0+y1..]);

            return (1 + y0 + y1 + y2, Hvalue::EntityAddress(vm_core::EntityAddress{node_id, vat_id, entity_id}));
        }
        _ => panic!()
    }
}

pub fn encode_value(val: &ast::Hvalue) -> Vec<u8> {
    let mut bytes: Vec<u8> = Vec::new();

    match val {
        Hvalue::Hu8(a0) => {
            bytes.push(0x01);
            bytes.push(*a0);
        },
        Hvalue::Promise(a0) => {
            bytes.push(0x02);
            bytes.push(*a0);
        },
        Hvalue::EntityAddress(address) => {
            bytes.push(0x03);
            bytes.extend_from_slice(&address.node_id.to_be_bytes());
            bytes.extend_from_slice(&address.vat_id.to_be_bytes());
            bytes.extend_from_slice(&address.entity_id.to_be_bytes());
        }
        _ => panic!()
    }

    bytes
}

pub fn encode_instruction(op: &Op) -> Vec<u8> {
    let mut bytes: Vec<u8> = vec![];

    match op {
        Op::Nop => {
            bytes.push(SimpleOp::Nop as u8);
        },

        Op::Add => {
            bytes.push(SimpleOp::Add as u8)
        },
        Op::Sub => {
            bytes.push(SimpleOp::Sub as u8)
        },
        Op::Mul => {
            bytes.push(SimpleOp::Mul as u8)
        },
        Op::Div => {
            bytes.push(SimpleOp::Div as u8)
        },

        Op::Ret => {
            bytes.push(SimpleOp::Ret as u8)
        },

        Op::Await => {
            bytes.push(SimpleOp::Await as u8)
        },

        Op::Push(a0) => {
            bytes.push(SimpleOp::Push as u8);
            bytes.append(&mut encode_value(&a0));
        },

        Op::ForeignCall(a0) => {},

        Op::Call(a0, a1) => {
            bytes.push(SimpleOp::Call as u8);
            bytes.extend_from_slice(&a0.to_be_bytes());
            bytes.extend_from_slice(&a1.to_be_bytes());
        },

        Op::Message(a0, a1) => {
            bytes.push(SimpleOp::Message as u8);
            bytes.extend_from_slice(&a0.to_be_bytes());
            bytes.extend_from_slice(&a1.to_be_bytes());
        },

        Op::Lload(s0) => {
            bytes.push(SimpleOp::Lload as u8);
            bytes.extend_from_slice(s0.as_bytes());
            bytes.push(0x0);
        },

        Op::Eload(s0) => {
            bytes.push(SimpleOp::Eload as u8);
            bytes.extend_from_slice(s0.as_bytes());
            bytes.push(0x0);
        },

        Op::Lstore(s0) => {
            bytes.push(SimpleOp::Lstore as u8);
            bytes.extend_from_slice(s0.as_bytes());
            bytes.push(0x0);
        }

        Op::Estore(s0) => {
            bytes.push(SimpleOp::Estore as u8);
            bytes.extend_from_slice(s0.as_bytes());
            bytes.push(0x0);
        }

        _ => panic!()
    }

    bytes
}

pub fn decode_instruction(x: usize, vblock: &[u8]) -> (usize, Op) {
    let inst_op: SimpleOp = unsafe { core::mem::transmute(vblock[x]) };
    let x = x + 1;

    match inst_op {
        SimpleOp::Add => {
            return (x, Op::Add);
        }
        SimpleOp::Sub => {
            return (x, Op::Sub);
        }
        SimpleOp::Mul => {
            return (x, Op::Mul);
        }
        SimpleOp::Div => {
            return (x, Op::Div);
        }
        SimpleOp::Ret => {
            return (x, Op::Ret);
        }

        SimpleOp::Message => {
            let (y, a0) = read_u64_sz(&vblock[x..]);
            let (y0, a0) = read_u64_sz(&vblock[x..]);
            let (y1, a1) = read_u8_sz(&vblock[x+y0..]);

            return (x + y0 + y1, Op::Message(a0, a1));
        }

        SimpleOp::Call => {
            let (y0, a0) = read_u64_sz(&vblock[x..]);
            let (y1, a1) = read_u8_sz(&vblock[x+y0..]);

            return (x + y0 + y1, Op::Call(a0, a1));
        }

        SimpleOp::Lload => {
            let (y, a0) = read_utf8_str_sz(&vblock[x..]);
            return (x + y, Op::Lload(a0));
        }
        SimpleOp::Eload => {
            let (y, a0) = read_utf8_str_sz(&vblock[x..]);
            return (x + y, Op::Eload(a0));
        }
        SimpleOp::Estore => {
            let (y, a0) = read_utf8_str_sz(&vblock[x..]);
            return (x + y, Op::Estore(a0));
        }
        SimpleOp::Lstore => {
            let (y, a0) = read_utf8_str_sz(&vblock[x..]);
            return (x + y, Op::Lstore(a0));
        }
        SimpleOp::ForeignCall => {
            let (y, a0) = read_u64_sz(&vblock[x..]);

            return (x + y, Op::ForeignCall(a0));
        }
        SimpleOp::Push => {
            let (y, a0) = decode_value(&vblock[x..]);

            return (x + y, Op::Push(a0));
        }
        SimpleOp::Await => {
            return (x, Op::Await);
        }
        _ => {
            return (x, Op::Nop);
        }
    }
}
