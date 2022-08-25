#pragma once

#include <string>

#include "hylic_ast.h"
#include "hylic.h"

enum SystemModule {
  Monad,
  Net,
  Io
};

HylicModule* load_system_module(SystemModule mod);

bool is_system_module(std::string import_string);

SystemModule system_import_to_enum(std::string str);
