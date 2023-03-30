use core;
use x86_64::instructions::port::{PortGeneric, WriteOnlyAccess};
use core::arch::asm;

pub fn mem_fence() {
    core::sync::atomic::compiler_fence(core::sync::atomic::Ordering::Acquire);
    unsafe {
        asm!("mfence", options(nomem, nostack));
    }
}

pub fn shutdown() {
    unsafe {
        let mut p: PortGeneric<u16, WriteOnlyAccess> = PortGeneric::new(0x604);
        p.write(0x2000);
    }
}
