#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace compyled::types {

struct SourceLocation {
  std::size_t Line;
  std::size_t Column;
};

using IntegerType = std::int64_t;
using FloatType = std::double_t;
using BooleanType = bool;

} // namespace compyled::types