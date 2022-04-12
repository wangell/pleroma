#pragma once

#include <map>
#include <string>
#include "hylic.h"
#include "hylic_ast.h"

struct TypesolverException : CompileException {
};

void typesolve(std::map<std::string, AstNode *> program);
