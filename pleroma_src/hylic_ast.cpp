#include "hylic_ast.h"
#include "hylic_eval.h"
#include <cassert>

AstNode *static_nop;
AstNode *static_true;
AstNode *static_false;

std::string ast_type_to_string(AstNodeType t) {
  switch (t) {
  case AstNodeType::MessageNode:
    return "MessageNode";
    break;
  case AstNodeType::NamespaceAccess:
    return "NamespaceAccess";
    break;
  case AstNodeType::PromiseNode:
    return "PromiseNode";
    break;
  case AstNodeType::EntityDef:
    return "EntityDef";
    break;
  case AstNodeType::FuncStmt:
    return "FuncStmt";
    break;
  case AstNodeType::ForeignFunc:
    return "ForeignFunc";
    break;
  case AstNodeType::ListNode:
    return "ListNode";
    break;
  case AstNodeType::SymbolNode:
    return "SymbolNode";
    break;
  case AstNodeType::StringNode:
    return "StringNode";
    break;
  case AstNodeType::CharacterNode:
    return "CharacterNode";
    break;
  case AstNodeType::TupleNode:
    return "TupleNode";
    break;
  case AstNodeType::OperatorExpr:
    return "OperatorExpr";
    break;
  case AstNodeType::BooleanExpr:
    return "BooleanExpr";
    break;
  case AstNodeType::NumberNode:
    return "NumberNode";
    break;
  case AstNodeType::CreateEntity:
    return "CreateEntity";
    break;
  case AstNodeType::ModUseNode:
    return "ModUseNode";
    break;
  }

  printf("Failed to convert AstNode type to string: %d\n", t);
  assert(false);
}

AstNode *make_for(std::string sym, AstNode *gen, std::vector<AstNode *> body) {
  ForStmt *node = new ForStmt;
  node->type = AstNodeType::ForStmt;
  node->generator = gen;
  node->body = body;
  node->sym = sym;
  return node;
}

AstNode *make_return(AstNode *a) {
  ReturnNode *r = new ReturnNode;
  r->type = AstNodeType::ReturnNode;
  r->expr = a;
  return r;
}

AstNode *make_operator_expr(OperatorExpr::Op op, AstNode *expr1,
                            AstNode *expr2) {
  OperatorExpr *exp = new OperatorExpr;
  exp->type = AstNodeType::OperatorExpr;
  exp->op = op;
  exp->term1 = expr1;
  exp->term2 = expr2;
  return exp;
}

AstNode *make_fallthrough() {
  FallthroughExpr *exp = new FallthroughExpr;
  exp->type = AstNodeType::FallthroughExpr;
  return exp;
}

AstNode *make_boolean_expr(BooleanExpr::Op op, AstNode *expr1, AstNode *expr2) {
  BooleanExpr *exp = new BooleanExpr;
  exp->type = AstNodeType::BooleanExpr;
  exp->op = op;
  exp->term1 = expr1;
  exp->term2 = expr2;
  return exp;
}

AstNode *make_assignment(SymbolNode* sym, AstNode *expr) {
  AssignmentStmt *assignment_stmt = new AssignmentStmt;
  assignment_stmt->type = AstNodeType::AssignmentStmt;
  assignment_stmt->sym = sym;
  assignment_stmt->value = expr;
  return assignment_stmt;
}

AstNode *
make_match(AstNode *match_expr,
           std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> cases) {
  MatchNode *node = new MatchNode;
  node->type = AstNodeType::MatchNode;
  node->match_expr = match_expr;
  node->cases = cases;
  return node;
}

AstNode *make_namespace_access(AstNode* ref, AstNode* field) {
  NamespaceAccess *node = new NamespaceAccess;
  node->type = AstNodeType::NamespaceAccess;
  node->ref = ref;
  node->field = field;
  // node->access_sym = sym;
  return node;
}

AstNode *make_mod_use(std::string mod_name, AstNode* accessor) {
  ModUseNode *node = new ModUseNode;
  node->type = AstNodeType::ModUseNode;
  node->mod_name = mod_name;
  node->accessor = accessor;
  return node;
}

AstNode *make_while(AstNode *generator, std::vector<AstNode *> body) {
  WhileStmt *while_stmt = new WhileStmt;

  while_stmt->type = AstNodeType::WhileStmt;
  while_stmt->generator = generator;
  while_stmt->body = body;

  return while_stmt;
}

AstNode *make_module_stmt(std::string module_name, bool namespaced, std::map<std::string, AstNode*> symbol_table) {
  ModuleStmt *mod_stmt = new ModuleStmt;
  mod_stmt->type = AstNodeType::ModuleStmt;
  mod_stmt->module = symbol_table;
  mod_stmt->module_name = module_name;
  mod_stmt->namespaced = namespaced;
  return mod_stmt;
}

AstNode *make_table(std::map<std::string, AstNode *> vals) {
  TableNode *table = new TableNode;
  table->type = AstNodeType::TableNode;
  table->table = vals;
  return table;
}

AstNode *make_number(int64_t v) {
  NumberNode *symbol_node = new NumberNode;
  symbol_node->type = AstNodeType::NumberNode;
  symbol_node->value_type = ValueType::Number;
  symbol_node->ctype.basetype = PType::u8;
  // symbol_node->value = strtol(s.c_str(), nullptr, 10);
  symbol_node->value = v;
  return symbol_node;
}

AstNode *make_string(std::string s) {
  StringNode *node = new StringNode;
  node->type = AstNodeType::StringNode;
  node->ctype.basetype = PType::str;
  node->value = s;
  return node;
}

AstNode *make_entity_creation() {}

