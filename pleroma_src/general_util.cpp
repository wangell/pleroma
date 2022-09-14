#pragma once

#include "general_util.h"
#include <algorithm>
#include <bits/types/__FILE.h>
#include <fstream>
#include <string>

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

void _panic(std::string msg, std::string file_name, int line_no, std::string func_name) {
  printf("\033[1;31mPANIC\033[0m (%s, line %d, %s) : %s\n", file_name.c_str(),
         line_no, func_name.c_str(), msg.c_str());

  std::string line;
  std::ifstream badfile("pleroma_src/" + file_name);
  std::string file_conts;

  int start_val = std::max(0, line_no - 10);
  int end_val = line_no + 10;
  int i = 0;
  printf("%s:\n", file_name.c_str());
  while (std::getline(badfile, line)) {
    if (i >= start_val && i <= end_val) {
      if (i == line_no - 1) printf("\033[1;31m");
      printf("\t%s\n", line.c_str());
      if (i == line_no - 1) printf("\033[0m");
    }
    i++;
  }

  exit(1);
}
