#pragma once

#include <queue>
#include <mutex>
#include <map>
#include "hylic.h"

struct Msg {
  int actor_id;
  int function_id;
};

struct Vat {
  int id = 0;
  int run_n = 0;

  std::mutex message_mtx;
  std::queue<Msg> messages;

  std::map<int, Actor> actors;
};

extern std::map<int, Vat *> vats;
