#pragma once

#include "Types.h"
#include <stdexcept>
#include <string>

namespace compyled::error {

class CompyledException : public std::runtime_error {
  types::SourceLocation Location_;

public:
  CompyledException(std::string &&Message, types::SourceLocation Location);
  ~CompyledException() override = default;
};

} // namespace compyled::error