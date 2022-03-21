#include "hylic_ast.h"

AstNode *static_nop;
AstNode *static_true;
AstNode *static_false;

AstNode* make_for(std::string sym, AstNode* gen, std::vector<AstNode*> body) {
  ForStmt *node = new ForStmt;
  node->type = AstNodeType::ForStmt;
  node->generator = gen;
  node->body = body;
  node->sym = sym;
  return node;
}

AstNode* make_return(AstNode* a) {
  ReturnNode *r = new ReturnNode;
  r->type = AstNodeType::ReturnNode;
  r->expr = a;
  return r;
}

AstNode *make_operator_expr(OperatorExpr::Op op, AstNode* expr1, AstNode* expr2) {
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

AstNode *make_assignment(std::string sym, AstNode* expr) {
  AssignmentStmt *assignment_stmt = new AssignmentStmt;
  assignment_stmt->type = AstNodeType::AssignmentStmt;
  assignment_stmt->sym = sym;
  assignment_stmt->value = expr;
  return assignment_stmt;
}

AstNode* make_match(AstNode* match_expr, std::vector<std::tuple<AstNode*, std::vector<AstNode*>>> cases) {
  MatchNode* node = new MatchNode;
  node->type = AstNodeType::MatchNode;
  node->match_expr = match_expr;
  node->cases = cases;
  return node;
}

AstNode* make_table_access(AstNode* table, std::string sym, bool breakthrough) {
  TableAccess* node = new TableAccess;
  node->type = AstNodeType::TableAccess;
  node->table = table;
  node->access_sym = sym;
  node->breakthrough = breakthrough;
  return node;
}

AstNode* make_while(AstNode* generator, std::vector<AstNode*> body) {
  WhileStmt *while_stmt = new WhileStmt;

  while_stmt->type = AstNodeType::WhileStmt;
  while_stmt->generator = generator;
  while_stmt->body = body;

  return while_stmt;
}

AstNode* make_module_stmt(std::string s, bool namespaced) {
  ModuleStmt* mod_stmt = new ModuleStmt;
  mod_stmt->type = AstNodeType::ModuleStmt;
  mod_stmt->module = s;
  mod_stmt->namespaced = namespaced;
  return mod_stmt;
}

AstNode* make_table(std::map<std::string, AstNode*> vals) {
  TableNode* table = new TableNode;
  table->type = AstNodeType::TableNode;
  table->table = vals;
  return table;
}

AstNode *make_number(int64_t v) {
  NumberNode *symbol_node = new NumberNode;
  symbol_node->type = AstNodeType::NumberNode;
  //symbol_node->value = strtol(s.c_str(), nullptr, 10);
  symbol_node->value = v;
  return symbol_node;
}

AstNode* make_string(std::string s) {
  StringNode *node = new StringNode;
  node->type = AstNodeType::StringNode;
  node->value = s;
  return node;
}

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

AstNode *make_actor(std::string s, std::map<std::string, FuncStmt*> functions, std::map<std::string, AstNode*> data) {
  EntityDef *actor_def = new EntityDef;
  actor_def->type = AstNodeType::EntityDef;
  actor_def->name = s;
  actor_def->functions = functions;
  actor_def->data = data;
  return actor_def;
}

AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode*> body) {
  FuncStmt *func_stmt = new FuncStmt;
  func_stmt->type = AstNodeType::FuncStmt;
  func_stmt->name = s;
  func_stmt->args = args;
  func_stmt->body = body;
  return func_stmt;
}

AstNode* make_nop() {
  if (!static_nop) {
    auto nop = new Nop;
    nop->type = AstNodeType::Nop;
    static_nop = nop;
  }
  return static_nop;
}

AstNode *make_function_call(AstNode* sym, std::vector<AstNode*> args) {
  FuncCall *func_call = new FuncCall;
  func_call->type = AstNodeType::FuncCall;
  func_call->sym = (SymbolNode*)sym;
  func_call->args = args;
  return func_call;
}

AstNode *make_entity(EntityDef* actor_def) {
  EntityNode *entity_node = new EntityNode;

  return entity_node;
}
