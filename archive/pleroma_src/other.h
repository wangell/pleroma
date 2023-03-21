#pragma once

#include <string>
#include <stdexcept>
#include <exception>

class PleromaException : public std::runtime_error
{
 public:
   PleromaException(const char* msg) : std::runtime_error(msg) {}
};
