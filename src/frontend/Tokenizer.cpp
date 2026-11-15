#include "Tokenizer.h"
#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace compyled::lexer {

namespace {

static const std::unordered_map<std::string_view, KeywordType> STRING_TO_KEYWORD = {
    {"false", KeywordType::False},
    {"await", KeywordType::Await},
    {"else", KeywordType::Else},
    {"import", KeywordType::Import},
    {"pass", KeywordType::Pass},
    {"none", KeywordType::None},
    {"break", KeywordType::Break},
    {"except", KeywordType::Except},
    {"in", KeywordType::In},
    {"raise", KeywordType::Raise},
    {"true", KeywordType::True},
    {"class", KeywordType::Class},
    {"finally", KeywordType::Finally},
    {"is", KeywordType::Is},
    {"return", KeywordType::Return},
    {"and", KeywordType::And},
    {"continue", KeywordType::Continue},
    {"for", KeywordType::For},
    {"lambda", KeywordType::Lambda},
    {"try", KeywordType::Try},
    {"as", KeywordType::As},
    {"def", KeywordType::Def},
    {"from", KeywordType::From},
    {"nonlocal", KeywordType::Nonlocal},
    {"while", KeywordType::While},
    {"assert", KeywordType::Assert},
    {"del", KeywordType::Del},
    {"global", KeywordType::Global},
    {"not", KeywordType::Not},
    {"with", KeywordType::With},
    {"async", KeywordType::Async},
    {"elif", KeywordType::Elif},
    {"if", KeywordType::If},
    {"or", KeywordType::Or},
    {"yield", KeywordType::Yield},
};

constexpr std::size_t DEFAULT_INDENT_SIZE = 4;

} // namespace

Tokenizer::Tokenizer(std::string &&Source, const std::size_t IndentSize)
    : Source_(std::move(Source)), CurrPos_{}, IndentSize_{IndentSize} {
  if (IndentSize % 2 != 0) {
    throw std::invalid_argument(std::format("Indent size {} is not a multiple of 2", IndentSize));
  }
}

Tokenizer::Tokenizer(std::string &&Source)
    : compyled::lexer::Tokenizer(std::move(Source), DEFAULT_INDENT_SIZE) {}

std::expected<Token, std::string> Tokenizer::getNextToken() {
  if (CurrPos_ == Source_.size()) {
    return Token{.Type = TokenType::Eof, .Location = CurrLocation_};
  } else if (CurrPos_ > Source_.size()) {
    return std::unexpected("Cannot read past the end of source");
  }

  const auto StartLocation = CurrLocation_;
  const auto StartPos = CurrPos_;
  while (Source_[CurrPos_] == ' ' && (CurrPos_ - StartPos) < IndentSize_) {
    CurrPos_++;
  }

  auto WhitespaceCount = CurrPos_ - StartPos;
  if (WhitespaceCount == 1) {
    return Token{.Type = TokenType::Whitespace, .Location = StartLocation};
  } else if (WhitespaceCount == IndentSize_) {
    return Token{.Type = TokenType::Indentation, .Location = StartLocation};
  } else if (WhitespaceCount > 0 && WhitespaceCount < IndentSize_) {
    return std::unexpected(std::format("Unexpected number of whitespaces '()' at line {}:{}",
                                       WhitespaceCount, CurrLocation_.Line, CurrLocation_.Column));
  }

  if (Source_[CurrPos_] == '\n') {
    CurrPos_++;
    CurrLocation_.Line++;
    CurrLocation_.Column = 0;
    return Token{.Type = TokenType::NewLine, .Location = StartLocation};
  }

  const auto RemainingSource = std::string_view{Source_.cbegin() + CurrPos_, Source_.cend()};

  const auto OperatorOrDelimiterType = detail::getOperatorOrDelimiterType(RemainingSource);
  if (OperatorOrDelimiterType) {
    CurrPos_ += OperatorOrDelimiterType->second;
    CurrLocation_.Column += OperatorOrDelimiterType->second;
    if (std::holds_alternative<OperatorType>(OperatorOrDelimiterType->first)) {
      return Token{.Type = TokenType::Operator,
                   .Location = StartLocation,
                   .Value = std::get<OperatorType>(OperatorOrDelimiterType->first)};
    }
    return Token{.Type = TokenType::Delimiter,
                 .Location = StartLocation,
                 .Value = std::get<DelimiterType>(OperatorOrDelimiterType->first)};
  }

  const auto Name = detail::getName(RemainingSource);
  if (Name) {
    CurrPos_ += Name->size();
    CurrLocation_.Column += Name->size();
    const auto it = STRING_TO_KEYWORD.find(*Name);
    if (it != STRING_TO_KEYWORD.cend()) {
      switch (it->second) {
      case KeywordType::And:
        return Token{.Type = TokenType::Operator,
                     .Location = StartLocation,
                     .Value = OperatorType::LogicalAnd};
      case KeywordType::Or:
        return Token{.Type = TokenType::Operator,
                     .Location = StartLocation,
                     .Value = OperatorType::LogicalOr};
      case KeywordType::Not:
        return Token{.Type = TokenType::Operator,
                     .Location = StartLocation,
                     .Value = OperatorType::LogicalNot};
      case KeywordType::True:
        return Token{.Type = TokenType::Boolean, .Location = StartLocation, .Value = true};
      case KeywordType::False:
        return Token{.Type = TokenType::Boolean, .Location = StartLocation, .Value = false};

      default:
        // No other keywords are operators or booleans and don't need special treatment.
      }
      return Token{.Type = TokenType::Keyword, .Location = StartLocation, .Value = it->second};
    } else {
      return Token{
          .Type = TokenType::Identifier, .Location = StartLocation, .Value = std::move(*Name)};
    }
  }

  const auto FloatLiteral = detail::getFloatLiteral(RemainingSource);
  if (FloatLiteral) {
    CurrPos_ += FloatLiteral->second;
    CurrLocation_.Column += FloatLiteral->second;
    return Token{.Type = TokenType::Float, .Location = StartLocation, .Value = FloatLiteral->first};
  }

  const auto IntegerLiteral = detail::getIntegerLiteral(RemainingSource);
  if (IntegerLiteral) {
    CurrPos_ += IntegerLiteral->second;
    CurrLocation_.Column += IntegerLiteral->second;
    return Token{
        .Type = TokenType::Integer, .Location = StartLocation, .Value = IntegerLiteral->first};
  }

  return std::unexpected(std::format("Unexpected character '{}' at line {}:{}", Source_[0],
                                     CurrLocation_.Line, CurrLocation_.Column));
}

