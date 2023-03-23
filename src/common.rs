cfg_if::cfg_if! {
    if #[cfg(feature = "hosted")] {
        pub use std::collections::HashMap;
        pub use std::fmt::{Debug, Error, Formatter};

        pub use String;
        pub use Box;
        pub use Vec;
        pub use std::vec;
        pub use std::str;
        pub use std::str::FromStr;
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "native")] {
        extern crate alloc;

        pub use alloc::collections::BTreeMap as HashMap;
        pub use alloc::string::String;
        pub use alloc::vec::Vec;
        pub use alloc::boxed::Box;
        pub use alloc::vec;
        pub use alloc::str;
        pub use alloc::str::FromStr;
    }
}
