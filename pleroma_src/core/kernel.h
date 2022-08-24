#pragma once

#include <map>
#include <string>
#include "../hylic_ast.h"

extern std::map<std::string, AstNode *> kernel_map;

void load_kernel();
AstNode *monad_start_program(EvalContext *context, EntityRefNode *eref);
