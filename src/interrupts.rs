use crate::native_util;
use core::arch::asm;

use lazy_static::lazy_static;
use pic8259::ChainedPics;
use spin;
use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame, PageFaultErrorCode};
use x86_64::{PhysAddr, VirtAddr};

use crate::multitasking;

pub const PIC_1_OFFSET: u8 = 32;
pub const PIC_2_OFFSET: u8 = PIC_1_OFFSET + 8;

pub static PICS: spin::Mutex<ChainedPics> =
    spin::Mutex::new(unsafe { ChainedPics::new(PIC_1_OFFSET, PIC_2_OFFSET) });

#[derive(Debug, Clone, Copy)]
#[repr(u8)]
pub enum InterruptIndex {
    Timer = PIC_1_OFFSET,
    Keyboard,
}

impl InterruptIndex {
    pub fn as_u8(self) -> u8 {
        self as u8
    }

    pub fn as_usize(self) -> usize {
        usize::from(self.as_u8())
    }
}

pub fn print_interrupt_masks() {
    unsafe { println!("{:?}", PICS.lock().read_masks()) };
}

lazy_static! {
    static ref IDT: InterruptDescriptorTable = {
        let mut idt = InterruptDescriptorTable::new();
        idt.divide_error.set_handler_fn(divide_error_handler);
        idt.debug.set_handler_fn(debug_handler);
        idt.non_maskable_interrupt.set_handler_fn(non_maskable_interrupt_handler);
        idt.invalid_tss.set_handler_fn(invalid_tss_handler);
        idt.invalid_opcode.set_handler_fn(invalid_opcode_handler);
        idt.segment_not_present.set_handler_fn(segment_not_present_handler);
        idt.stack_segment_fault.set_handler_fn(stack_segment_fault_handler);
        idt.general_protection_fault.set_handler_fn(general_protection_fault_handler);
        idt.breakpoint.set_handler_fn(breakpoint_handler);
        idt.double_fault.set_handler_fn(double_fault_handler);
        idt.page_fault.set_handler_fn(page_fault_handler);
        idt[InterruptIndex::Timer.as_usize()].set_handler_fn(timer_interrupt_handler);
        idt[InterruptIndex::Keyboard.as_usize()].set_handler_fn(keyboard_interrupt_handler);

        idt
    };
}

pub fn init_idt() {
    IDT.load();
}

extern "x86-interrupt" fn general_protection_fault_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) {
    println!("gen prot");
}


extern "x86-interrupt" fn stack_segment_fault_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) {
    println!("ss");
}


extern "x86-interrupt" fn segment_not_present_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) {
    println!("seg not pres");
}


extern "x86-interrupt" fn invalid_tss_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) {
    println!("invalid tss");
}


extern "x86-interrupt" fn device_not_available_handler(
    stack_frame: InterruptStackFrame,
) {
}


extern "x86-interrupt" fn invalid_opcode_handler(
    stack_frame: InterruptStackFrame,
) {
    println!("invalid op");
}


extern "x86-interrupt" fn bound_range_exceeded_handler(
    stack_frame: InterruptStackFrame,
) {
}


extern "x86-interrupt" fn overflow_handler(
    stack_frame: InterruptStackFrame,
) {
}


extern "x86-interrupt" fn non_maskable_interrupt_handler(
    stack_frame: InterruptStackFrame,
) {
}


extern "x86-interrupt" fn debug_handler(
    stack_frame: InterruptStackFrame,
) {
}

extern "x86-interrupt" fn divide_error_handler(
    stack_frame: InterruptStackFrame,
) {
}

extern "x86-interrupt" fn page_fault_handler(
    stack_frame: InterruptStackFrame,
    error_code: PageFaultErrorCode,
) {
    use x86_64::registers::control::Cr2;

    println!("EXCEPTION: PAGE FAULT");
    println!("Accessed Address: {:?}", Cr2::read());
    println!("Error Code: {:?}", error_code);
    println!("{:#?}", stack_frame);
    native_util::hlt_loop();
}

