#pragma once

#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include "common.h"


class CompileException : public std::runtime_error {
public:
  std::string build_exception_msg(std::string file, u32 line_n, u32 char_n,
                                  std::string specific_msg) {
    std::ifstream infile(file);
    std::string line;
    int i = 0;
    while (std::getline(infile, line)) {
      std::istringstream iss(line);
      i += 1;
      if (i == line_n) break;
    }

    std::string outmsg = specific_msg + ": " + file + ", line " +
                         std::to_string(line_n) + ", char " +
      std::to_string(char_n) + "\n" + "\033[1;31m" + line + "\033[0m\n";

    return outmsg;
  }

  CompileException(std::string file, u32 line_n, u32 char_n, std::string msg)
    : std::runtime_error(build_exception_msg(file, line_n, char_n, msg)) {}
};
