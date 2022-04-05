#include "hylic_ast.h"
#include <cassert>

AstNode *static_nop;
AstNode *static_true;
AstNode *static_false;

std::string ast_type_to_string(AstNodeType t) {
  switch (t) {
  case AstNodeType::PromiseNode:
    return "PromiseNode";
    break;
  case AstNodeType::EntityDef:
    return "EntityDef";
    break;
  case AstNodeType::FuncStmt:
    return "FuncStmt";
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

AstNode *make_namespace_access(std::string namespace_table, AstNode *accessor) {
  NamespaceAccess *node = new NamespaceAccess;
  node->type = AstNodeType::NamespaceAccess;
  node->namespace_table = namespace_table;
  node->accessor = accessor;
  // node->access_sym = sym;
  return node;
}

AstNode *make_while(AstNode *generator, std::vector<AstNode *> body) {
  WhileStmt *while_stmt = new WhileStmt;

  while_stmt->type = AstNodeType::WhileStmt;
  while_stmt->generator = generator;
  while_stmt->body = body;

  return while_stmt;
}

AstNode *make_module_stmt(std::string s, bool namespaced) {
  ModuleStmt *mod_stmt = new ModuleStmt;
  mod_stmt->type = AstNodeType::ModuleStmt;
  mod_stmt->module = s;
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
  symbol_node->ptype = PType::u8;
  // symbol_node->value = strtol(s.c_str(), nullptr, 10);
  symbol_node->value = v;
  return symbol_node;
}

AstNode *make_string(std::string s) {
  StringNode *node = new StringNode;
  node->type = AstNodeType::StringNode;
  node->ptype = PType::str;
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

AstNode *make_actor(std::string s, std::map<std::string, FuncStmt *> functions,
                    std::map<std::string, AstNode *> data) {
  EntityDef *actor_def = new EntityDef;
  actor_def->type = AstNodeType::EntityDef;
  actor_def->name = s;
  actor_def->functions = functions;
  actor_def->data = data;
  return actor_def;
}

AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode *> body, std::vector<PType*> param_types) {
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

AstNode *make_message_node(std::string entity_ref_name,
                           std::string function_name, MessageDistance dist,
                           CommMode comm_mode, std::vector<AstNode *> args) {
  MessageNode *func_call = new MessageNode;
  func_call->type = AstNodeType::MessageNode;
  func_call->function_name = function_name;
  func_call->entity_ref_name = entity_ref_name;
  func_call->message_distance = dist;
  func_call->comm_mode = comm_mode;
  func_call->args = args;
  return func_call;
}

AstNode *make_create_entity(std::string entity_def_name, bool new_vat) {
  CreateEntityNode *entity_node = new CreateEntityNode;
  entity_node->type = AstNodeType::CreateEntity;
  entity_node->entity_def_name = entity_def_name;

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

AstNode *make_list(std::vector<AstNode *> list) {
  ListNode *list_node = new ListNode;
  list_node->type = AstNodeType::ListNode;
  list_node->list = list;

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
