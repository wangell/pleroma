use core::arch::asm;
extern crate alloc;

use crate::drivers;
use crate::filesys::fat;
use crate::initrd;
use crate::interrupts;
use crate::memory;
use crate::multitasking;
use crate::native_util;
use crate::palloc;
use crate::pbin;
use crate::pci;
use crate::vm_core;
use crate::vm_core::{Msg, Vat};
use core::mem;
use core::ptr;

use lazy_static::lazy_static;
use x86_64::instructions::port::{PortGeneric, WriteOnlyAccess};
use x86_64::structures::paging::{OffsetPageTable, PageTable, Translate};
use x86_64::{PhysAddr, VirtAddr};

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec;
use alloc::vec::Vec;
use spin;

use limine::LimineBootInfoRequest;
use limine::LimineBootTimeRequest;
use limine::LimineFramebufferRequest;
use limine::LimineHhdmRequest;

static BOOTLOADER_INFO: LimineBootInfoRequest = LimineBootInfoRequest::new(0);
static HH_INFO: LimineHhdmRequest = LimineHhdmRequest::new(0);
static FB_INFO: LimineFramebufferRequest = LimineFramebufferRequest::new(0);

pub static vat_list: spin::Mutex<BTreeMap<u32, Vat>> = spin::Mutex::new(BTreeMap::new());
pub static available_vats: spin::Mutex<Vec<u32>> = spin::Mutex::new(Vec::new());

pub static MAPPER: spin::Mutex<Option<OffsetPageTable<'static>>> = spin::Mutex::new(None);
pub static FRAME_ALLOCATOR: spin::Mutex<Option<memory::BootInfoFrameAllocator>> =
    spin::Mutex::new(None);

pub fn print_bootloader_info() {
    if let Some(bootinfo) = BOOTLOADER_INFO.get_response().get() {
        println!(
            "booted by {} v{}",
            bootinfo.name.to_str().unwrap().to_str().unwrap(),
            bootinfo.version.to_str().unwrap().to_str().unwrap(),
        );
    }
}

pub fn write_buffer(fb: &limine::LimineFramebuffer, buf: *mut u8, buf_size: usize) {
    let mut write_add = fb.address.as_ptr().unwrap();

    for i in 0..100 {
        unsafe {
            core::ptr::copy(buf, write_add, buf_size);
        }
    }
}

#[no_mangle]
pub fn boot() -> ! {
    println!("Welcome to Pleroma!");
    println!("Less is more, and nothing at all is a victory.");

    print_bootloader_info();

    interrupts::init_idt();

    // Setup virtual memory + heap
    {
        let mut frame_allocator = FRAME_ALLOCATOR.lock();
        let mut mapper = MAPPER.lock();

        let mem_offset: u64 = HH_INFO.get_response().get().unwrap().offset;
        let phys_mem_offset = VirtAddr::new(mem_offset);
        *mapper = unsafe { Some(memory::init(phys_mem_offset)) };
        *frame_allocator = unsafe { Some(memory::BootInfoFrameAllocator::init()) };

        palloc::init_heap(
            mapper.as_mut().unwrap(),
            frame_allocator.as_mut().unwrap(),
        )
        .expect("heap initialization failed");
    }

    initrd::setup_initrd();

    // Scan + setup PCI devices
    pci::pciScanBus();

    // Schedule first vat
    {
        let mut sched = multitasking::SCHEDULER.lock();
        //sched.new_process(vat_runner as usize);
        sched.new_process(vat_runner as usize);
        sched.new_process(halt_proc as usize);
    }

    {
        let mut vl = vat_list.lock();
        vl.insert(0, Vat::new());
        vl.insert(1, Vat::new());
        let mut avail = available_vats.lock();
        avail.push(0);
        avail.push(1);
    }

    let fb = &FB_INFO.get_response().get().unwrap().framebuffers()[0];

    println!("BPP : {} {}", fb.width, fb.height);
    //let mut buf = Box::new(vec![0xFF; (fb.width * fb.height * 4) as usize]);
    //write_buffer(&*fb, buf.as_mut_ptr(), (fb.width * fb.height * 4) as usize);
    use crate::framebuffer;
    let mut our_fb = framebuffer::Framebuffer::new(fb);

    our_fb.write_str("Welcome to Pleroma!", 1, 2);
    //our_fb.swap_buffer();

    // Wait for first interrupt to trigger scheduler

    unsafe { interrupts::PICS.lock().initialize() };
    unsafe { interrupts::PICS.lock().write_masks(0, 0) };

    x86_64::instructions::interrupts::enable();

    loop {
        x86_64::instructions::hlt();
    }
}

fn halt_proc() {
    loop {
        x86_64::instructions::hlt();
    }
}

fn ksleep(ms: u32) {
    {
        let mut sched = multitasking::SCHEDULER.lock();
        let cpd = sched.current_pid as usize;
        sched.process_queue[cpd].status = multitasking::ProcStatus::Sleeping;
        sched.process_queue[cpd].sleep_timer = ms.into();
    }
    unsafe {
        asm!(
            "int 32"
        );
    }
}

fn call_other_func(a: u32, b: u32) -> u32{
    ksleep(2000);
    a+b
}

fn vat_runner() {
    let program: Vec<u8> = vec![1, 0, 0, 0, 7, 0, 5, 0, 0, 7, 0, 1, 6];


    loop {
        // Lift vat onto hotplate

        println!("Hello from proc1!");

        ksleep(2000);
        println!("After 5 seconds!");

        call_other_func(1, 2);
        println!("did func");
        //let mut try_vat: Option<u32> = None;
        //let mut real_q: Option<&mut Vat> = None;

        //{
        //    let mut av = available_vats.lock();
        //    try_vat = av.pop();
        //}

        //if let Some(z) = try_vat {
        //    {
        //        let mut tt = vat_list.lock();
        //        real_q = tt.get_mut(&z);
        //    }

        //    if let Some(q) = real_q {
        //        let msg = vm_core::Msg {
        //            src_address: vm_core::EntityAddress::new(0, 0, 0),
        //            dst_address: vm_core::EntityAddress::new(0, 0, 0),
        //            promise_id: None,
        //            is_response: false,
        //            function_name: String::from("main"),
        //            values: Vec::new(),
        //            function_id: 0,
        //        };

        //        vm::run_msg(&program, q, &msg);
        //    }
        //}

        //if let Some(z) = try_vat {
        //    let mut av = available_vats.lock();
        //    av.push(z);
        //}

        // Release vat, if vat list is empty, hlt()

        x86_64::instructions::hlt();
    }
}

#[panic_handler]
fn rust_panic(info: &core::panic::PanicInfo) -> ! {

    println!("{}", info);
    native_util::hcf();
}
