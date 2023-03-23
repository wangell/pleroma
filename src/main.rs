#![cfg_attr(feature = "native", no_std)]
#![cfg_attr(feature = "native", no_main)]
#![cfg_attr(feature = "native", feature(abi_x86_interrupt))]
#![cfg_attr(feature = "native", feature(alloc_error_handler))] // at the top of the file


#[macro_use]
#[cfg(feature = "hosted")]
extern crate lalrpop_util;

#[macro_use]
#[cfg(feature = "native")]
pub mod io;

#[cfg(feature = "native")]
pub mod interrupts;

#[cfg(feature = "native")]
pub mod native;

#[cfg(feature = "hosted")]
pub mod ast;

#[cfg(feature = "hosted")]
pub mod codegen;

#[cfg(feature = "hosted")]
pub mod compile;

#[cfg(feature = "hosted")]
pub mod kernel;

#[cfg(feature = "hosted")]
pub mod opcodes;

#[cfg(feature = "hosted")]
pub mod parser;

#[cfg(feature = "hosted")]
pub mod pbin;

#[cfg(feature = "hosted")]
pub mod pnet;

#[cfg(feature = "hosted")]
pub mod sem;

#[cfg(feature = "hosted")]
pub mod vm;

#[cfg(feature = "hosted")]
pub mod vm_core;

#[cfg(feature = "hosted")]
pub mod hosted;

#[cfg(feature = "hosted")]
fn main() {
    hosted::boot();
}

#[cfg(feature = "native")]
#[no_mangle]
pub extern "C" fn _start() -> ! {
    native::boot();
}
