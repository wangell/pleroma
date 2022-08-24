#pragma once

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <list>
#include "hylic_tokenizer.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include "hylic_parse.h"

HylicModule *load_file(std::string path);
std::map<std::string, AstNode *> parse(TokenStream stream);
bool typecheck(std::map<std::string, AstNode *>);
AstNode *eval(AstNode *obj, Scope *scope);
void parse_file(std::string path);

extern EntityRefNode *monad_ref;
