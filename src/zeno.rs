use crate::ast;
use crate::common::HashMap;
use crate::common::Vec;
use crate::common::String;
use crate::filesys;
use crate::metanoid;

struct ZenoSupra {}

struct ZenoInfra {}

impl ZenoSupra {
    pub fn checkout(params: ast::AstNode) -> ast::Value {

        // Check queue for the ZenoFile, if nobody in queue proceed:
        // Can checkout read, write, append, read-write, etc.
        // If somoene in queue, check if current holder is read, write - multiple readers allowed or one writer, maybe a notification API (multiple readers, one writer with notifications)
        // Lock file in index
        // Using ZenoBacking, write/read to index file (+ cache) cluster locations
        // File -> [(node_id, [cluster_ids]), ...]

        ast::Value::None
    }
}

impl ZenoInfra {
    pub fn read_chunk(params: ast::AstNode) -> ast::Value {
        ast::Value::None
    }

    //pub fn export_functions() -> HashMap<String, fn(ast::AstNode) -> ast::AstNode> {
    //    HashMap::from([
    //        (String::from("checkout"), checkout)
    //    ])
    //}
}

struct ZenoFile {
}

trait ZenoBacking {
    fn read_chunk(chunk_id: u64) -> Result<Vec<u8>, String>;
    fn write_chunk(chunk_id: u64, data: Vec<u8>) -> Result<(), String>;
}

// Batched I/O commands
// write_byte/write - calculate location client side = chunks[byte / CHUNK_SIZE]
struct ZenoCommands {
}
