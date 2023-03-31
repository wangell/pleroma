use limine::LimineMemmapEntry;
use limine::LimineMemmapRequest;
use limine::LimineMemoryMapEntryType;
use limine::NonNullPtr;
use x86_64::structures::paging::{
    FrameAllocator, Mapper, OffsetPageTable, Page, PageTable, PhysFrame, Size4KiB, Translate,
};
use x86_64::{PhysAddr, VirtAddr};

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

pub struct BootInfoFrameAllocator {
    next: usize,
    max_entries: usize,
}

static mut bitmap: [u8; 50000] = [0; 50000];
static MM_INFO: LimineMemmapRequest = LimineMemmapRequest::new(0);

unsafe impl FrameAllocator<Size4KiB> for BootInfoFrameAllocator {
    fn allocate_frame(&mut self) -> Option<PhysFrame> {

        let frame_idx = self.get_contiguous_range(1).0 as u64;
        let frame = Some(PhysFrame::from_start_address(PhysAddr::new(frame_idx * 4096)).unwrap());
        self.mark_page_used(frame_idx as usize);

        frame
    }
}

impl BootInfoFrameAllocator {
    pub unsafe fn init() -> Self {
        let mut bifa = Self {
            next: 0,
            max_entries: 0
        };

        let mmap = MM_INFO
            .get_response()
            .get()
            .expect("failed to get memory map")
            .memmap();

        unsafe {
            // Assume all frames are used
            for i in 0..bitmap.len() {
                bitmap[i] = 0xFF;
            }

            for area in mmap
                .iter()
                .filter(|entry| entry.typ == LimineMemoryMapEntryType::Usable)
            {
                let frame_start = area.base / 4096;
                let frame_end = (area.base + area.len) / 4096;

                // Mark until one page before frame_end
                for q in frame_start..frame_end {
                    bifa.mark_page_unused(q as usize);
                }
            }
        }

        bifa
    }

    fn get_contiguous_range(&mut self, n: u64) -> (u64, u64) {
        // [0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1]
        let mut n_start = 0;
        let mut n_end = 0;

        if (n == 0) {
            panic!();
        }

        while (!self.is_page_free(n_start)) {
            n_start = n_start + 1;
        }

        n_end = n_start;

        while (n_end - n_start + 1) < n as usize {
            if self.is_page_free((n_end + 1) as usize) {
                n_end += 1;
            } else {

                n_start = n_end + 1;

                while !self.is_page_free(n_start) {
                    n_start = n_start + 1;
                }

                n_end = n_start;
            }

            unsafe {
                if n_end > bitmap.len() as usize {
                    panic!()
                };
            }
        }

        (n_start as u64, n_end as u64)
    }

    pub fn is_page_free(&mut self, page_n: usize) -> bool {
        let base_idx = page_n / 8;
        let offset_idx = page_n % 8;

        unsafe {
            (bitmap[base_idx] >> (7 - offset_idx) & 1) == 0
        }
    }

    pub fn mark_page_used(&mut self, page_n: usize) {
        let base_idx = page_n / 8;
        let offset_idx = page_n % 8;

        unsafe {
            if (!self.is_page_free(page_n)) {
                panic!("Trying to mark an already used page: {}!", page_n);
            }

            bitmap[base_idx] |= (1 << (7 - offset_idx));
        }
    }

    pub fn mark_page_unused(&mut self, page_n: usize) {
        let base_idx = page_n / 8;
        let offset_idx = page_n % 8;

        unsafe {
            bitmap[base_idx] &= !(1 << (7 - offset_idx));
        }
    }
}

pub trait BootInfoAllocatorTrait {
    fn allocate_contiguous(&mut self, n: u64) -> PhysFrame;
}

impl BootInfoAllocatorTrait for BootInfoFrameAllocator {
    fn allocate_contiguous(&mut self, n: u64) -> PhysFrame {
        let (start, end) = self.get_contiguous_range(n);
        for i in start..=end {
            self.mark_page_used(i as usize);
        }

        PhysFrame::from_start_address(PhysAddr::new(start * 4096)).unwrap()
    }
}