extern "x86-interrupt" fn breakpoint_handler(stack_frame: InterruptStackFrame) {
    println!("Encountered a breakpoint!");
    println!("EXCEPTION: BREAKPOINT\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn double_fault_handler(
    stack_frame: InterruptStackFrame,
    _error_code: u64,
) -> ! {
    println!("Encountered a double fault!");
    println!("EXCEPTION: {:#?}", stack_frame);
    panic!();
}

extern "x86-interrupt" fn timer_interrupt_handler(mut _stack_frame: InterruptStackFrame) {
    //unsafe {
    //    asm!(
    //        "mov {}, rsp",
    //        "mov {}, rbp",
    //        lateout(reg) stack_pointer,
    //        lateout(reg) frame_pointer,
    //    );
    //}

    //println!("Stack Pointer: 0x{:x}", stack_pointer);
    //println!("Frame Pointer: 0x{:x}", frame_pointer);
    use crate::multitasking;

    x86_64::instructions::interrupts::without_interrupts(|| {
        unsafe {
            let blahp: u64 = blahblah as u64;
            let mut frame = _stack_frame.as_mut();
            let mut vol_frame = frame.read();
            let mut sched = multitasking::SCHEDULER.lock();
            if sched.process_queue.len() > 0 {
                // Save stack pointer/IP
                let cpd = sched.current_pid as usize;
                //println!("Current PID {}, IP {}, RSP {}", cpd, sched.process_queue[cpd].ip, sched.process_queue[cpd].rsp);

                // We don't restore anything for the very first task
                if (sched.first_time) {
                    sched.first_time = false;
                } else {
                    sched.process_queue[cpd].ip = vol_frame.instruction_pointer.as_u64() as usize;
                    sched.process_queue[cpd].rsp = vol_frame.stack_pointer.as_u64() as usize;
                }
                //sched.process_queue[cpd].code_segment = vol_frame.code_segment;
                //sched.process_queue[cpd].stack_segment = vol_frame.stack_segment;
                //sched.process_queue[cpd].cpu_flags = vol_frame.cpu_flags;

                // Set new PID + restore instruction pointer + stack pointer
                let next_pid = ((sched.current_pid + 1) % sched.process_queue.len() as u64) as u64;
                sched.current_pid = next_pid;
                vol_frame.instruction_pointer = VirtAddr::new(sched.process_queue[sched.current_pid as usize].ip as u64);
                vol_frame.stack_pointer = VirtAddr::new(sched.process_queue[sched.current_pid as usize].rsp as u64);
                // TODO: figure out defaults for these
                //vol_frame.code_segment = sched.process_queue[sched.current_pid as usize].code_segment;
                //vol_frame.stack_segment = sched.process_queue[sched.current_pid as usize].stack_segment;
                //vol_frame.cpu_flags = sched.process_queue[sched.current_pid as usize].cpu_flags;
                frame.write(vol_frame);
            }
        }
    });

    unsafe {
        PICS.lock()
            .notify_end_of_interrupt(InterruptIndex::Timer.as_u8());
    }
}

//extern "x86-interrupt" fn timer_interrupt_handler(mut _stack_frame: InterruptStackFrame) {
//    //print!(".");
//    //use crate::native;
//    //native::trigger_timer();
//    let stack_pointer: usize;
//    let frame_pointer: usize;
//
//    unsafe {
//        asm!(
//            "mov {}, rsp",
//            "mov {}, rbp",
//            lateout(reg) stack_pointer,
//            lateout(reg) frame_pointer,
//        );
//    }
//
//    println!("Stack Pointer: 0x{:x}", stack_pointer);
//    println!("Frame Pointer: 0x{:x}", frame_pointer);
//
//    unsafe {
//        let blahp: u64 = blahblah as u64;
//        let mut frame = _stack_frame.as_mut();
//        let mut vol_frame = frame.read();
//        vol_frame.instruction_pointer = VirtAddr::new(blahp);
//        frame.write(vol_frame);
//    }
//
//    unsafe {
//        PICS.lock()
//            .notify_end_of_interrupt(InterruptIndex::Timer.as_u8());
//    }
//}

pub fn blahblah() {
    println!("This worked;");
}

extern "x86-interrupt" fn keyboard_interrupt_handler(_stack_frame: InterruptStackFrame) {
    use pc_keyboard::{layouts, DecodedKey, HandleControl, Keyboard, ScancodeSet1};
    use spin::Mutex;
    use x86_64::instructions::port::Port;

    lazy_static! {
        static ref KEYBOARD: Mutex<Keyboard<layouts::Us104Key, ScancodeSet1>> = Mutex::new(
            Keyboard::new(layouts::Us104Key, ScancodeSet1, HandleControl::Ignore)
        );
    }

    let mut keyboard = KEYBOARD.lock();
    let mut port = Port::new(0x60);

    let scancode: u8 = unsafe { port.read() };
    if scancode == 1 {
        println!("Escape");
        use crate::native;
        native::shutdown();
    }
    if let Ok(Some(key_event)) = keyboard.add_byte(scancode) {
        if let Some(key) = keyboard.process_keyevent(key_event) {
            match key {
                DecodedKey::Unicode(character) => print!("{}", character),
                DecodedKey::RawKey(key) => print!("{:?}", key),
            }
        }
    }

    unsafe {
        PICS.lock()
            .notify_end_of_interrupt(InterruptIndex::Keyboard.as_u8());
    }
}
