pub struct Nodeman<'a> {
    pub node: &'a mut Node,
    pub def: ast::EntityDef,
}

impl Nodeman<'_> {
    pub fn new(node: &mut Node) -> Nodeman {
        let mut n = Nodeman {
            node: node,
            def: ast::EntityDef {
                name: String::from("Nodeman"),
                data_declarations: Vec::new(),
                inoculation_list: Vec::new(),
                functions: HashMap::new(),
                foreign_functions: HashMap::new(),
            },
        };

        n
    }

    pub fn hello(entity: &mut vm_core::Entity, args: ast::Hvalue) -> ast::Hvalue {
        println!("\x1b[0;32mHello, welcome to Pleroma!\x1b[0m");
        entity.data.insert(String::from("Hi"), ast::Hvalue::Hu8(5));
        return ast::Hvalue::None;
    }

}

pub fn load_nodeman(nodeman: &Nodeman) {
    let contents = fs::read_to_string("./sys/io.plm")
        .expect("Should have been able to read the file");
    let mut module = parser::parse_module(contents.as_str());

    let mut nodedef = nodeman.def.clone();

    if let AstNode::Module(mut real_module) = module {
        let real_def = real_module.entity_defs.get_mut("Nodeman").unwrap();
        if let ast::AstNode::EntityDef(d) = real_def {
            d.register_foreign_function(&String::from("test"), Nodeman::hello);
        }

        let mut root = ast::Root{
            modules : BTreeMap::new(),
            external_modules: BTreeMap::new()
        };

        root.modules.insert(String::from("Nodeman"), AstNode::Module(real_module));

        compile::compile(&mut AstNode::Root(root), "io.plmb");
    }
}
