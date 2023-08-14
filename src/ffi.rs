use core::fmt::Debug;
use core::any::Any;

pub trait BoundEntity: Send + Debug {
    fn as_any(&self) -> &dyn Any;
}
