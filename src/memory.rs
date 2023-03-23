use x86_64::structures::paging::{OffsetPageTable, PageTable, Translate};
use x86_64::{PhysAddr, VirtAddr};
use limine::LimineMemmapRequest;

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

pub struct BootInfoFrameAllocator {
    next: usize,
}

static MM_INFO: LimineMemmapRequest = LimineMemmapRequest::new(0);

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
        Self { next: 0 }
    }
}
