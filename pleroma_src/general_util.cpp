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

void dbp(int lvl, const char *format, ...) {
  time_t rawtime;
  struct tm *timeinfo;
  char buffer[80];

  switch (lvl) {
  case log_critical:
    printf("\033[1;31m");
    break;
  case log_error:
    printf("\033[1;33m");
    break;
  case log_warning:
    printf("\033[1;35m");
    break;
  case log_info:
    printf("\033[1;37m");
    break;
  default:
    break;
  }

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, 80, "%m-%d %T", timeinfo);

  va_list argptr;
  va_start(argptr, format);
  printf("[%s] ", buffer);
  vprintf(format, argptr);

  // Reset colors
  printf("\033[0m");

  printf("\n");
  va_end(argptr);
}
