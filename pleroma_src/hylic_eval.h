#pragma once

#include <vector>
#include "hylic_ast.h"
#include <mutex>
#include <queue>
#include <string>

struct Entity {
  EntityDef *entity_def;
  std::map<std::string, AstNode *> data;
};

struct Msg {
  int entity_id;
  std::string function_name;
};

struct Promise {
  AstNode* callback;
};

struct Vat {
  int id = 0;
  int run_n = 0;

  std::mutex message_mtx;
  std::queue<Msg> messages;
  std::queue<Msg> out_messages;

  std::queue<Promise> promises;

  std::map<int, Entity *> entities;
};

struct Context {
};

AstNode *eval(Vat* vat, Entity* entity, AstNode *obj, Scope *scope = &global_scope);
Scope *find_symbol_scope(std::string sym, Scope *scope);
AstNode *find_symbol(std::string sym, Scope *scope);
Entity *create_entity(EntityDef *entity_def);
AstNode *eval_func_local(Vat *vat, Entity *entity, std::string function_name, std::vector<AstNode *> args);
