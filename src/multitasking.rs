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
    pub first_time : bool
}

impl Scheduler {
    pub fn new() -> Self {
        Scheduler {
            current_pid: 0,
            process_queue: Vec::new(),
            first_time: true
        }
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
    pub stack_segment: u64
}

impl ProcessControlBlock {
    pub fn new(pid: u64, ip: usize) -> Self {
        let mut pcb = ProcessControlBlock {
            pid: pid,
            ip: ip,
            code_segment: 0,
            stack_segment: 0,
            cpu_flags: 0,
            stack: allocate_stack(4096),
            rsp: 0
        };

        // Make sure last entry in the stack is the instruction pointer to the function
        //let ip_bytes = ip.to_le_bytes();
        //pcb.stack[4088..4096].copy_from_slice(&ip_bytes);

        unsafe {
            //pcb.rsp = pcb.stack.as_ptr().offset(4088) as usize;
            pcb.rsp = pcb.stack.as_ptr().offset(4096) as usize;
        }

        pcb
    }
}

pub fn allocate_stack(size: usize) -> Vec<u8> {
    let stack = vec![0; size];
    stack
}

//pub fn thing2() {
//    println!("loop2");
//    use crate::native_util;
//    native_util::hlt_loop();
//}

//pub fn test_stack(i: u32) {
//    println!("new stack works");
//}

//pub fn thing() {
//    let mut blah1: *const u8;
//    unsafe {
//        //blah1 = process.stack.as_ptr().offset(4096 - (8 * 14));
//        //blah1 = process.stack.as_ptr().offset(4096 - 9);
//        //blah1 = process.stack.as_ptr().offset(4088);
//        // Offset by IP + registers
//        blah1 = pcb.stack.as_ptr().offset(4096 - (8 * (1 + 15)));
//        //blah1 = pcb.stack.as_ptr().offset(4088);
//    }
//
//    unsafe {
//        asm!(
//            "push rax",
//            "push rbx",
//            "push rcx",
//            "push rdx",
//            "push rsi",
//            "push rdi",
//            "push rbp",
//
//            "push r8",
//            "push r9",
//            "push r10",
//            "push r11",
//            "push r12",
//            "push r14",
//            "push r15",
//        );
//        post_switch(blah1 as usize);
//    }
//}

//pub fn switch_to_task(process: &mut ProcessControlBlock) {
//    println!("Switching...");
//    let blah2: usize = thing as usize;
//
//    let byeter = blah2.to_be_bytes();
//    println!("{:?}", blah2);
//    println!("{:?}", byeter);
//    process.stack[4095] = byeter[0];
//    process.stack[4094] = byeter[1];
//    process.stack[4093] = byeter[2];
//    process.stack[4092] = byeter[3];
//    process.stack[4091] = byeter[4];
//    process.stack[4090] = byeter[5];
//    process.stack[4089] = byeter[6];
//    process.stack[4088] = byeter[7];
//
//    println!("{:?}", &process.stack[4088..]);
//
//    let mut blah1: *const u8;
//    unsafe {
//        //blah1 = process.stack.as_ptr().offset(4096 - (8 * 14));
//        //blah1 = process.stack.as_ptr().offset(4096 - 9);
//        //blah1 = process.stack.as_ptr().offset(4088);
//        // Offset by IP + registers
//        blah1 = process.stack.as_ptr().offset(4096 - (8 * (1 + 15)));
//        //blah1 = process.stack.as_ptr().offset(4088);
//    }
//    println!("{:?}", blah1);
//
//    unsafe {
//        post_switch(blah1 as usize);
//    }
//}
//
//#[naked]
//pub unsafe extern "C" fn post_switch(blah1: usize) {
//    asm!(
//        // Switch
//        "mov rsp, rdi",
//        "pop r15",
//        "pop r14",
//        "pop r13",
//        "pop r12",
//        "pop r11",
//        "pop r10",
//        "pop r9",
//        "pop r8",
//
//        "pop rbp",
//        "pop rdi",
//        "pop rsi",
//        "pop rdx",
//        "pop rcx",
//        "pop rbx",
//        "pop rax",
//        "ret",
//        options(noreturn)
//    );
//}
//
//#[naked]
//pub unsafe extern "C" fn pre_switch() {
//    asm!(
//        "push rax",
//        "push rbx",
//        "push rcx",
//        "push rdx",
//        "push rsi",
//        "push rdi",
//        "push rbp",
//
//        "push r8",
//        "push r9",
//        "push r10",
//        "push r11",
//        "push r12",
//        "push r13",
//        "push r14",
//        "push r15",
//        options(noreturn)
//    );
//}
