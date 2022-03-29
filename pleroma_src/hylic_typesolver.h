#pragma once

#include <map>
#include <string>
#include "hylic_ast.h"

void typesolve(std::map<std::string, AstNode *> program);
