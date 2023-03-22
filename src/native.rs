

use limine::LimineBootInfoRequest;

use core::arch::asm;

//mod io;
//mod interrupts;

use crate::interrupts;

use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame};

use lazy_static::lazy_static;

lazy_static! {
    static ref IDT: InterruptDescriptorTable = {
        let mut idt = InterruptDescriptorTable::new();
        idt.breakpoint.set_handler_fn(breakpoint_handler);
        idt.double_fault.set_handler_fn(double_fault_handler);
        idt[interrupts::InterruptIndex::Timer.as_usize()].set_handler_fn(timer_interrupt_handler);
        idt[interrupts::InterruptIndex::Keyboard.as_usize()].set_handler_fn(keyboard_interrupt_handler);
        idt
    };
}

pub fn init_idt() {
    IDT.load();
}

extern "x86-interrupt" fn breakpoint_handler(stack_frame: InterruptStackFrame)
{
    println!("Encountered a breakpoint!");
    println!("EXCEPTION: BREAKPOINT\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn double_fault_handler(stack_frame: InterruptStackFrame, _error_code: u64) -> !
{
    println!("Encountered a double fault!");
    println!("EXCEPTION: {:#?}", stack_frame);
    panic!();
}

extern "x86-interrupt" fn timer_interrupt_handler(
    _stack_frame: InterruptStackFrame)
{
    print!(".");

    unsafe {
        interrupts::PICS.lock().notify_end_of_interrupt(interrupts::InterruptIndex::Timer.as_u8());
    }
}

extern "x86-interrupt" fn keyboard_interrupt_handler(
    _stack_frame: InterruptStackFrame)
{
    use pc_keyboard::{layouts, DecodedKey, HandleControl, Keyboard, ScancodeSet1};
    use spin::Mutex;
    use x86_64::instructions::port::Port;

    lazy_static! {
        static ref KEYBOARD: Mutex<Keyboard<layouts::Us104Key, ScancodeSet1>> =
            Mutex::new(Keyboard::new(layouts::Us104Key, ScancodeSet1,
                HandleControl::Ignore)
            );
    }

    let mut keyboard = KEYBOARD.lock();
    let mut port = Port::new(0x60);

    let scancode: u8 = unsafe { port.read() };
    if let Ok(Some(key_event)) = keyboard.add_byte(scancode) {
        if let Some(key) = keyboard.process_keyevent(key_event) {
            match key {
                DecodedKey::Unicode(character) => print!("{}", character),
                DecodedKey::RawKey(key) => print!("{:?}", key),
            }
        }
    }

    //unsafe {
    //    for i in 0..0x9999 {
    //        *(i as *mut u64) = 42;
    //    }
    //}

    unsafe {
        interrupts::PICS.lock().notify_end_of_interrupt(interrupts::InterruptIndex::Keyboard.as_u8());
    }
}

static BOOTLOADER_INFO: LimineBootInfoRequest = LimineBootInfoRequest::new(0);

#[no_mangle]
pub extern "C" fn _start() -> ! {
    println!("Welcome to Pleroma!");

    if let Some(bootinfo) = BOOTLOADER_INFO.get_response().get() {
        println!(
            "booted by {} v{}",
            bootinfo.name.to_str().unwrap().to_str().unwrap(),
            bootinfo.version.to_str().unwrap().to_str().unwrap(),
        );

    }

    init_idt();
    unsafe {interrupts::PICS.lock().initialize()};
    println!(" Interrupts enabled: {}", x86_64::instructions::interrupts::are_enabled());

    x86_64::instructions::interrupts::enable();

    println!(" Interrupts enabled: {}", x86_64::instructions::interrupts::are_enabled());

    unsafe {println!("{}", interrupts::PICS.lock().read_masks()[0])};
    unsafe {println!("{}", interrupts::PICS.lock().read_masks()[1])};

    unsafe {interrupts::PICS.lock().write_masks(0, 0)};

    //x86_64::instructions::interrupts::int3(); // new
    loop {
        x86_64::instructions::hlt();
    };
}

#[panic_handler]
fn rust_panic(info: &core::panic::PanicInfo) -> ! {
    println!("{}", info);
    hcf();
}

/// Die, spectacularly.
pub fn hcf() -> ! {
    loop {
    }
}
