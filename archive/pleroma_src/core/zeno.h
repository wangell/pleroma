#pragma once

#include <map>
#include <string>
#include "../hylic_ast.h"

std::map<std::string, AstNode *> load_zeno();
void write_file(std::string filename, std::string contents);
