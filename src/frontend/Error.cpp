
#include "Error.h"

namespace compyled::error {

CompyledException::CompyledException(std::string &&Message, types::SourceLocation Location)
    : std::runtime_error(std::move(Message)), Location_(std::move(Location)) {}

} // namespace compyled::error