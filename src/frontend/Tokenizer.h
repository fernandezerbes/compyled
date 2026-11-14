#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
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

enum class OperatorType {
  UnaryAdd,
  UnarySubtract,
  UnaryMultiply,
  UnaryExponentiate,
  UnaryFloatDivide,
  UnaryIntegerDivide,
  UnaryModulo,
  UnaryBitwiseAnd,
  UnaryBitwiseOr,
  UnaryBitwiseXor,
  UnaryLeftShift,
  UnaryRightShift,
  BitwiseAnd,
  BitwiseOr,
  BitwiseXor,
  BitwiseInvert,
  LeftShift,
  RightShift,
  LessOrEqual,
  GreaterOrEqual,
  Less,
  Greater,
  Equal,
  NotEqual,
  Add,
  Subtract,
  Exponentiate,
  Multiply,
  IntegerDivide,
  FloatDivide,
  Modulo,
  LogicalNot,
  LogicalAnd,
  LogicalOr,
};

enum class KeywordType {
  False,
  Await,
  Else,
  Import,
  Pass,
  None,
  Break,
  Except,
  In,
  Raise,
  True,
  Class,
  Finally,
  Is,
  Return,
  // And,   // Parsed as OperatorType::LogicalAnd
  Continue,
  For,
  Lambda,
  Try,
  As,
  Def,
  From,
  Nonlocal,
  While,
  Assert,
  Del,
  Global,
  // Not,   // Parsed as OperatorType::LogicalNot
  With,
  Async,
  Elif,
  If,
  // Or,    // Parsed as OperatorType::LogicalOr
  Yield,
};

struct Token {
  using ValueType = std::variant<std::string, std::int64_t, double, OperatorType, KeywordType>;

  TokenType Type;
  SourceLocation Location;
  ValueType Value;
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

namespace detail {

std::optional<std::int64_t> getIntegerLiteral(const std::string_view IntegerStr);
std::optional<double> getFloatLiteral(const std::string_view FloatStr);
std::optional<OperatorType> getOperatorType(const std::string_view OperatorStr);
std::optional<KeywordType> getKeywordType(const std::string_view OperatorStr);

} // namespace detail

} // namespace compyled::lexer
