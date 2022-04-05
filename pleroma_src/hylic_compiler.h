#pragma once

#include <map>
#include <string>
#include "hylic_ast.h"

void compile(std::map<std::string, AstNode*> program);
