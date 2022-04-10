#pragma once

#include <vector>
#include <string>

#include "../hylic_ast.h"
#include "../hylic_eval.h"

FuncStmt *setup_direct_call(AstNode *(*foreign_func)(EvalContext *,
                                                     std::vector<AstNode *>),
                            std::string name, std::vector<std::string> args,
                            std::vector<CType *> arg_types, CType ctype);
