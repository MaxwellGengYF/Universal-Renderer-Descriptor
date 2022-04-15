#pragma once

#include "../config.h"
namespace tf {

// Procedure: ostreamize
template <typename T>
void ostreamize(std::ostream& os, T&& token) {
  os << std::forward<T>(token);
}

// Procedure: ostreamize
template <typename T, typename... Rest>
void ostreamize(std::ostream& os, T&& token, Rest&&... rest) {
  os << std::forward<T>(token);
  ostreamize(os, std::forward<Rest>(rest)...);
}

// Function: stringify
template <typename... ArgsT>
string stringify(ArgsT&&... args) {
  ostringstream oss;
  ostreamize(oss, std::forward<ArgsT>(args)...);
  return oss.str();
}


}  // end of namespace tf -----------------------------------------------------

