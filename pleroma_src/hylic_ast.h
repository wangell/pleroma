#pragma once

#include <string>
#include <map>
#include <vector>

enum class AstNodeType {
  Stmt,
  Expr,
  Nop,

  TableNode,
  NamespaceAccess,

  CreateEntity,

  EntityDef,

  EntityRefNode,

  WhileStmt,
  ForStmt,
  FallthroughExpr,
  ModuleStmt,
  FuncStmt,
  AssignmentStmt,

  PromiseResNode,

  SymbolNode,
  NumberNode,
  ListNode,
  StringNode,
  CharacterNode,
  PromiseNode,
  TupleNode,

  OperatorExpr,
  BooleanExpr,
  MessageNode,
  ReturnNode,
  BooleanNode,
  MatchNode,
};

enum class MessageDistance {
  Local,
  Far,
  Alien
};

enum class CommMode {
  Sync,
  Async
};

enum class PType {
  u8
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

struct NamespaceAccess : AstNode {
  std::string namespace_table;
  AstNode* accessor;
};

enum class ValueType {
  String,
  Number,
  Boolean,
  Character,
  User,
};

// Move all values to this over time
struct ValueNode : AstNode {
  ValueType value_type;
};

struct SymbolNode : AstNode {
  std::string sym;
};

struct NumberNode : ValueNode {
  int64_t value;
};

struct ListNode : ValueNode {
  std::vector<AstNode*> list;
};

struct StringNode : ValueNode {
  std::string value;
};

struct BooleanNode : ValueNode {
  bool value;
};

struct PromiseNode : ValueNode {
  int promise_id;
};

struct EntityRefNode : ValueNode {
  int node_id;
  int vat_id;
  int entity_id;
};

struct TupleNode : ValueNode {
  int tuple_size;
  std::vector<AstNode*> value;
};

struct UsertypeValueNode : ValueNode {
  std::map<std::string, ValueNode*> table;
};

struct PromiseResNode : AstNode {
  std::string sym;
  std::vector<AstNode*> body;
};

struct MessageNode : AstNode {
  std::string entity_ref_name;
  std::string function_name;

  MessageDistance message_distance;
  CommMode comm_mode;

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
  std::string entity_def_name;
};

AstNode *make_for(std::string sym, AstNode *gen, std::vector<AstNode *> body);
AstNode *make_return(AstNode *a);
AstNode *make_operator_expr(OperatorExpr::Op op, AstNode *expr1, AstNode *expr2);
AstNode *make_fallthrough();
AstNode *make_boolean_expr(BooleanExpr::Op op, AstNode *expr1, AstNode *expr2);
AstNode *make_assignment(std::string sym, AstNode *expr);
AstNode *make_match(AstNode *match_expr, std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> cases);
AstNode *make_namespace_access(std::string namespace_table, AstNode *accessor);
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
AstNode *make_message_node(std::string entity_ref_name, std::string function_name, MessageDistance dist, CommMode comm_mode, std::vector<AstNode *> args);
AstNode *make_create_entity(std::string entity_name, bool new_vat);
AstNode *make_entity_ref(int node_id, int vat_id, int entity_id);
AstNode *make_list(std::vector<AstNode *> list);
AstNode *make_promise_node(int promise_id);
// HACK Make this an AstNode and then eval + check in symbol table
AstNode *make_promise_resolution_node(std::string sym, std::vector<AstNode*> body);

std::string ast_type_to_string(AstNodeType t);
