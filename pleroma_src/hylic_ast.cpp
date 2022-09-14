#include "hylic_ast.h"
#include "general_util.h"
#include "hylic_eval.h"
#include <cassert>
#include <string>

AstNode *static_nop;
AstNode *static_true;
AstNode *static_false;

std::string entity_ref_str(EntityRefNode *ref) {
  return "(" + std::to_string(ref->node_id) + ", " +
         std::to_string(ref->vat_id) + ", " +
         std::to_string(ref->entity_id) + ")";
}

std::string ast_type_to_string(AstNodeType t) {
  switch (t) {
  case AstNodeType::WhileStmt: return "WhileStmt";
  case AstNodeType::IndexNode: return "IndexNode";
  case AstNodeType::BooleanNode: return "BooleanNode";
  case AstNodeType::MatchNode: return "MatchNode";
  case AstNodeType::ForStmt: return "ForStmt";
  case AstNodeType::EntityRefNode: return "EntityRefNode";
  case AstNodeType::MessageNode: return "MessageNode";
  case AstNodeType::NamespaceAccess: return "NamespaceAccess";
  case AstNodeType::PromiseNode: return "PromiseNode";
  case AstNodeType::EntityDef: return "EntityDef";
  case AstNodeType::FuncStmt: return "FuncStmt";
  case AstNodeType::ForeignFunc: return "ForeignFunc";
  case AstNodeType::ListNode: return "ListNode";
  case AstNodeType::SymbolNode: return "SymbolNode";
  case AstNodeType::StringNode: return "StringNode";
  case AstNodeType::CharacterNode: return "CharacterNode";
  case AstNodeType::TupleNode: return "TupleNode";
  case AstNodeType::OperatorExpr: return "OperatorExpr";
  case AstNodeType::BooleanExpr: return "BooleanExpr";
  case AstNodeType::NumberNode: return "NumberNode";
  case AstNodeType::CreateEntity: return "CreateEntity";
  case AstNodeType::ModUseNode: return "ModUseNode";
  case AstNodeType::AssignmentStmt: return "AssignmentStmt";
  case AstNodeType::ReturnNode: return "ReturnNode";
  case AstNodeType::PromiseResNode: return "PromiseResNode";
  case AstNodeType::SelfNode: return "SelfNode";
  case AstNodeType::CommentNode: return "CommentNode";
  }
  printf("Failed to convert AstNode type to string: %d\n", t);
  assert(false);
}

AstNode *make_comment(std::string comment) {
  CommentNode *node = new CommentNode;
  node->type = AstNodeType::CommentNode;
  node->comment = comment;
  return node;
}

AstNode *make_self() {
  SelfNode *node = new SelfNode;
  node->type = AstNodeType::SelfNode;
  return node;
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

AstNode *make_assignment(AstNode* sym, AstNode *expr) {
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
  symbol_node->ctype.dtype = DType::Local;
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

AstNode *make_actor(HylicModule* module, std::string s, std::map<std::string, FuncStmt *> functions, std::map<std::string, AstNode *> data, std::vector<InoCap> inocaps, std::vector<std::string> preamble, std::vector<std::string> postamble) {
  EntityDef *actor_def = new EntityDef;
  actor_def->type = AstNodeType::EntityDef;
  actor_def->name = s;
  actor_def->module = module;
  actor_def->functions = functions;
  actor_def->data = data;
  actor_def->inocaps = inocaps;
  actor_def->preamble = preamble;
  actor_def->postamble = postamble;
  return actor_def;
}

AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode *> body, std::vector<CType*> param_types, bool pure) {
  FuncStmt *func_stmt = new FuncStmt;
  func_stmt->type = AstNodeType::FuncStmt;
  func_stmt->name = s;
  func_stmt->args = args;
  func_stmt->param_types = param_types;
  func_stmt->body = body;
  func_stmt->pure = pure;
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
  entity_node->new_vat = new_vat;

  // If it's a new vat, return @far Ent, otherwise loc Ent
  if (new_vat) {
    entity_node->ctype.basetype = PType::Promise;
    entity_node->ctype.dtype = DType::Local;
    entity_node->ctype.subtype = new CType;
    entity_node->ctype.subtype->basetype = PType::Entity;
    entity_node->ctype.subtype->entity_name = entity_def_name;
    entity_node->ctype.subtype->dtype = DType::Far;
  } else {
    entity_node->ctype.basetype = PType::Entity;
    entity_node->ctype.entity_name = entity_def_name;
    entity_node->ctype.dtype = DType::Local;
  }

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

AstNode *make_foreign_func_call(AstNode *(*foreign_func)(EvalContext* context, std::vector<AstNode *>), std::vector<AstNode*> args, CType ret_type) {
  ForeignFuncCall *ffc = new ForeignFuncCall;
  ffc->type = AstNodeType::ForeignFunc;
  ffc->foreign_func = foreign_func;
  ffc->args = args;
  ffc->ctype = ret_type;
  return ffc;
}

AstNode *make_index_node(AstNode *list, AstNode *accessor) {
  IndexNode *index_node = new IndexNode;
  index_node->type = AstNodeType::IndexNode;
  index_node->list = list;
  index_node->accessor = accessor;
  return index_node;
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
  std::string dstring;
  if (ctype->dtype == DType::Local) {
    dstring = "loc";
  } else if (ctype->dtype == DType::Far) {
    dstring = "far";
  } else {
    dstring = "aln";
  }

  switch (ctype->basetype) {
  case PType::None:
    return dstring + " None";
    break;
  case PType::u8:
    return dstring + " u8";
    break;
  case PType::u16:
    return dstring + " u16";
    break;
  case PType::u32:
    return dstring + " u32";
    break;
  case PType::str:
    return dstring + " str";
    break;
  case PType::BaseEntity:
    return dstring + " Entity";
    break;
  case PType::NotAssigned:
    return "n/a";
    break;
  case PType::Entity:
    assert(!ctype->entity_name.empty());
    return dstring + " " + ctype->entity_name;
    break;
  case PType::Promise:
    if (ctype->subtype) {
      return dstring + "@" + ctype_to_string(ctype->subtype);
    }
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

void print_ctype(CType *ctype) {
  printf("%s\n", ctype_to_string(ctype).c_str());
}

void print_ctype(CType ctype) {
  printf("%s\n", ctype_to_string(&ctype).c_str());
}

std::string extract_string(AstNode *node) {
  assert(node->type == AstNodeType::StringNode);

  return ((StringNode*) node)->value;
}

std::string stringify_value_node(AstNode *node) {
  switch(node->type) {
  case AstNodeType::StringNode: return ((StringNode*)node)->value;
  }

  return "";
}
