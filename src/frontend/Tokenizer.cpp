#include "Tokenizer.h"
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>

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

  CurrPos_ = FoundPos;
  CurrLocation_.Column += FoundPos - StartPos;

  const auto IntValue = detail::getFloatLiteral({&Source_[StartPos], &Source_[FoundPos]});
  if (IntValue) {
    return Token{.Type = TokenType::Integer, .Location = CurrLocation_, .Value = *IntValue};
  }

  const auto FloatValue = detail::getFloatLiteral({&Source_[StartPos], &Source_[FoundPos]});
  if (FloatValue) {
    return Token{.Type = TokenType::Float, .Location = CurrLocation_, .Value = *FloatValue};
  }

  const auto OpType = detail::getOperatorType({&Source_[StartPos], &Source_[FoundPos]});
  if (OpType) {
    return Token{.Type = TokenType::Operator, .Location = CurrLocation_, .Value = *OpType};
  }

  const auto KeyType = detail::getKeywordType({&Source_[StartPos], &Source_[FoundPos]});
  if (KeyType) {
    return Token{.Type = TokenType::Keyword, .Location = CurrLocation_, .Value = *KeyType};
  }

  return Token{.Type = TokenType::Identifier,
               .Location = CurrLocation_,
               .Value = std::string(Source_.cbegin() + StartPos, Source_.cend() + FoundPos)};
}

