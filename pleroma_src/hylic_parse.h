#pragma once

#include <map>
#include <string>
#include "hylic_ast.h"
#include "hylic_tokenizer.h"
#include "hylic.h"
#include <cassert>

std::map<std::string, AstNode *> parse(TokenStream stream);
