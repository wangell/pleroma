#pragma once

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <list>
#include "hylic_tokenizer.h"

enum class AstNodeType {
  Stmt,
  Expr,
  Nop,

  TableNode,
  TableAccess,

  ActorDef,

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

struct ActorDef : AstNode {
  std::string name;
  std::map<std::string, FuncStmt*> functions;
  std::map<std::string, AstNode *> data;
};

struct Actor {
  std::map<int, std::vector<AstNode*>> functions;
};

extern std::stack<TokenStream> tokenstream_stack;

void load_file(std::string);
void parse(TokenStream stream);
AstNode *eval(AstNode *obj, Scope *scope);
void parse_file(std::string path);
