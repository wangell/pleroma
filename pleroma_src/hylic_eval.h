#pragma once

#include <vector>
#include "hylic_ast.h"
#include <mutex>
#include <queue>
#include <string>

struct EntityAddress {
  int node_id;
  int vat_id;
  int entity_id;
};

struct Entity {
  EntityDef *entity_def;
  EntityAddress address;
  std::map<std::string, AstNode *> data;

  std::map<std::string, AstNode*> file_scope;
};

struct Msg {
  int entity_id;
  int vat_id;
  int node_id;
  std::string function_name;
};

struct PromiseResult {
  bool resolved = false;
  int blah;
};

struct Vat {
  int id = 0;
  int run_n = 0;

  int promise_id_base = 0;

  std::mutex message_mtx;
  std::queue<Msg> messages;
  std::queue<Msg> out_messages;

  std::map<int, PromiseResult> promises;

  std::map<int, Entity *> entities;
};

struct Scope {
  Scope *parent = nullptr;
  std::map<std::string, AstNode *> table;
};

extern Scope global_scope;

struct EvalContext {
  Vat* vat;
  Entity* entity;
  Scope *scope;
};

AstNode *eval(EvalContext* context, AstNode *obj);
Scope *find_symbol_scope(std::string sym, Scope *scope);
AstNode *find_symbol(std::string sym, Scope *scope);
Entity *create_entity(EvalContext* context, EntityDef *entity_def, EntityAddress address);
AstNode *eval_func_local(EvalContext *context, Entity *entity, std::string function_name, std::vector<AstNode *> args);
