#pragma once

#include <string>
#include "common.h"

enum class PCommand {
  Start,
  Test
};

struct PleromaArgs {
  PCommand command;
  std::string config_path = "pleroma.json";

  std::string local_hostname = "127.0.0.1";
  u32 local_port = 8080;

  std::string remote_hostname;
  u32 remote_port = 0;

  std::string program_path = "examples/helloworld.plm";
  // In the future, we should automatically find this
  std::string entity_name = "UserProgram";
};

PleromaArgs parse_args(int argc, char** argv);
