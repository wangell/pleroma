use core::slice::{from_raw_parts, from_raw_parts_mut};
use crate::filesys::fat;

use limine::LimineModuleRequest;

static INITRD: LimineModuleRequest = LimineModuleRequest::new(0);

pub fn setup_initrd() {
    let modules = INITRD.get_response().get().unwrap().modules();

    println!("Initrd modules:");
    for module in modules {
        if let Some(path) = module.path.to_str() {
            println!("\t{:?}", path);

            if let Some(base_addr) = module.base.as_ptr() {
                let safe_slice;
                unsafe {
                    safe_slice = from_raw_parts_mut(base_addr, module.length as usize);
                }
                let fat_fs = fat::FatFs::new(safe_slice, fat::FatType::Fat16);
                let entries = fat_fs.list_root_dir();

                println!("Root dir {}:", path.to_str().unwrap());
                for i in entries {
                    println!("\t{}", i.filename());
                }
            }
        }
    }
}
