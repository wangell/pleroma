#pragma once

#include "common.h"
#include "hylic_tokenizer.h"
#include <map>
#include <string>
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
  IndexNode,

  ForeignFunc,

  ModUseNode,

  UndefinedNode
};

enum class MessageDistance { Local, Far, Alien };

enum class CommMode { Sync, Async };

enum class DType { Local, Far, Alien };

enum class PType {
  NotAssigned,

  None,
  Nil,

  u8,
  u16,
  u32,
  u64,

  str,

  boolean,

  List,
  UserType,
  Promise,
  Entity
};

struct CType {
  PType basetype;

  DType dtype;
  CType* subtype;
  std::string entity_name;

  std::list<Token *>::iterator start;
  std::list<Token *>::iterator end;
};

struct AstNode {
  AstNodeType type;
  AstNode *parent;

  CType ctype;

  std::list<Token *>::iterator start;
  std::list<Token *>::iterator end;
};

struct InoCap {
  std::string var_name;
  CType *ctype;
};

struct HylicModule {
  std::map<std::string, HylicModule *> imports;
  std::map<std::string, AstNode *> entity_defs;
};

struct Nop : AstNode {};

struct ReturnNode : AstNode {
  AstNode *expr;
};

struct ModUseNode: AstNode {
  std::string mod_name;
  AstNode* accessor;
};

struct TableNode : AstNode {
  std::map<std::string, AstNode *> table;
};

struct ModuleStmt : AstNode {
  std::string module_name;
  bool namespaced;
  std::map<std::string, AstNode *> module;
};

struct FuncStmt : AstNode {
  std::string name;

  std::vector<CType *> param_types;
  std::vector<std::string> args;

  std::vector<AstNode *> body;
  bool pure;
};

struct ForStmt : AstNode {
  std::string sym;
  AstNode *generator;
  std::vector<AstNode *> body;
};

struct WhileStmt : AstNode {
  AstNode *generator;
  std::vector<AstNode *> body;
};

struct NamespaceAccess : AstNode {
  AstNode *ref;
  AstNode *field;
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
  std::vector<AstNode *> list;
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
  std::vector<AstNode *> value;
};

struct UsertypeValueNode : ValueNode {
  std::map<std::string, ValueNode *> table;
};

struct PromiseResNode : AstNode {
  std::string sym;
  std::vector<AstNode *> body;
};

struct MessageNode : AstNode {
  AstNode* entity_ref;
  std::string function_name;

  MessageDistance message_distance;
  CommMode comm_mode;

  std::vector<AstNode *> args;
};

struct MatchNode : AstNode {
  AstNode *match_expr;
  std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> cases;
};

struct OperatorExpr : AstNode {
  enum Op { Plus, Minus, Times, Divide } op;
  AstNode *term1;
  AstNode *term2;
};

struct FallthroughExpr : AstNode {};

struct BooleanExpr : AstNode {
  enum Op { GreaterThan, LessThan, Equals, GreaterThanEqual, LessThanEqual } op;
  AstNode *term1;
  AstNode *term2;
};

struct EntityDef : AstNode {
  std::string name;
  HylicModule* module;
  std::map<std::string, FuncStmt *> functions;
  std::map<std::string, AstNode *> data;
  std::vector<InoCap> inocaps;
};

struct CreateEntityNode : AstNode {
  std::string entity_def_name;
  bool new_vat = false;
};

struct AssignmentStmt : AstNode {
  AstNode *sym;
  AstNode *value;
};

struct IndexNode : AstNode {
  AstNode *list;
  AstNode *accessor;
};

struct EvalContext;

struct ForeignFuncCall : AstNode {
  AstNode *(*foreign_func)(EvalContext*, std::vector<AstNode *>);
  std::vector<AstNode *> args;
};

AstNode *make_for(std::string sym, AstNode *gen, std::vector<AstNode *> body);
AstNode *make_return(AstNode *a);
AstNode *make_operator_expr(OperatorExpr::Op op, AstNode *expr1,
                            AstNode *expr2);
AstNode *make_fallthrough();
AstNode *make_boolean_expr(BooleanExpr::Op op, AstNode *expr1, AstNode *expr2);
AstNode *make_assignment(AstNode *sym, AstNode *expr);
AstNode *make_match(AstNode *match_expr, std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> cases);
AstNode *make_namespace_access(AstNode* ref, AstNode* field);
AstNode *make_while(AstNode *generator, std::vector<AstNode *> body);
AstNode *make_module_stmt(std::string s, bool namespaced, std::map<std::string, AstNode*> symbol_table);
AstNode *make_table(std::map<std::string, AstNode *> vals);
AstNode *make_number(int64_t v);
AstNode *make_string(std::string s);
AstNode *make_boolean(bool b);
AstNode *make_symbol(std::string s);
AstNode *make_actor(HylicModule* hm, std::string s, std::map<std::string, FuncStmt *> functions, std::map<std::string, AstNode *> data, std::vector<InoCap> inocaps);
AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode *> body, std::vector<CType *> param_types, bool pure);
AstNode *make_nop();
AstNode *make_undefined();
AstNode *make_message_node(AstNode* entity_ref, std::string function_name, CommMode comm_mode, std::vector<AstNode *> args);
AstNode *make_create_entity(std::string entity_name, bool new_vat);
AstNode *make_entity_ref(int node_id, int vat_id, int entity_id);
AstNode *make_list(std::vector<AstNode *> list, CType* ctype);
AstNode *make_promise_node(int promise_id);
AstNode *make_mod_use(std::string mod_name, AstNode* accessor);
AstNode *make_index_node(AstNode* list, AstNode* accessor);
// HACK Make this an AstNode and then eval + check in symbol table
AstNode *make_promise_resolution_node(std::string sym, std::vector<AstNode *> body);

AstNode *make_foreign_func_call(AstNode* (*foreign_func)(EvalContext*, std::vector<AstNode*>), std::vector<AstNode*> args);

std::string ast_type_to_string(AstNodeType t);
std::string ctype_to_string(CType *ctype);

void print_ast(AstNode *node, int indent_level = 0);
void print_ast_block(std::vector<AstNode *> block);

CType *clone_ctype(CType* ctype);

void print_ctype(CType *ctype);
void print_ctype(CType ctype);
