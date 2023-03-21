#[repr(u8)]
pub enum Op {
    Lb8,

    Ret,

    Add,
    Sub,
    Mul,
    Div,

    RetExit,

    ForeignCall,
    SysCall
}

impl From<u8> for Op {
    fn from(value: u8) -> Self {
        match value {
            0x00 => Op::Lb8,
            0x01 => Op::Ret,

            0x02 => Op::Add,
            0x03 => Op::Sub,
            0x04 => Op::Mul,
            0x05 => Op::Div,

            0x06 => Op::RetExit,

            0x07 => Op::ForeignCall,
            0x08 => Op::SysCall,
            _ => panic!("Invalid value for Op {}", value),
        }
    }
}
