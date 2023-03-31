use crate::common::vec;
use crate::common::Box;
use crate::common::Vec;

const vga_char_width: u8 = 8;
const vga_char_height: u8 = 16;

pub struct Framebuffer {
    buffer: Box<Vec<u8>>,
    target_address: *mut u8,
    buffer_width: u64,
    buffer_height: u64,
}

static vga_font_data: &[u8] = include_bytes!("../vga.font");

impl Framebuffer {
    pub fn new(fb: &limine::LimineFramebuffer) -> Self {
        Self {
            buffer: Box::new(vec![0xFF; (fb.width * fb.height * 4) as usize]),
            target_address: fb.address.as_ptr().unwrap(),
            buffer_width: fb.width,
            buffer_height: fb.height,
        }
    }

    pub fn swap_buffer(&mut self) {
        unsafe {
            // FIXME: check if this needs to be volatile
            core::ptr::copy(
                self.buffer.as_ptr(),
                self.target_address,
                (self.buffer_width * self.buffer_height * 4)
                    .try_into()
                    .unwrap(),
            );
        }
    }

    pub fn draw_pixel(&mut self, x: usize, y: usize, r: u8, g: u8, b: u8) {
        self.buffer[((x * 4) + (y * self.buffer_width as usize * 4)) as usize] = b;
        self.buffer[((x * 4) + (y * self.buffer_width as usize * 4) + 1) as usize] = g;
        self.buffer[((x * 4) + (y * self.buffer_width as usize * 4) + 2) as usize] = r;
    }

    pub fn write_str(&mut self, s: &str, x: usize, y: usize) {
        let mut dx = 0;
        let mut dy = 0;

        for z in s.chars() {
            self.write_char(z, x + dx, y + dy);
            dx += 1;
        }
    }

    pub fn write_char(&mut self, c: char, x: usize, y: usize) {
        let final_x = x * vga_char_width as usize;
        let final_y = y * vga_char_height as usize;

        let letter_idx = c as usize;

        let mask: [u8; 8] = [128, 64, 32, 16, 8, 4, 2, 1];

        for cy in 0..16 as usize {
            for cx in 0..8 as usize {
                let d = vga_font_data[letter_idx*16 + cy] & mask[cx];

                if d == 0 {
                    self.draw_pixel(cx + final_x, cy + final_y, 0xFF, 0xFF, 0xFF);
                } else {
                    self.draw_pixel(cx + final_x, cy + final_y, 8, 153, 46);
                }
            }
        }
    }
}
