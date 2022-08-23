#pragma once

#include <string>

#include "hylic_ast.h"
#include "hylic.h"

enum SystemModule { Monad };

HylicModule* load_system_module(SystemModule mod);
