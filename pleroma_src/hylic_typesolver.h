#pragma once

#include <map>
#include <string>
#include "hylic.h"
#include "hylic_ast.h"

class TypesolverException : CompileException {
};

void typesolve(HylicModule* module);
