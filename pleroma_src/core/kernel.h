#pragma once

#include "../hylic_ast.h"
#include "../system.h"
#include <map>
#include <string>

extern std::map<SystemModule, std::map<std::string, AstNode *>> kernel_map;

void load_kernel();
AstNode *monad_start_program(EvalContext *context, EntityRefNode *eref);

void add_new_pnode(PleromaNode *node);

void load_software();
