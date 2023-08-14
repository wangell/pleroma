use core::fmt::Debug;
use core::any::Any;
use crate::ast;

pub trait BoundEntity: Send + Debug {
    fn as_any(&mut self) -> &mut dyn Any;
}

//#[macro_export]
//macro_rules! nested_hashmap {
//    ($($k:expr => {$($sub_k:expr => ($sub_v:path, $sub_vec:expr)),*}),*) => {
//        {
//            let mut outer_map: HashMap<_, HashMap<_, (_, Vec<_>)>> = HashMap::new();
//            $(
//                let mut inner_map = HashMap::new();
//                $(
//                    inner_map.insert($sub_k.to_string(), ($sub_v as fn(), $sub_vec));
//                )*
//                outer_map.insert($k.to_string(), inner_map);
//            )*
//            outer_map
//        }
//    };
//}

#[macro_export]
macro_rules! nested_hashmap {
    ($($k:expr => {$($sub_k:expr => ($sub_v:path, $sub_vec:expr)),*}),*) => {
        {
            let mut outer_map: HashMap<String, HashMap<String, (ast::RawFF, Vec<(String, ast::CType)>)>> = HashMap::new();
            $(
                let mut inner_map = HashMap::new();
                $(
                    inner_map.insert($sub_k.to_string(), ($sub_v as ast::RawFF, $sub_vec));
                )*
                outer_map.insert($k.to_string(), inner_map);
            )*
            outer_map
        }
    };
}
