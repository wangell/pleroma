#pragma once

#include <map>
#include <string>
#include "hylic.h"
#include "hylic_ast.h"

class TypesolverException : public CompileException {
public:
  TypesolverException(std::string file, u32 line_n, u32 char_n, std::string msg)
      : CompileException(file, line_n, char_n, msg) {}
};
void typesolve(HylicModule* module);
