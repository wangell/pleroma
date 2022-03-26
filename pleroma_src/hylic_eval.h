#pragma once

#include "hylic_ast.h"

struct Entity {
  EntityDef *entity_def;
  std::map<std::string, AstNode *> data;
};

AstNode *eval(AstNode *obj, Scope *scope = &global_scope);
AstNode *eval_func(AstNode* obj, std::vector<AstNode *> args);
Scope *find_symbol_scope(std::string sym, Scope * scope);
AstNode *find_symbol(std::string sym, Scope * scope);

Entity* create_entity(EntityDef* entity_def);
