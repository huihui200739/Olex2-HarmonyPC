#pragma once
#include <string>
#include <vector>
#include <utility>
#include <unistd.h>
namespace olex2::harmony {
// execv receives an argument vector, not a shell command. Store exact UTF-8
// values, including spaces and empty arguments, without inserting quote bytes.
class UnirunProcessArguments {
  std::vector<std::string> values_;
  std::vector<char*> pointers_;
public:
  explicit UnirunProcessArguments(std::vector<std::string> values):values_(std::move(values)) {
    pointers_.reserve(values_.size()+1);for(std::string& value:values_)pointers_.push_back(value.data());pointers_.push_back(nullptr);
  }
  UnirunProcessArguments(const UnirunProcessArguments&)=delete;
  UnirunProcessArguments& operator=(const UnirunProcessArguments&)=delete;
  char* const* data() const {return pointers_.data();}
  size_t size() const {return values_.size();}
};
inline int UnirunReplaceProcess(const std::string& executable,const UnirunProcessArguments& arguments) {return ::execv(executable.c_str(),arguments.data());}
}