namespace detail {

// TODO: Implement actual integer parsing as per
// https://docs.python.org/3/reference/lexical_analysis.html#integer-literals
std::optional<std::int64_t> getIntegerLiteral(const std::string_view IntegerStr) {
  auto IntValue = std::int64_t{};
  const auto [ptr, ec] = std::from_chars(IntegerStr.cbegin(), IntegerStr.cend(), IntValue);
  assert(ptr == IntegerStr.cend());
  if (ec == std::errc{}) {
    return IntValue;
  }

  return std::nullopt;
}

// TODO: Implement actual floating point parsing as per
// https://docs.python.org/3/reference/lexical_analysis.html#floating-point-literals
std::optional<double> getFloatLiteral(const std::string_view FloatStr) {
  auto IntValue = std::int64_t{};
  const auto [ptr, ec] = std::from_chars(FloatStr.cbegin(), FloatStr.cend(), IntValue);
  assert(ptr == FloatStr.cend());
  if (ec == std::errc{}) {
    return IntValue;
  }

  return std::nullopt;
}

/*
 * From https://docs.python.org/3/reference/lexical_analysis.html#operators-and-delimiters
 * Excludes delimiters and includes logical operators.
 *   OP:
 *      | assignment_operator
 *      | bitwise_operator
 *      | comparison_operator
 *      | enclosing_delimiter
 *      | other_delimiter
 *      | arithmetic_operator
 *      | "..."
 *      | other_op
 *
 *   assignment_operator:   "+=" | "-=" | "*=" | "**=" | "/="  | "//=" | "%=" |
 *                          "&=" | "|=" | "^=" | "<<=" | ">>=" | "@="  | ":="
 *   bitwise_operator:      "&"  | "|"  | "^"  | "~"   | "<<"  | ">>"
 *   comparison_operator:   "<=" | ">=" | "<"  | ">"   | "=="  | "!="
 *   enclosing_delimiter:   "("  | ")"  | "["  | "]"   | "{"   | "}"
 *   other_delimiter:       ","  | ":"  | "!"  | ";"   | "="   | "->"
 *   arithmetic_operator:   "+"  | "-"  | "**" | "*"   | "//"  | "/"   | "%"
 *   other_op:              "."  | "@"
 *
 */
std::optional<OperatorType> getOperatorType(const std::string_view OperatorStr) {
  if (OperatorStr == "+=")
    return OperatorType::UnaryAdd;
  if (OperatorStr == "-=")
    return OperatorType::UnarySubtract;
  if (OperatorStr == "*=")
    return OperatorType::UnaryMultiply;
  if (OperatorStr == "**=")
    return OperatorType::UnaryExponentiate;
  if (OperatorStr == "/=")
    return OperatorType::UnaryFloatDivide;
  if (OperatorStr == "//=")
    return OperatorType::UnaryIntegerDivide;
  if (OperatorStr == "%=")
    return OperatorType::UnaryModulo;
  if (OperatorStr == "&=")
    return OperatorType::UnaryBitwiseAnd;
  if (OperatorStr == "|=")
    return OperatorType::UnaryBitwiseOr;
  if (OperatorStr == "^=")
    return OperatorType::UnaryBitwiseXor;
  if (OperatorStr == "<<=")
    return OperatorType::UnaryLeftShift;
  if (OperatorStr == ">>=")
    return OperatorType::UnaryRightShift;
  if (OperatorStr == "&")
    return OperatorType::BitwiseAnd;
  if (OperatorStr == "|")
    return OperatorType::BitwiseOr;
  if (OperatorStr == "^")
    return OperatorType::BitwiseXor;
  if (OperatorStr == "~")
    return OperatorType::BitwiseInvert;
  if (OperatorStr == "<<")
    return OperatorType::LeftShift;
  if (OperatorStr == ">>")
    return OperatorType::RightShift;
  if (OperatorStr == "<=")
    return OperatorType::LessOrEqual;
  if (OperatorStr == ">=")
    return OperatorType::GreaterOrEqual;
  if (OperatorStr == "<")
    return OperatorType::Less;
  if (OperatorStr == ">")
    return OperatorType::Greater;
  if (OperatorStr == "==")
    return OperatorType::Equal;
  if (OperatorStr == "!=")
    return OperatorType::NotEqual;
  if (OperatorStr == "+")
    return OperatorType::Add;
  if (OperatorStr == "-")
    return OperatorType::Subtract;
  if (OperatorStr == "**")
    return OperatorType::Exponentiate;
  if (OperatorStr == "*")
    return OperatorType::Multiply;
  if (OperatorStr == "//")
    return OperatorType::IntegerDivide;
  if (OperatorStr == "/")
    return OperatorType::FloatDivide;
  if (OperatorStr == "%")
    return OperatorType::Modulo;
  if (OperatorStr == "not")
    return OperatorType::LogicalNot;
  if (OperatorStr == "and")
    return OperatorType::LogicalAnd;
  if (OperatorStr == "or")
    return OperatorType::LogicalOr;

  return std::nullopt;
}

// From https://docs.python.org/3/reference/lexical_analysis.html#keywords
// Excludes "and", "or", and "not" because those are considered OperatorTypes.
std::optional<KeywordType> getKeywordType(const std::string_view OperatorStr) {
  if (OperatorStr == "false")
    return KeywordType::False;
  if (OperatorStr == "await")
    return KeywordType::Await;
  if (OperatorStr == "else")
    return KeywordType::Else;
  if (OperatorStr == "import")
    return KeywordType::Import;
  if (OperatorStr == "pass")
    return KeywordType::Pass;
  if (OperatorStr == "none")
    return KeywordType::None;
  if (OperatorStr == "break")
    return KeywordType::Break;
  if (OperatorStr == "except")
    return KeywordType::Except;
  if (OperatorStr == "in")
    return KeywordType::In;
  if (OperatorStr == "raise")
    return KeywordType::Raise;
  if (OperatorStr == "True")
    return KeywordType::True;
  if (OperatorStr == "class")
    return KeywordType::Class;
  if (OperatorStr == "finally")
    return KeywordType::Finally;
  if (OperatorStr == "is")
    return KeywordType::Is;
  if (OperatorStr == "return")
    return KeywordType::Return;
  if (OperatorStr == "continue")
    return KeywordType::Continue;
  if (OperatorStr == "for")
    return KeywordType::For;
  if (OperatorStr == "lambda")
    return KeywordType::Lambda;
  if (OperatorStr == "try")
    return KeywordType::Try;
  if (OperatorStr == "as")
    return KeywordType::As;
  if (OperatorStr == "def")
    return KeywordType::Def;
  if (OperatorStr == "from")
    return KeywordType::From;
  if (OperatorStr == "nonlocal")
    return KeywordType::Nonlocal;
  if (OperatorStr == "while")
    return KeywordType::While;
  if (OperatorStr == "assert")
    return KeywordType::Assert;
  if (OperatorStr == "del")
    return KeywordType::Del;
  if (OperatorStr == "global")
    return KeywordType::Global;
  if (OperatorStr == "with")
    return KeywordType::With;
  if (OperatorStr == "async")
    return KeywordType::Async;
  if (OperatorStr == "elif")
    return KeywordType::Elif;
  if (OperatorStr == "if")
    return KeywordType::If;
  if (OperatorStr == "yield")
    return KeywordType::Yield;

  return std::nullopt;
}

} // namespace detail

} // namespace compyled::lexer