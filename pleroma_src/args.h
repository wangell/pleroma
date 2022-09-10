#pragma once

#include <string>
#include "common.h"

enum class PCommand {
  Start,
  Test
};

struct PleromaArgs {
  PCommand command;
  std::string config_path;

  std::string local_hostname;
  u32 local_port;

  std::string remote_hostname;
  u32 remote_port;

  std::string program_path = "examples/helloworld.plm";
};

PleromaArgs parse_args(int argc, char** argv);
