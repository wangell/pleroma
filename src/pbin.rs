use crate::common::{HashMap, String, Box, Vec, vec, str};
use crate::opcodes::{Op};

#[derive(Debug)]
pub enum Inst {
    Nop,
    Addi,
    Subi,
    Muli,
    Divi,
    Ret,
    Push(u8),

    ForeignCall(u8),
    Call(u64),
}

pub fn read_u8(vblock: &[u8]) -> (usize, u8) {
    let v = vblock[0];
    return (1, v);
}

pub fn read_u16(vblock: &[u8]) -> (usize, u16) {
    let v = ((vblock[0] as u16) << 8) | vblock[1] as u16;
    return (2, v);
}

pub fn read_u32(vblock: &[u8]) -> (usize, u32) {
    let v = ((vblock[0] as u32) << 24) | ((vblock[1] as u32) << 16) | ((vblock[2] as u32) << 8) | vblock[3] as u32;
    return (4, v);
}

pub fn read_u64(vblock: &[u8]) -> (usize, u64) {
    let v = ((vblock[0] as u64) << 56) | ((vblock[1] as u64) << 48) | ((vblock[2] as u64) << 40) | ((vblock[3] as u64) << 32) | ((vblock[4] as u64) << 24) | ((vblock[5] as u64) << 16) | ((vblock[6] as u64) << 8) | vblock[7] as u64;
    return (8, v);
}

pub fn read_s32(vblock : &[u8]) -> (usize, i32) {
    let (sv, v) = read_u32(vblock);
    return (sv, v as i32);
}

pub fn read_s64(vblock : &[u8]) -> (usize, i64) {
    let (sv, v) = read_u64(vblock);
    return (sv, v as i64);
}

pub fn read_float32(vblock : &[u8]) -> (usize, f32) {
    let (sv, v) = read_u32(vblock);
    return (sv, v as f32);
}

pub fn read_float64(vblock : &[u8]) -> (usize, f64) {
    let (sv, v) = read_u64(vblock);
    return (sv, v as f64);
}

pub fn read_utf8_str(vblock: &[u8]) -> (usize, String) {

    let mut i = 0;
    while vblock[i] != 0 {
        i += 1;
    }
    println!("{}", i);

    let s = match str::from_utf8(vblock) {
        Ok(v) => v,
        Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
    };

    return (i, String::from(s))
}

pub fn decode_instruction(x: usize, vblock: &[u8]) -> (usize, Inst) {
    let inst = vblock[x];
    let x = x + 1;
    match Op::from(inst) {
        Op::Add => {
            return (x, Inst::Addi);
        }
        Op::Sub => {
            return (x, Inst::Subi);
        }
        Op::Mul => {
            return (x, Inst::Muli);
        }
        Op::Div => {
            return (x, Inst::Divi);
        }
        Op::Ret => {
            return (x, Inst::Ret);
        }
        Op::ForeignCall => {
            let (y, a0) = read_u8(&vblock[x..]);

            return (x + y, Inst::ForeignCall(a0));
        }
        Op::Lb8 => {
            let (y, a0) = read_u8(&vblock[x..]);

            return (x + y, Inst::Push(a0));
        }
        _ => {
            println!("Nop");
            return (x, Inst::Nop);
        }
    }
}

pub fn load_entity_function_table(location: &mut usize, vblock: &[u8]) -> HashMap<u32, HashMap<u32, (usize, usize)>> {
    let table_size = read_u8(&vblock[*location..]);
    *location += 1;

    let mut table : HashMap<u32, HashMap<u32, (usize, usize)>> = HashMap::new();

    let mut z = 0;

    while (z < table_size.1) {
        let ent_id = read_u8(&vblock[*location..]);

        *location += 1;

        let func_id = read_u8(&vblock[*location..]);
        *location += 1;

        let loc = read_u16(&vblock[*location..]);
        *location += 2;

        let func_len = read_u16(&vblock[*location..]);
        *location += 2;

        table.entry(ent_id.1.into()).or_insert(HashMap::new());
        table.get_mut(&ent_id.1.into()).unwrap().insert(func_id.1.into(), (loc.1 as usize, func_len.1 as usize));

        z += 1;
    }

    table
}

pub fn disassemble(binary_code: &Vec<u8>) {
    println!("Disassembly:");
    let mut x = 0;
    let table = load_entity_function_table(&mut x, &binary_code[..]);

    for (eid, funcs) in &table {
        println!("EID: {}", eid);
        for (fid, (loc, sz)) in &table[&eid] {
            println!("\tFID ({}): {}", sz, *fid);
            x = *loc;
            let loc_end = *sz + *loc;
            while x < loc_end {
                let inst = decode_instruction(x, &binary_code[..]);
                println!("\t\t{:?}", inst.1);
                x = inst.0;
            }
            println!();
        }
    }
}
