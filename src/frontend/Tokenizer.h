#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <variant>

// Based on https://docs.python.org/3/reference/lexical_analysis.html
namespace compyled::lexer {

struct SourceLocation {
  std::size_t Line;
  std::size_t Column;
};

enum class TokenType {
  Integer,
  Float,
  Keyword,
  Operator,
  Indentation,
  NewLine,
  Identifier,
  Whitespace,
  Eof,
};

struct Token {
  TokenType Type;
  SourceLocation Location;
  std::variant<std::string, std::int64_t, double> Value;
};

class Tokenizer {
public:
  Tokenizer(std::string &&Source);
  Tokenizer(std::string &&Source, const std::size_t IndentSize);

  std::expected<Token, std::string> getNextToken();

private:
  std::string Source_;
  std::size_t CurrPos_;
  std::size_t IndentSize_{};
  SourceLocation CurrLocation_{};
};

} // namespace compyled::lexer
