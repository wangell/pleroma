use limine::LimineBootInfoRequest;
use limine::LimineHhdmRequest;
use limine::LimineMemmapRequest;

use core::arch::asm;
extern crate alloc;


//mod io;
//mod interrupts;

use crate::interrupts;

use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame, PageFaultErrorCode};

use lazy_static::lazy_static;

use x86_64::structures::paging::OffsetPageTable;
use x86_64::{structures::paging::PageTable, structures::paging::Translate, VirtAddr};

pub unsafe fn init(physical_memory_offset: VirtAddr) -> OffsetPageTable<'static> {
    let level_4_table = active_level_4_table(physical_memory_offset);
    OffsetPageTable::new(level_4_table, physical_memory_offset)
}

unsafe fn active_level_4_table(physical_memory_offset: VirtAddr) -> &'static mut PageTable {
    use x86_64::registers::control::Cr3;

    let (level_4_table_frame, _) = Cr3::read();

    let phys = level_4_table_frame.start_address();
    let virt = physical_memory_offset + phys.as_u64();
    let page_table_ptr: *mut PageTable = virt.as_mut_ptr();

    &mut *page_table_ptr // unsafe
}

pub struct EmptyFrameAllocator;

unsafe impl FrameAllocator<Size4KiB> for EmptyFrameAllocator {
    fn allocate_frame(&mut self) -> Option<PhysFrame> {
        None
    }
}

use x86_64::structures::paging::{FrameAllocator, Mapper, Page, PhysFrame, Size4KiB};

/// Creates an example mapping for the given page to frame `0xb8000`.
pub fn create_example_mapping(
    page: Page,
    mapper: &mut OffsetPageTable,
    frame_allocator: &mut impl FrameAllocator<Size4KiB>,
) {
    use x86_64::structures::paging::PageTableFlags as Flags;

    let frame = PhysFrame::containing_address(PhysAddr::new(0xb8000));
    let flags = Flags::PRESENT | Flags::WRITABLE;

    let map_to_result = unsafe {
        // FIXME: this is not safe, we do it only for testing
        mapper.map_to(page, frame, flags, frame_allocator)
    };
    map_to_result.expect("map_to failed").flush();
}

use x86_64::PhysAddr;

lazy_static! {
    static ref IDT: InterruptDescriptorTable = {
        let mut idt = InterruptDescriptorTable::new();
        idt.breakpoint.set_handler_fn(breakpoint_handler);
        idt.double_fault.set_handler_fn(double_fault_handler);
        idt.page_fault.set_handler_fn(page_fault_handler);
        idt[interrupts::InterruptIndex::Timer.as_usize()].set_handler_fn(timer_interrupt_handler);
        idt[interrupts::InterruptIndex::Keyboard.as_usize()]
            .set_handler_fn(keyboard_interrupt_handler);
        idt
    };
}