AstNode *make_boolean(bool b) {
  if (!static_true || !static_false) {
    BooleanNode *true_node = new BooleanNode;
    true_node->type = AstNodeType::BooleanNode;
    true_node->value = true;
    static_true = true_node;

    BooleanNode *false_node = new BooleanNode;
    false_node->type = AstNodeType::BooleanNode;
    false_node->value = false;
    static_false = false_node;
  }

  if (b) {
    return static_true;
  } else {
    return static_false;
  }
}

AstNode *make_symbol(std::string s) {
  SymbolNode *symbol_node = new SymbolNode;
  symbol_node->type = AstNodeType::SymbolNode;
  symbol_node->sym = s;
  return symbol_node;
}

AstNode *make_actor(HylicModule* module, std::string s, std::map<std::string, FuncStmt *> functions,
                    std::map<std::string, AstNode *> data, std::vector<InoCap> inocaps) {
  EntityDef *actor_def = new EntityDef;
  actor_def->type = AstNodeType::EntityDef;
  actor_def->name = s;
  actor_def->module = module;
  actor_def->functions = functions;
  actor_def->data = data;
  actor_def->inocaps = inocaps;
  return actor_def;
}

AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode *> body, std::vector<CType*> param_types) {
  FuncStmt *func_stmt = new FuncStmt;
  func_stmt->type = AstNodeType::FuncStmt;
  func_stmt->name = s;
  func_stmt->args = args;
  func_stmt->param_types = param_types;
  func_stmt->body = body;
  return func_stmt;
}

AstNode *make_nop() {
  if (!static_nop) {
    auto nop = new Nop;
    nop->type = AstNodeType::Nop;
    static_nop = nop;
  }
  return static_nop;
}

AstNode *make_undefined() {
  AstNode* und = new AstNode;
  assert(false);
}

AstNode *make_message_node(AstNode* entity_ref, std::string function_name, CommMode comm_mode, std::vector<AstNode *> args) {
  MessageNode *func_call = new MessageNode;
  func_call->type = AstNodeType::MessageNode;
  func_call->entity_ref = entity_ref;
  func_call->function_name = function_name;
  func_call->comm_mode = comm_mode;
  func_call->args = args;

  return func_call;
}

AstNode *make_create_entity(std::string entity_def_name, bool new_vat) {
  CreateEntityNode *entity_node = new CreateEntityNode;
  entity_node->type = AstNodeType::CreateEntity;
  entity_node->entity_def_name = entity_def_name;
  entity_node->ctype.basetype = PType::Entity;
  entity_node->ctype.entity_name = entity_def_name;
  entity_node->new_vat = new_vat;

  return entity_node;
}

AstNode *make_entity_ref(int node_id, int vat_id, int entity_id) {
  EntityRefNode *entity_ref = new EntityRefNode;

  entity_ref->type = AstNodeType::EntityRefNode;
  entity_ref->entity_id = entity_id;
  entity_ref->vat_id = vat_id;
  entity_ref->node_id = node_id;

  return entity_ref;
}

AstNode *make_list(std::vector<AstNode *> list, CType *ctype) {
  ListNode *list_node = new ListNode;
  list_node->type = AstNodeType::ListNode;
  list_node->list = list;
  list_node->ctype.basetype = PType::List;
  list_node->ctype.subtype = ctype;

  return list_node;
}

AstNode *make_promise_node(int promise_id) {
  PromiseNode *promise_node = new PromiseNode;
  promise_node->type = AstNodeType::PromiseNode;
  promise_node->promise_id = promise_id;

  return promise_node;
}

AstNode *make_promise_resolution_node(std::string sym,
                                      std::vector<AstNode *> body) {
  PromiseResNode *promise_res_node = new PromiseResNode;
  promise_res_node->type = AstNodeType::PromiseResNode;
  promise_res_node->body = body;
  promise_res_node->sym = sym;

  return promise_res_node;
}

AstNode *make_foreign_func_call(AstNode *(*foreign_func)(EvalContext* context, std::vector<AstNode *>), std::vector<AstNode*> args) {
  ForeignFuncCall *ffc = new ForeignFuncCall;
  ffc->type = AstNodeType::ForeignFunc;
  ffc->foreign_func = foreign_func;
  ffc->args = args;
  return ffc;
}

void print_ast(AstNode *node, int indent_level) {
  for (int k = 0; k < indent_level; ++k) {
    printf("\t");
  }
  printf("%s\n", ast_type_to_string(node->type).c_str());

  if (node->type == AstNodeType::EntityDef) {
    for (auto &[k, v] : ((EntityDef *)node)->functions) {
      print_ast(v, indent_level + 1);
    }
  }
}

void print_ast_block(std::vector<AstNode *> block) {
  for (auto node : block) {
    print_ast(node);
  }
}

CType *clone_ctype(CType *ctype) {
  CType *cloned = new CType;
  cloned->basetype = ctype->basetype;
  cloned->subtype = clone_ctype(ctype);
  return cloned;
}

std::string ctype_to_string(CType *ctype) {
  switch (ctype->basetype) {
  case PType::u8:
    return "u8";
    break;
  case PType::u16:
    return "u16";
    break;
  case PType::u32:
    return "u32";
    break;
  case PType::str:
    return "str";
    break;
  case PType::NotAssigned:
    return "n/a";
    break;
  case PType::Entity:
    assert(!ctype->entity_name.empty());
    return ctype->entity_name;
    break;
  case PType::List:
    if (ctype->subtype) {
      return "[" + ctype_to_string(ctype->subtype) + "]";
    } else {
      return "[]";
    }
    break;
  }

  printf("Failed to catch type %d\n", ctype->basetype);
  assert(false);
}
