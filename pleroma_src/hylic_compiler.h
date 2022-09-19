#pragma once

#include "hylic_ast.h"
#include <fstream>
#include <map>
#include <string>

struct CompileContext {
  std::ofstream comp_out;

  std::map<std::string, u64> functions;
  std::vector<std::tuple<std::string, u64>> function_thunks;
};

void compile(std::map<std::string, AstNode*> program);
void compile_node(CompileContext *cc, AstNode *in_node);
