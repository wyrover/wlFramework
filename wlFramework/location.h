#ifndef WL_LOCATION_H_
#define WL_LOCATION_H_

#include <string>
#include "stdafx.h"

extern "C" {
  void* _ReturnAddress();
};

namespace wl {
class WL_EXPORT Location {
public:
  Location(const char* function_name, const char* file_name, int line_number, const void* program_counter);
  bool operator<(const Location& other) const {
    if (line_number_ != other.line_number_)
      return line_number_ < other.line_number_;
    if (file_name_ != other.file_name_)
      return file_name_ < other.file_name_;
    return function_name_ < other.function_name_;
  }
  std::string ToString() const;
private:
  const char* function_name_;
  const char* file_name_;
  int line_number_;
  const void* program_counter_;
};
}

#define FROM_HERE wl::Location(__FUNCTION__, __FILE__, __LINE__, _ReturnAddress())

#endif