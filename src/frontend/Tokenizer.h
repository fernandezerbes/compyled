#pragma once

#include <cmath>
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
  Delimiter,
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
  Assign,
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

enum class DelimiterType {
  LeftParenthesis,
  RightParenthesis,
  LeftBracket,
  RightBracket,
  LeftBraces,
  RightBraces,
  Comma,
  Colon,
  Exclamation,
  Semicolon,
  Equal,
  Arrow,
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
  And,
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
  Not,
  With,
  Async,
  Elif,
  If,
  Or,
  Yield,
};

using IntegerType = std::int64_t;
using FloatType = std::double_t;

struct Token {
  using ValueType =
      std::variant<std::string, IntegerType, FloatType, OperatorType, KeywordType, DelimiterType>;

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

std::optional<std::pair<IntegerType, std::size_t>> getIntegerLiteral(const std::string_view Source);

std::optional<std::pair<FloatType, std::size_t>> getFloatLiteral(const std::string_view Source);

std::optional<std::pair<std::variant<OperatorType, DelimiterType>, std::size_t>>
getOperatorOrDelimiterType(const std::string_view Source);

std::optional<std::string> getName(const std::string_view Source);

} // namespace detail

} // namespace compyled::lexer
