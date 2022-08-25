#pragma once

#include "general_util.h"

std::vector<std::string> split_import(std::string bstr) {

  std::vector<std::string> tokens;

  size_t pos = 0;
  std::string token;
  int i = 0;
  while ((pos = bstr.find("►")) != std::string::npos) {
    token = bstr.substr(0, pos);
    tokens.push_back(token);

    // This probably doesn't work, need real UTF-8 library
    bstr.erase(0, pos + 3);
  }

  tokens.push_back(bstr);

  // for (auto k : tokens) {
  //  printf("tok %s\n", k.c_str());
  //}

  return tokens;
}
