#include "Tokenizer.h"
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>

namespace compyled::lexer {

namespace {

constexpr char WHITESPACE = ' ';
constexpr char NEWLINE = '\n';
constexpr char COLON = ':';
constexpr char SEMICOLON = ';';
constexpr auto *DELIMITERS = " :\n;";
constexpr std::size_t DEFAULT_INDENT_SIZE = 4;

} // namespace

Tokenizer::Tokenizer(std::string &&Source, const std::size_t IndentSize)
    : Source_(std::move(Source)), CurrPos_{}, IndentSize_{IndentSize} {}

Tokenizer::Tokenizer(std::string &&Source)
    : compyled::lexer::Tokenizer(std::move(Source), DEFAULT_INDENT_SIZE) {}

std::expected<Token, std::string> Tokenizer::getNextToken() {
  if (CurrPos_ >= Source_.size()) {
    return Token{.Type = TokenType::Eof, .Location = CurrLocation_};
  }

  const auto StartPos = CurrPos_;
  while (CurrPos_ < Source_.size() && Source_[CurrPos_] == WHITESPACE &&
         (CurrPos_ - StartPos) < IndentSize_) {
    CurrPos_++;
  }
  const auto WhitespaceCount = CurrPos_ - StartPos;
  if (WhitespaceCount == 1) {
    CurrLocation_.Column++;
    return Token{.Type = TokenType::Whitespace, .Location = CurrLocation_};
  } else if (WhitespaceCount == IndentSize_) {
    CurrLocation_.Column += IndentSize_;
    return Token{.Type = TokenType::Indentation, .Location = CurrLocation_};
  } else if (WhitespaceCount != 0) {
    return std::unexpected(
        std::format("Invalid token at {}:{}", CurrLocation_.Line, CurrLocation_.Column));
  }

  if (Source_[CurrPos_] == NEWLINE) {
    CurrPos_++;
    CurrLocation_.Line++;
    CurrLocation_.Column = 0;
    return Token{.Type = TokenType::NewLine, .Location = CurrLocation_};
  }

  const auto FoundPos = Source_.find_first_of(DELIMITERS, CurrPos_);
  if (FoundPos == std::string::npos) {
    return std::unexpected(
        std::format("Invalid token at {}:{}", CurrLocation_.Line, CurrLocation_.Column));
  }

  auto IntValue = std::int64_t{};
  const auto [ptr, ec] = std::from_chars(&Source_[StartPos], &Source_[FoundPos], IntValue);
  assert(ptr == &Source_[FoundPos]);
  if (ec == std::errc{}) {
    return Token{.Type = TokenType::Integer, .Location = CurrLocation_, .Value = IntValue};
  }

  auto FloatValue = double{};
  const auto [ptr2, ec2] = std::from_chars(&Source_[StartPos], &Source_[FoundPos], FloatValue);
  assert(ptr2 == &Source_[FoundPos]);
  if (ec2 == std::errc{}) {
    return Token{.Type = TokenType::Float, .Location = CurrLocation_, .Value = FloatValue};
  }

  CurrPos_ = FoundPos;
  CurrLocation_.Column += FoundPos - StartPos;

  return Token{.Type = TokenType::Identifier,
               .Location = CurrLocation_,
               .Value = std::string(Source_.cbegin() + StartPos, Source_.cend() + FoundPos)};
}

} // namespace compyled::lexer