namespace detail {

// TODO: Implement actual integer parsing as per
// https://docs.python.org/3/reference/lexical_analysis.html#integer-literals
std::optional<std::pair<IntegerType, std::size_t>>
getIntegerLiteral(const std::string_view Source) {
  auto IntValue = IntegerType{};
  const auto [ptr, ec] = std::from_chars(Source.cbegin(), Source.cend(), IntValue);
  if (ec == std::errc{}) {
    return std::pair{IntValue, std::distance(Source.cbegin(), ptr)};
  }

  return std::nullopt;
}

// TODO: Implement actual floating point parsing as per
// https://docs.python.org/3/reference/lexical_analysis.html#floating-point-literals
std::optional<std::pair<FloatType, std::size_t>> getFloatLiteral(const std::string_view Source) {
  auto FloatValue = FloatType{};
  const auto [ptr, ec] = std::from_chars(Source.cbegin(), Source.cend(), FloatValue);
  if (ec == std::errc{}) {
    return std::pair{FloatValue, std::distance(Source.cbegin(), ptr)};
  }

  return std::nullopt;
}

/*
 * From https://docs.python.org/3/reference/lexical_analysis.html#operators-and-delimiters
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
std::optional<std::pair<std::variant<OperatorType, DelimiterType>, std::size_t>>
getOperatorOrDelimiterType(const std::string_view Source_) {
  auto CurrPos_ = 0uz;
  switch (Source_[CurrPos_]) {
  case '+': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::UnaryAdd, CurrPos_};
    }
    return std::pair{OperatorType::Add, CurrPos_};
  }
  case '-': {
    CurrPos_++;
    if (CurrPos_ < Source_.size()) {
      if (Source_[CurrPos_] == '=') {
        CurrPos_++;
        return std::pair{OperatorType::UnarySubtract, CurrPos_};
      } else if (Source_[CurrPos_] == '>') {
        CurrPos_++;
        return std::pair{DelimiterType::Arrow, CurrPos_};
      }
    }
    return std::pair{OperatorType::Subtract, CurrPos_};
  }
  case '*': {
    CurrPos_++;
    if (CurrPos_ < Source_.size()) {
      CurrPos_++;
      if (Source_[CurrPos_] == '*') {
        CurrPos_++;
        if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
          CurrPos_++;
          return std::pair{OperatorType::UnaryExponentiate, CurrPos_};
        }
        return std::pair{OperatorType::Exponentiate, CurrPos_};
      } else if (Source_[CurrPos_] == '=') {
        CurrPos_++;
        return std::pair{OperatorType::UnaryMultiply, CurrPos_};
      }
    }
    return std::pair{OperatorType::Multiply, CurrPos_};
  }
  case '/': {
    CurrPos_++;
    if (CurrPos_ < Source_.size()) {
      CurrPos_++;
      if (Source_[CurrPos_] == '/') {
        CurrPos_++;
        if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
          CurrPos_++;
          return std::pair{OperatorType::UnaryIntegerDivide, CurrPos_};
        }
        return std::pair{OperatorType::IntegerDivide, CurrPos_};
      } else if (Source_[CurrPos_] == '=') {
        CurrPos_++;
        return std::pair{OperatorType::UnaryFloatDivide, CurrPos_};
      }
    }
    return std::pair{OperatorType::FloatDivide, CurrPos_};
  }
  case '%': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::UnaryModulo, CurrPos_};
    }
    return std::pair{OperatorType::Modulo, CurrPos_};
  }
  case '&': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::UnaryBitwiseAnd, CurrPos_};
    }
    return std::pair{OperatorType::BitwiseAnd, CurrPos_};
  }
  case '|': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::UnaryBitwiseOr, CurrPos_};
    }
    return std::pair{OperatorType::BitwiseOr, CurrPos_};
  }
  case '^': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::UnaryBitwiseXor, CurrPos_};
    }
    return std::pair{OperatorType::BitwiseXor, CurrPos_};
  }
  case '<': {
    CurrPos_++;
    if (CurrPos_ < Source_.size()) {
      CurrPos_++;
      if (Source_[CurrPos_] == '<') {
        CurrPos_++;
        if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
          CurrPos_++;
          return std::pair{OperatorType::UnaryLeftShift, CurrPos_};
        }
        return std::pair{OperatorType::LeftShift, CurrPos_};
      } else if (Source_[CurrPos_] == '=') {
        CurrPos_++;
        return std::pair{OperatorType::LessOrEqual, CurrPos_};
      }
      return std::pair{OperatorType::Less, CurrPos_};
    }
    return std::pair{OperatorType::Less, CurrPos_};
  }
  case '>': {
    CurrPos_++;
    if (CurrPos_ < Source_.size()) {
      CurrPos_++;
      if (Source_[CurrPos_] == '>') {
        CurrPos_++;
        if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
          CurrPos_++;
          return std::pair{OperatorType::UnaryRightShift, CurrPos_};
        }
        return std::pair{OperatorType::RightShift, CurrPos_};
      } else if (Source_[CurrPos_] == '=') {
        CurrPos_++;
        return std::pair{OperatorType::GreaterOrEqual, CurrPos_};
      }
    }
    return std::pair{OperatorType::Greater, CurrPos_};
  }
  case '~': {
    return std::pair{OperatorType::BitwiseInvert, CurrPos_};
  }
  case '=': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::Equal, CurrPos_};
    }
    return std::pair{OperatorType::Assign, CurrPos_};
  }
  case '!': {
    CurrPos_++;
    if (CurrPos_ < Source_.size() && Source_[CurrPos_] == '=') {
      CurrPos_++;
      return std::pair{OperatorType::NotEqual, CurrPos_};
    }
    return std::pair{DelimiterType::Exclamation, CurrPos_};
  }
  case '(': {
    CurrPos_++;
    return std::pair{DelimiterType::LeftParenthesis, CurrPos_};
  }
  case ')': {
    CurrPos_++;
    return std::pair{DelimiterType::RightParenthesis, CurrPos_};
  }
  case '[': {
    CurrPos_++;
    return std::pair{DelimiterType::LeftBracket, CurrPos_};
  }
  case ']': {
    CurrPos_++;
    return std::pair{DelimiterType::RightBracket, CurrPos_};
  }
  case '{': {
    CurrPos_++;
    return std::pair{DelimiterType::LeftBraces, CurrPos_};
  }
  case '}': {
    CurrPos_++;
    return std::pair{DelimiterType::RightBraces, CurrPos_};
  }
  case ',': {
    CurrPos_++;
    return std::pair{DelimiterType::Comma, CurrPos_};
  }
  case ':': {
    CurrPos_++;
    return std::pair{DelimiterType::Colon, CurrPos_};
  }
  case ';': {
    CurrPos_++;
    return std::pair{DelimiterType::Semicolon, CurrPos_};
  }
  default:
    return std::nullopt;
  };
  std::unreachable();
}

// From https://docs.python.org/3/reference/lexical_analysis.html#keywords
std::optional<std::string> getName(const std::string_view Source) {
  assert(!Source.empty());
  auto CurrPos = 0uz;
  if (std::isalpha(Source[CurrPos]) || Source[CurrPos] == '_') {
    CurrPos++;
    while (CurrPos < Source.size() && (std::isalnum(Source[CurrPos]) || Source[CurrPos] == '_')) {
      CurrPos++;
    }
  }
  if (CurrPos > 0) {
    return std::string(Source.cbegin(), Source.cbegin() + CurrPos);
  }
  return std::nullopt;
}

} // namespace detail

} // namespace compyled::lexer