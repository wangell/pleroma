#pragma once

#include <queue>
#include <mutex>
#include <map>
#include "hylic.h"
#include "hylic_eval.h"

struct Msg {
  int actor_id;
  int function_id;
};

struct Vat {
  int id = 0;
  int run_n = 0;

  std::mutex message_mtx;
  std::queue<Msg> messages;

  std::map<int, Entity*> entities;
};

extern std::map<int, Vat *> vats;
