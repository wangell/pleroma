#pragma once

#include <string>
#include <map>
#include <vector>

enum class AstNodeType {
  Stmt,
  Expr,
  Nop,

  TableNode,
  TableAccess,

  CreateEntity,

  EntityDef,

  WhileStmt,
  ForStmt,
  FallthroughExpr,
  ModuleStmt,
  FuncStmt,
  AssignmentStmt,
  SymbolNode,
  NumberNode,
  StringNode,
  OperatorExpr,
  BooleanExpr,
  FuncCall,
  ReturnNode,
  BooleanNode,
  MatchNode
};

struct PType {
  std::string type;
  enum Dist {
    Local,
    Far,
    Alien
  } distance;
};

struct AstNode {
  AstNodeType type;
  AstNode* parent;

  PType ptype;
};

struct Nop : AstNode {
};

struct ReturnNode : AstNode {
  AstNode* expr;
};

struct Scope {
  Scope *parent = nullptr;
  std::map<std::string, AstNode *> table;
};

extern Scope global_scope;

struct TableNode : AstNode {
  std::map<std::string, AstNode*> table;
};

struct ModuleStmt : AstNode {
  std::string module;
  bool namespaced;
};

struct FuncStmt : AstNode {
  std::string name;
  std::vector<std::string> args;
  std::vector<AstNode*> body;
};

struct AssignmentStmt : AstNode {
  std::string sym;
  AstNode* value;
};

struct ForStmt : AstNode {
  std::string sym;
  AstNode* generator;
  std::vector<AstNode*> body;
};

struct WhileStmt : AstNode {
  AstNode* generator;
  std::vector<AstNode*> body;
};

struct TableAccess : AstNode {
  AstNode* table;
  std::string access_sym;
  bool breakthrough = false;
};

struct SymbolNode : AstNode {
  std::string sym;
};

struct NumberNode : AstNode {
  int64_t value;
};

struct StringNode : AstNode {
  std::string value;
};

struct BooleanNode : AstNode {
  bool value;
};

struct FuncCall : AstNode {
  SymbolNode* sym;
  std::vector<AstNode*> args;
};

struct MatchNode : AstNode {
  AstNode* match_expr;
  std::vector<std::tuple<AstNode*, std::vector<AstNode*>>> cases;
};

struct OperatorExpr : AstNode {
  enum Op {Plus, Minus, Times, Divide} op;
  AstNode* term1;
  AstNode* term2;
};

struct FallthroughExpr : AstNode {
};

struct BooleanExpr : AstNode {
  enum Op { GreaterThan, LessThan, Equals, GreaterThanEqual, LessThanEqual } op;
  AstNode *term1;
  AstNode *term2;
};

struct EntityDef : AstNode {
  std::string name;
  std::map<std::string, FuncStmt*> functions;
  std::map<std::string, AstNode *> data;
};

struct CreateEntityNode : AstNode {
};

AstNode *make_for(std::string sym, AstNode *gen, std::vector<AstNode *> body);
AstNode *make_return(AstNode *a);
AstNode *make_operator_expr(OperatorExpr::Op op, AstNode *expr1, AstNode *expr2);
AstNode *make_fallthrough();
AstNode *make_boolean_expr(BooleanExpr::Op op, AstNode *expr1, AstNode *expr2);
AstNode *make_assignment(std::string sym, AstNode *expr);
AstNode *make_match(AstNode *match_expr, std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> cases);
AstNode *make_table_access(AstNode *table, std::string sym, bool breakthrough);
AstNode *make_while(AstNode *generator, std::vector<AstNode *> body);
AstNode *make_module_stmt(std::string s, bool namespaced);
AstNode *make_table(std::map<std::string, AstNode *> vals);
AstNode *make_number(int64_t v);
AstNode *make_string(std::string s);
AstNode *make_boolean(bool b);
AstNode *make_symbol(std::string s);
AstNode *make_actor(std::string s, std::map<std::string, FuncStmt *> functions, std::map<std::string, AstNode *> data);
AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode *> body);
AstNode *make_nop();
AstNode *make_function_call(AstNode *sym, std::vector<AstNode *> args);
AstNode *make_create_entity(EntityDef *actor_def);
