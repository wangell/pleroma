#include "args.h"
#include "other.h"

#include <string>
#include <vector>
#include <algorithm>

std::vector<std::string> acceptable_opts = {
  "config",
  "local-host",
  "remote-host",
  "program",
  "entity"
};

std::vector<std::string> acceptable_flags = {
  "v"
};

PleromaArgs parse_args(int argc, char** argv) {
  PleromaArgs pargs;

  std::vector<std::string> vargs;

  for (int k = 1; k < argc; ++k) {
    vargs.push_back(argv[k]);
  }

  if (vargs.size() < 1) {
    throw PleromaException("Must use command [start, test]");
  }

  if (vargs[0] == "start") {
    pargs.command = PCommand::Start;
  } else if (vargs[0] == "test") {
    pargs.command = PCommand::Test;
  } else {
    throw PleromaException("Need valid command: start or test.");
  }

  if (pargs.command == PCommand::Start) {

    for (int k = 1; k < vargs.size(); ++k) {
      // Option vs flag
      if (vargs[k][0] == '-' && vargs[k][1] == '-') {
        std::string opt_name = std::string(vargs[k].begin()+2, vargs[k].end());

        if (k + 1 >= vargs.size() || vargs[k + 1][0] == '-') {
          throw PleromaException(std::string("Missing an argument to an option: " + opt_name).c_str());
        }

        std::string opt_val = vargs[k + 1];

        if (opt_name == "local-host") {
          auto fcol = opt_val.find(":");
          pargs.local_hostname = opt_val.substr(0, fcol);
          std::string port_string = opt_val.substr(fcol + 1, opt_val.size() - 1);
          pargs.local_port = std::stoi(port_string);
        } else if (opt_name == "remote-host") {
          auto fcol = opt_val.find(":");
          pargs.remote_hostname = opt_val.substr(0, fcol);
          std::string port_string = opt_val.substr(fcol + 1, opt_val.size() - 1);
          pargs.remote_port = std::stoi(port_string);
        } else if (opt_name == "config") {
          pargs.config_path = opt_val;
        } else if (opt_name == "program") {
          pargs.program_path = opt_val;
        } else if (opt_name == "entity") {
          pargs.entity_name = opt_val;
        } else {
          throw PleromaException(("Invalid command-line option: " + opt_name).c_str());
        }

        } else if (vargs[k][0] == '-') {
        }
    }
  }

  return pargs;
}
