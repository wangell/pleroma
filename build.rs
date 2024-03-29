#[cfg(feature = "native")]
use std::{env, error::Error};

#[cfg(feature = "native")]
fn main() -> Result<(), Box<dyn Error + Send + Sync>> {
		extern crate lalrpop;

    lalrpop::process_root().unwrap();

    // Get the name of the package.
    let kernel_name = env::var("CARGO_PKG_NAME")?;

    // Tell rustc to pass the linker script to the linker.
    println!("cargo:rustc-link-arg-bin={kernel_name}=--script=conf/linker.ld");

    // Have cargo rerun this script if the linker script or CARGO_PKG_ENV changes.
    println!("cargo:rerun-if-changed=conf/linker.ld");
    println!("cargo:rerun-if-env-changed=CARGO_PKG_NAME");

    Ok(())
}

#[cfg(feature = "hosted")]
fn main() {
		extern crate lalrpop;

    lalrpop::process_root().unwrap();
}
