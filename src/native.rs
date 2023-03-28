use core::arch::asm;
extern crate alloc;

use crate::filesys::fat;
use crate::interrupts;
use crate::memory;
use crate::multitasking;
use crate::native_util;
use crate::palloc;
use crate::pbin;
use crate::vm;
use crate::vm_core;
use crate::vm_core::{Msg, Vat};

use limine::LimineBootInfoRequest;
use limine::LimineBootTimeRequest;
use limine::LimineFramebufferRequest;
use limine::LimineHhdmRequest;
use limine::LimineModuleRequest;

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

static BOOTLOADER_INFO: LimineBootInfoRequest = LimineBootInfoRequest::new(0);
static HH_INFO: LimineHhdmRequest = LimineHhdmRequest::new(0);
static FB_INFO: LimineFramebufferRequest = LimineFramebufferRequest::new(0);
static INITRD: LimineModuleRequest = LimineModuleRequest::new(0);

pub static vat_list: spin::Mutex<BTreeMap<u32, Vat>> = spin::Mutex::new(BTreeMap::new());
pub static available_vats: spin::Mutex<Vec<u32>> = spin::Mutex::new(Vec::new());

pub fn shutdown() {
    unsafe {
        let mut blah: PortGeneric<u16, WriteOnlyAccess> = PortGeneric::new(0x604);
        blah.write(0x2000);
    }
}

#[no_mangle]
pub fn boot() -> ! {
    println!("Welcome to Pleroma!");
    println!("Less is more, and nothing at all is a victory.");


    if let Some(bootinfo) = BOOTLOADER_INFO.get_response().get() {
        println!(
            "booted by {} v{}",
            bootinfo.name.to_str().unwrap().to_str().unwrap(),
            bootinfo.version.to_str().unwrap().to_str().unwrap(),
        );
    }

    interrupts::init_idt();

    // Setup virtual memory + heap
    let mem_offset: u64 = HH_INFO.get_response().get().unwrap().offset;
    let phys_mem_offset = VirtAddr::new(mem_offset);
    let mut mapper = unsafe { memory::init(phys_mem_offset) };
    let mut frame_allocator = unsafe { memory::BootInfoFrameAllocator::init() };
    palloc::init_heap(&mut mapper, &mut frame_allocator).expect("heap initialization failed");

    use crate::parser;
    println!("{:?}", parser::parse("~sys;

ε Nodeman {blah : far u32, yolo : loc u32} {

  δ create() → loc u32 {
  }

  δ createblah() → loc u32 {
    ↵ 9 + 11 + 2;
  }
}
"));

    let modules = INITRD.get_response().get().unwrap().modules();

    println!("Initrd modules:");
    for module in modules {
        if let Some(path) = module.path.to_str() {
            println!("\t{:?}", path);

            if let Some(base_addr) = module.base.as_ptr() {
                use core::slice::{from_raw_parts, from_raw_parts_mut};
                let safe_slice;
                unsafe {
                    safe_slice = from_raw_parts_mut(base_addr, module.length as usize);
                }
                let fat_fs = fat::FatFs::new(safe_slice, fat::FatType::Fat16);
                let entries = fat_fs.list_root_dir();

                println!("Root dir {}:", path.to_str().unwrap());
                for i in entries {
                    println!("\t{}", i.filename());
                }
            }
        }
    }

    // Schedule first vat
    {
        let mut sched = multitasking::SCHEDULER.lock();
        sched.new_process(vat_runner as usize);
        sched.new_process(vat_runner as usize);
    }

    {
        let mut vl = vat_list.lock();
        vl.insert(0, Vat::new());
        vl.insert(1, Vat::new());
        let mut avail = available_vats.lock();
        avail.push(0);
        avail.push(1);
    }

    // Wait for first interrupt to trigger scheduler
    println!("Boot complete: waiting for first scheduled task.");

    unsafe { interrupts::PICS.lock().initialize() };
    unsafe { interrupts::PICS.lock().write_masks(0, 0) };

    x86_64::instructions::interrupts::enable();
    loop {
        x86_64::instructions::hlt();
    }
}

fn vat_runner() {
    let program: Vec<u8> = vec![1, 0, 0, 0, 7, 0, 5, 0, 0, 7, 0, 1, 6];

    loop {
        println!("Hello from proc1!");
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

        x86_64::instructions::hlt();
    }
}

#[panic_handler]
fn rust_panic(info: &core::panic::PanicInfo) -> ! {
    println!("{}", info);
    native_util::hcf();
}
