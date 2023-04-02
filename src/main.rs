#![cfg_attr(feature = "native", no_std)]
#![cfg_attr(feature = "native", no_main)]
#![cfg_attr(feature = "native", feature(abi_x86_interrupt))]
#![cfg_attr(feature = "native", feature(alloc_error_handler))] // at the top of the file
#![feature(naked_functions)]
#![feature(iter_advance_by)]
#![allow(warnings)]

#[cfg(feature = "native")]
#[macro_use]
pub mod io;


cfg_if::cfg_if! {
    if #[cfg(feature = "native")] {

        #[macro_use]
        extern crate lalrpop_util;

        pub mod interrupts;
        pub mod memory;
        pub mod palloc;
        pub mod native_util;
        pub mod native;

        pub mod multitasking;
        pub mod zeno;
        pub mod metanoid;
        pub mod bin_helpers;
        pub mod filesys;

        pub mod pci;
        pub mod architecture;
        pub mod initrd;
        pub mod drivers;

        pub mod framebuffer;
        pub mod net;
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        #[macro_use]
        extern crate lalrpop_util;

        pub mod pnet;
        pub mod hosted;
        pub mod kernel;
        pub mod compile;
    }
}

pub mod lexer;
pub mod parser;
pub mod sem;
pub mod codegen;
pub mod vm_core;
pub mod vm;
pub mod ast;
pub mod pbin;
pub mod opcodes;
pub mod common;

#[cfg(feature = "hosted")]
fn main() {
    hosted::boot();
}

#[cfg(feature = "native")]
#[no_mangle]
pub extern "C" fn _start() -> ! {
    native::boot();
}
