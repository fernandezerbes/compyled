#pragma once

#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

// Based on https://docs.python.org/3/reference/lexical_analysis.html
namespace compyled::lexer {

enum class BaseType {
  Integer,
  Float,
  Boolean,
  None,
};

enum class TokenType {
  BaseType,
  Value,
  Keyword,
  Operator,
  Delimiter,
  Indent,
  Dedent,
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
  Int,
  Float,
  Bool,
  Range,
};

struct Token {
  using ValueType =
      std::variant<std::string, types::IntegerType, types::FloatType, types::BooleanType,
                   OperatorType, KeywordType, DelimiterType, BaseType>;

  TokenType Type;
  types::SourceLocation Location;
  ValueType Value;
};

class Tokenizer {
  std::string Source_{};
  std::size_t CurrPos_{};
  std::size_t IndentSize_{};
  types::SourceLocation CurrLocation_{};
  // Keeps the value of the next token at all times.
  // Updated every time `consumeNextToken` is called.
  Token NextToken_;
  
  // Update the NextToken_ with the NewToken value, and return the NewToken.
  Token updateNextToken(Token &&NewToken) noexcept;

public:
  // Constructors throw if the Source have no valid tokens. 
  explicit Tokenizer(std::string &&Source);
  Tokenizer(std::string &&Source, const std::size_t IndentSize);

  // Get the value of the next token without advancing the current position.
  Token peekNextToken() const noexcept;

  // Return the value of the next token and then consume it.
  [[maybe_unused]] Token consumeNextToken();
};

namespace detail {

std::optional<std::pair<types::IntegerType, std::size_t>>
getIntegerLiteral(const std::string_view Source);

std::optional<std::pair<types::FloatType, std::size_t>>
getFloatLiteral(const std::string_view Source);

std::optional<std::pair<std::variant<OperatorType, DelimiterType>, std::size_t>>
getOperatorOrDelimiterType(const std::string_view Source);

std::optional<std::string> getName(const std::string_view Source);

} // namespace detail

} // namespace compyled::lexer
