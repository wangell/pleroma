#pragma once

#include <queue>
#include <mutex>
#include <map>
#include "hylic.h"
#include "hylic_eval.h"
#include "common.h"

extern std::map<int, Vat *> vats;

struct PleromaNode {
  u32 node_id;
};
