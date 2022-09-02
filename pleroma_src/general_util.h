#pragma once

#include <string>
#include <vector>

std::vector<std::string> split_import(std::string bstr);

enum LogLevel {
  log_critical,
  log_error,
  log_warning,
  log_info,
  log_debug
};

void dbp(int, const char * format, ...);
