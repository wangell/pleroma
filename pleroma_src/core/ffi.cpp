#pragma once

#include <string>
#include <vector>

#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "ffi.h"

FuncStmt *setup_direct_call(AstNode *(*foreign_func)(EvalContext *,
                                                     std::vector<AstNode *>),
                            std::string name, std::vector<std::string> args,
                            std::vector<CType *> arg_types, CType ctype) {
  std::vector<AstNode *> body;
  std::vector<AstNode *> nu_args;

  for (auto k : args) {
    nu_args.push_back(make_symbol(k));
  }

  auto ffi = make_foreign_func_call(foreign_func, nu_args, ctype);

  body.push_back(make_return(ffi));

  auto func = (FuncStmt *)make_function(name, args, body, arg_types, false);
  func->ctype = ctype;
  return func;
}

void add_function(HalfModule* hm, FuncStmt *fs) {
  hm->functions[fs->name] = fs;
}
