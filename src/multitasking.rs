extern crate alloc;

use alloc::boxed::Box;
use alloc::vec;
use alloc::vec::Vec;
use core::arch::asm;
use spin::Mutex;

use lazy_static::lazy_static;

pub struct Scheduler {
    pub current_pid: u64,
    pub process_queue: Vec<ProcessControlBlock>,
    pub last_pid: u64,
    pub first_time: bool,
}

impl Scheduler {
    pub fn new() -> Self {
        Scheduler {
            current_pid: 0,
            last_pid: 0,
            process_queue: Vec::new(),
            first_time: true,
        }
    }

    pub fn new_process(&mut self, func_ptr: usize) {
        self.process_queue
            .push(ProcessControlBlock::new(self.last_pid + 1, func_ptr));
        self.last_pid += 1;
    }
}

lazy_static! {
    pub static ref SCHEDULER: Mutex<Scheduler> = Mutex::new(Scheduler::new());
}

#[derive(Debug)]
pub struct ProcessControlBlock {
    pub pid: u64,
    pub stack: Vec<u8>,

    pub ip: usize,
    pub rsp: usize,
    pub cpu_flags: u64,
    pub code_segment: u64,
    pub stack_segment: u64,
}

impl ProcessControlBlock {
    pub fn new(pid: u64, ip: usize) -> Self {
        const stack_size: usize = 1000000;

        let mut pcb = ProcessControlBlock {
            pid,
            ip,
            code_segment: 0,
            stack_segment: 0,
            // 0x0200 = interrupts enabled
            cpu_flags: 0x0200,
            stack: allocate_stack(stack_size),
            rsp: 0,
        };

        unsafe {
            pcb.rsp = pcb.stack.as_ptr().offset(1000000) as usize;
        }

        pcb
    }
}

pub fn allocate_stack(size: usize) -> Vec<u8> {
    let stack = vec![0; size];
    stack
}
