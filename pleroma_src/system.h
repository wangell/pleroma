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
