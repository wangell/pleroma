use limine::LimineBootInfoRequest;
use limine::LimineBootTimeRequest;
use limine::LimineFramebufferRequest;
use limine::LimineHhdmRequest;

use core::arch::asm;
extern crate alloc;

use alloc::vec;

use crate::interrupts;
use crate::memory;
use crate::native_util;
use crate::palloc;
use crate::vm_core;
use crate::vm_core::{Vat, Msg};
use crate::vm;
use crate::pbin;

use lazy_static::lazy_static;

use x86_64::structures::paging::{OffsetPageTable, PageTable, Translate};
use x86_64::{PhysAddr, VirtAddr};

static BOOTLOADER_INFO: LimineBootInfoRequest = LimineBootInfoRequest::new(0);
static HH_INFO: LimineHhdmRequest = LimineHhdmRequest::new(0);
static FB_INFO: LimineFramebufferRequest = LimineFramebufferRequest::new(0);

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;

use spin;

pub static VatList: spin::Mutex<BTreeMap<u32, Vat>> = spin::Mutex::new(BTreeMap::new());
pub static current_vat: spin::Mutex<u32> = spin::Mutex::new(0);

pub fn trigger_timer() {
    let mut cv = current_vat.lock();
    *cv += 1;
    let ll = VatList.lock().len() as u32;
    if (ll > 0) {
        *cv = *cv % ll;
    }
}

fn target_function() {
    println!("JUMPED!");
}

unsafe fn jmp_to_target_function() {
    let target_fn_address = target_function as *const ();
    asm!(
        "jmp {0}",
        in(reg) target_fn_address,
        options(noreturn)
    );
}

pub fn shutdown() {
    unsafe {
        use x86_64::instructions::port::{PortGeneric, WriteOnlyAccess};
        let mut blah: PortGeneric<u16, WriteOnlyAccess> = PortGeneric::new(0x604);
        blah.write(0x2000);
    }
}

pub fn clear_screen(fb1: &limine::NonNullPtr<limine::LimineFramebuffer>, ya: bool) {

    let mut qq = Box::new(Vec::<u8>::new());
    for y in (0..100000) {
        qq.push(0);
    }
    println!("here");

    for y in (0..fb1.height) {
        for x in (0..fb1.width * 4).step_by(4) {
            unsafe {
                let blah: *mut u8 = fb1.address.as_ptr().unwrap() as *mut u8;
                let base_address = x as isize;
                let y_offset = (y * fb1.pitch) as isize;
                //*blah.offset(x as isize * (fb1.bpp / 8) as isize) = 0xFF;

                use core::ptr;

                if (ya) {
                    let slice: &[u8] = &*qq;
                    let raw_ptr: *const u8 = slice.as_ptr(); 
                    ptr::copy(raw_ptr, blah, qq.len());
                } else {
                    *blah.offset(y_offset + base_address + 2) = 0xFF;
                    *blah.offset(y_offset + base_address + 1) = 0xFF;
                    *blah.offset(y_offset + base_address) = 0xFF;
                }
            }
        }
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

    // Turn on interrupts + mask
    unsafe { interrupts::PICS.lock().initialize() };
    //print!("Enabling interrupts...");
    x86_64::instructions::interrupts::enable();
    //println!(" enabled.");
    unsafe { interrupts::PICS.lock().write_masks(0, 0) };

    let mem_offset: u64 = HH_INFO.get_response().get().unwrap().offset;
    let phys_mem_offset = VirtAddr::new(mem_offset);

    let mut mapper = unsafe { memory::init(phys_mem_offset) };
    let mut frame_allocator = unsafe { memory::BootInfoFrameAllocator::init() };

    palloc::init_heap(&mut mapper, &mut frame_allocator).expect("heap initialization failed");

    {
        let mut vl = VatList.lock();
        vl.insert(0, Vat::new());
        vl.insert(1, Vat::new());
    }

    //let mut x: u64 = 4;
    //unsafe {
    //    asm!("jmp ",);
    //}
    //println!("{}", x);

    //native_util::hlt_loop();
    let mut fb1 = &FB_INFO.get_response().get().unwrap().framebuffers()[0];

    let program : Vec<u8> = vec![1, 0, 0, 0, 7, 0, 5, 0, 0, 7, 0, 1, 6];
    //pbin::disassemble(&program);
    loop {
        {
            let z = *current_vat.lock();

            let mut blah = VatList.lock();
            let real_q = &mut blah.get_mut(&z).unwrap();
            ////println!("{}", real_q.vat_id);
            let msg = vm_core::Msg {
                src_address: vm_core::EntityAddress::new(0, 0, 0),
                dst_address: vm_core::EntityAddress::new(0, 0, 0),
                promise_id: None,
                is_response: false,
                function_name: String::from("main"),
                values: Vec::new(),
                function_id: 0,
            };

            vm::run_msg(&program, real_q, &msg);

            //unsafe {
            //    jmp_to_target_function();
            //}
        }
        //clear_screen(fb1, true);
        //clear_screen(fb1, false);
        x86_64::instructions::hlt();
    }
}

#[panic_handler]
fn rust_panic(info: &core::panic::PanicInfo) -> ! {
    println!("{}", info);
    hcf();
}

pub fn hcf() -> ! {
    loop {}
}
