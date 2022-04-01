#pragma once

#include <map>
#include <string>
#include "hylic_ast.h"
#include "hylic_tokenizer.h"
#include "hylic.h"
#include <cassert>

enum class TLUserType {
  Entity,
  Data
};

struct ParseContext {
  std::map<std::string, TLUserType> tl_symbol_table;
};

std::map<std::string, AstNode *> parse(TokenStream stream);

std::vector<AstNode *> parse_block(ParseContext *context, int expected_indent);
