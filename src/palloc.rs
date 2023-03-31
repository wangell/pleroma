extern crate alloc;
use alloc::alloc::{GlobalAlloc, Layout};
use x86_64::structures::paging::{
    mapper::MapToError, FrameAllocator, Mapper, Page, PageTableFlags, Size4KiB, PhysFrame
};
use x86_64::{PhysAddr, VirtAddr};
use linked_list_allocator::LockedHeap;
use crate::memory::BootInfoAllocatorTrait;

pub const HEAP_START: usize = 0x_4444_4444_0000;
pub const HEAP_SIZE: usize = 1024 * 1024 * 50;

pub fn init_heap<T>(
    mapper: &mut impl Mapper<Size4KiB>,
    frame_allocator: &mut T,
) -> Result<(), MapToError<Size4KiB>>
    where T: FrameAllocator<Size4KiB> + BootInfoAllocatorTrait,
{
    let page_range = {
        let heap_start = VirtAddr::new(HEAP_START as u64);
        let heap_end = heap_start + HEAP_SIZE - 1u64;
        let heap_start_page = Page::containing_address(heap_start);
        let heap_end_page = Page::containing_address(heap_end);
        Page::range_inclusive(heap_start_page, heap_end_page)
    };

    // Memory should be relatively untouched at this point and should have a large contiguous space
    let heap_size = ((HEAP_START as u64) + (HEAP_SIZE as u64) - 1u64) - HEAP_START as u64;
    let n_frames = (heap_size / 4096) + 1;
    // FIXME: this should fallback to single frame allocation on error
    let frame_start = frame_allocator.allocate_contiguous(n_frames);

    for (idx, page) in page_range.enumerate() {
        let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;
        let frame = PhysFrame::from_start_address(PhysAddr::new(frame_start.start_address().as_u64() + (idx as u64 * 4096))).unwrap();
        unsafe { mapper.map_to(page, frame, flags, frame_allocator)?.flush() };
    }

    unsafe {
        ALLOCATOR.lock().init(HEAP_START, HEAP_SIZE);
    }

    Ok(())
}

#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();

#[alloc_error_handler]
fn alloc_error_handler(layout: alloc::alloc::Layout) -> ! {
    panic!("allocation error: {:?}", layout)
}
