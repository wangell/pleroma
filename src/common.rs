cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        pub use std::collections::HashMap;
        pub use std::collections::BTreeMap;
        pub use std::fmt::{Debug, Error, Formatter};

        pub use String;
        pub use Box;
        pub use Vec;
        pub use std::vec;
        pub use std::str;
        pub use std::str::FromStr;
        pub use std::string::ToString;
        pub use std::sync::Arc;
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "native")] {
        extern crate alloc;

        pub use alloc::collections::BTreeMap as HashMap;
        pub use alloc::collections::BTreeMap;
        pub use alloc::string::String;
        pub use alloc::vec::Vec;
        pub use alloc::boxed::Box;
        pub use alloc::vec;
        pub use alloc::str;
        pub use alloc::str::FromStr;
        pub use alloc::string::ToString;
        pub use alloc::sync::Arc;

        pub fn ceil(value: f64) -> f64 {
            let integer_part = value as i64;
            let fractional_part = value - integer_part as f64;

            if fractional_part > 0.0 {
                integer_part as f64 + 1.0
            } else {
                integer_part as f64
            }
        }
    }
}