pub fn init_idt() {
    IDT.load();
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
    hlt_loop();
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

extern "x86-interrupt" fn timer_interrupt_handler(_stack_frame: InterruptStackFrame) {
    //print!(".");

    unsafe {
        interrupts::PICS
            .lock()
            .notify_end_of_interrupt(interrupts::InterruptIndex::Timer.as_u8());
    }
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
    if let Ok(Some(key_event)) = keyboard.add_byte(scancode) {
        if let Some(key) = keyboard.process_keyevent(key_event) {
            match key {
                DecodedKey::Unicode(character) => print!("{}", character),
                DecodedKey::RawKey(key) => print!("{:?}", key),
            }
        }
    }

    unsafe {
        interrupts::PICS
            .lock()
            .notify_end_of_interrupt(interrupts::InterruptIndex::Keyboard.as_u8());
    }
}

static BOOTLOADER_INFO: LimineBootInfoRequest = LimineBootInfoRequest::new(0);

static HH_INFO: LimineHhdmRequest = LimineHhdmRequest::new(0);

static MM_INFO: LimineMemmapRequest = LimineMemmapRequest::new(0);

pub struct BootInfoFrameAllocator {
    next: usize,
}

unsafe impl FrameAllocator<Size4KiB> for BootInfoFrameAllocator {
    fn allocate_frame(&mut self) -> Option<PhysFrame> {
        use limine::LimineMemoryMapEntryType;
        let mmap = MM_INFO
            .get_response()
            .get()
            .expect("failed to get memory map")
            .memmap();
        let mut usable_frames = mmap
            .iter()
            .filter(|entry| entry.typ == LimineMemoryMapEntryType::Usable)
            .map(|area| {
                let frame_addr = area.base;
                let frame_end = area.base + area.len;
                let frame_size = frame_end - frame_addr;
                let num_frames = frame_size / 4096;
                let start_frame = PhysFrame::containing_address(PhysAddr::new(frame_addr));
                (0..num_frames).map(move |i| start_frame + i)
            })
            .flatten();
        let frame = usable_frames.nth(self.next).clone();
        self.next += 1;

        frame
    }
}

impl BootInfoFrameAllocator {
    pub unsafe fn init() -> Self {
        Self {
            next: 0,
        }
    }
}
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;

#[no_mangle]
pub fn boot() -> ! {
    //println!("Welcome to Pleroma!");
    //println!("Less is more, and nothing at all is a victory.");


    if let Some(bootinfo) = BOOTLOADER_INFO.get_response().get() {
        //println!(
        //    "booted by {} v{}",
        //    bootinfo.name.to_str().unwrap().to_str().unwrap(),
        //    bootinfo.version.to_str().unwrap().to_str().unwrap(),
        //);
    }

    init_idt();

    // Turn on interrupts + mask
    unsafe { interrupts::PICS.lock().initialize() };
    //print!("Enabling interrupts...");
    x86_64::instructions::interrupts::enable();
    //println!(" enabled.");
    unsafe { interrupts::PICS.lock().write_masks(0, 0) };

    let mem_offset: u64 = HH_INFO.get_response().get().unwrap().offset;

    use x86_64::registers::control::Cr3;

    let (level_4_page_table, _) = Cr3::read();
    //println!(
    //    "Level 4 page table at: {:?}",
    //    level_4_page_table.start_address()
    //);

    use x86_64::VirtAddr;

    let phys_mem_offset = VirtAddr::new(mem_offset);
    let l4_table = unsafe { active_level_4_table(phys_mem_offset) };

    for (i, entry) in l4_table.iter().enumerate() {
        if !entry.is_unused() {
            //println!("L4 Entry {}: {:?}", i, entry);

            // get the physical address from the entry and convert it
            let phys = entry.frame().unwrap().start_address();
            let virt = phys.as_u64() + mem_offset;
            let ptr = VirtAddr::new(virt).as_mut_ptr();
            let l3_table: &PageTable = unsafe { &*ptr };

            // print non-empty entries of the level 3 table
            for (i, entry) in l3_table.iter().enumerate() {
                if !entry.is_unused() {
                    //println!("  L3 Entry {}: {:?}", i, entry);
                }
            }
        }
    }

    let addresses = [
        // the identity-mapped vga buffer page
        0xb8000,
        0x201008,
        0x0100_0020_1a10,
        // virtual address mapped to physical address 0
        mem_offset,
    ];

    let mut mapper = unsafe { init(phys_mem_offset) };

    for &address in &addresses {
        let virt = VirtAddr::new(address);
        let phys = mapper.translate_addr(virt);
        //println!("{:?} -> {:?}", virt, phys);
    }

    let mut frame_allocator = unsafe { BootInfoFrameAllocator::init() };


    // map an unused page
    let page = Page::containing_address(VirtAddr::new(0));
    create_example_mapping(page, &mut mapper, &mut frame_allocator);

    // write the string `New!` to the screen through the new mapping
    let page_ptr: *mut u64 = page.start_address().as_mut_ptr();
    unsafe { page_ptr.offset(400).write_volatile(0x_f021_f077_f065_f04e) };

        init_heap(&mut mapper, &mut frame_allocator)
        .expect("heap initialization failed");

    let x = Box::new(41);
        println!("heap_value at {:p}", x);

    let mut zz: BTreeMap<u32, String> = BTreeMap::new();
    zz.insert(0, String::from("Hillo"));

    println!("{}", zz[&0]);

    hlt_loop();
}

fn hlt_loop() -> ! {
    loop {
        x86_64::instructions::hlt();
    }
}

#[panic_handler]
fn rust_panic(info: &core::panic::PanicInfo) -> ! {
    println!("{}", info);
    hcf();
}

/// Die, spectacularly.
pub fn hcf() -> ! {
    loop {}
}

// Alloc
use alloc::alloc::{GlobalAlloc, Layout};
use core::ptr::null_mut;

pub const HEAP_START: usize = 0x_4444_4444_0000;
pub const HEAP_SIZE: usize = 100 * 1024; // 100 KiB

use x86_64::{
    structures::paging::{
        mapper::MapToError, PageTableFlags
    },
};

pub fn init_heap(
    mapper: &mut impl Mapper<Size4KiB>,
    frame_allocator: &mut impl FrameAllocator<Size4KiB>,
) -> Result<(), MapToError<Size4KiB>> {
    let page_range = {
        let heap_start = VirtAddr::new(HEAP_START as u64);
        let heap_end = heap_start + HEAP_SIZE - 1u64;
        let heap_start_page = Page::containing_address(heap_start);
        let heap_end_page = Page::containing_address(heap_end);
        Page::range_inclusive(heap_start_page, heap_end_page)
    };

    for page in page_range {
        let frame = frame_allocator
            .allocate_frame()
            .ok_or(MapToError::FrameAllocationFailed)?;
        let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;
        unsafe {
            mapper.map_to(page, frame, flags, frame_allocator)?.flush()
        };
    }

      unsafe {
        ALLOCATOR.lock().init(HEAP_START, HEAP_SIZE);
    }

    Ok(())
}

pub struct Dummy;

unsafe impl GlobalAlloc for Dummy {
    unsafe fn alloc(&self, _layout: Layout) -> *mut u8 {
        null_mut()
    }

    unsafe fn dealloc(&self, _ptr: *mut u8, _layout: Layout) {
        panic!("dealloc should be never called")
    }
}

use linked_list_allocator::LockedHeap;

#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();

#[alloc_error_handler]
fn alloc_error_handler(layout: alloc::alloc::Layout) -> ! {
    panic!("allocation error: {:?}", layout)
}
