#include "Tokenizer.h"
#include "Error.h"
#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace compyled::lexer {

namespace {

// From https://docs.python.org/3/reference/lexical_analysis.html#keywords
// In addition:
//  - Types int, float, bool
//  - range
static const std::unordered_map<std::string_view, KeywordType> STRING_TO_KEYWORD = {
    {"False", KeywordType::False},
    {"await", KeywordType::Await},
    {"else", KeywordType::Else},
    {"import", KeywordType::Import},
    {"pass", KeywordType::Pass},
    {"None", KeywordType::None},
    {"break", KeywordType::Break},
    {"except", KeywordType::Except},
    {"in", KeywordType::In},
    {"raise", KeywordType::Raise},
    {"True", KeywordType::True},
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
    {"int", KeywordType::Int},
    {"float", KeywordType::Float},
    {"bool", KeywordType::Bool},
    {"range", KeywordType::Range},
};

constexpr std::size_t DEFAULT_INDENT_SIZE = 4;

} // namespace

Tokenizer::Tokenizer(std::string &&Source, const std::size_t IndentSize)
    : Source_(std::move(Source)), IndentSize_{IndentSize} {
  if (IndentSize % 2 != 0) {
    throw std::invalid_argument(std::format("Indent size {} is not a multiple of 2", IndentSize));
  }
  NextToken_ = consumeNextToken();
  // Initialization of NextToken_ with consumeNextToken increments the position.
  // Bring it back to 0.
  CurrPos_ = 0;
  CurrLocation_ = {};
}

Tokenizer::Tokenizer(std::string &&Source)
    : compyled::lexer::Tokenizer(std::move(Source), DEFAULT_INDENT_SIZE) {}

Token Tokenizer::peekNextToken() const noexcept { return NextToken_; }

Token Tokenizer::consumeNextToken() {
  if (CurrPos_ == Source_.size()) {
    return updateNextToken({.Type = TokenType::Eof, .Location = CurrLocation_});
  } else if (CurrPos_ > Source_.size()) {
    throw error::CompyledException("Cannot read past the end of source", CurrLocation_);
  }

  const auto StartLocation = CurrLocation_;
  const auto StartPos = CurrPos_;
  while (Source_[CurrPos_] == ' ' && (CurrPos_ - StartPos) < IndentSize_) {
    CurrPos_++;
    CurrLocation_.Column++;
  }

  auto WhitespaceCount = CurrPos_ - StartPos;
  if (WhitespaceCount == 1) {
    return updateNextToken({.Type = TokenType::Whitespace, .Location = StartLocation});
  } else if (WhitespaceCount == IndentSize_) {
    return updateNextToken({.Type = TokenType::Indent, .Location = StartLocation});
  } else if (WhitespaceCount > 0 && WhitespaceCount < IndentSize_) {
    throw error::CompyledException(
        std::format("Unexpected number of whitespaces '{}'", WhitespaceCount), CurrLocation_);
  }

  if (Source_[CurrPos_] == '\n') {
    CurrPos_++;
    CurrLocation_.Line++;
    CurrLocation_.Column = 0;
    return updateNextToken({.Type = TokenType::NewLine, .Location = StartLocation});
  }

  const auto RemainingSource = std::string_view{Source_.cbegin() + CurrPos_, Source_.cend()};

  const auto OperatorOrDelimiterType = detail::getOperatorOrDelimiterType(RemainingSource);
  if (OperatorOrDelimiterType) {
    CurrPos_ += OperatorOrDelimiterType->second;
    CurrLocation_.Column += OperatorOrDelimiterType->second;
    if (std::holds_alternative<OperatorType>(OperatorOrDelimiterType->first)) {
      return updateNextToken({.Type = TokenType::Operator,
                              .Location = StartLocation,
                              .Value = std::get<OperatorType>(OperatorOrDelimiterType->first)});
    }
    return updateNextToken({.Type = TokenType::Delimiter,
                            .Location = StartLocation,
                            .Value = std::get<DelimiterType>(OperatorOrDelimiterType->first)});
  }

  const auto Name = detail::getName(RemainingSource);
  if (Name) {
    CurrPos_ += Name->size();
    CurrLocation_.Column += Name->size();
    const auto it = STRING_TO_KEYWORD.find(*Name);
    if (it != STRING_TO_KEYWORD.cend()) {
      switch (it->second) {
      case KeywordType::And:
        return updateNextToken({.Type = TokenType::Operator,
                                .Location = StartLocation,
                                .Value = OperatorType::LogicalAnd});
      case KeywordType::Or:
        return updateNextToken({.Type = TokenType::Operator,
                                .Location = StartLocation,
                                .Value = OperatorType::LogicalOr});
      case KeywordType::Not:
        return updateNextToken({.Type = TokenType::Operator,
                                .Location = StartLocation,
                                .Value = OperatorType::LogicalNot});
      case KeywordType::True:
        return updateNextToken(
            {.Type = TokenType::Value, .Location = StartLocation, .Value = true});
      case KeywordType::False:
        return updateNextToken(
            {.Type = TokenType::Value, .Location = StartLocation, .Value = false});
      case KeywordType::Int:
        return updateNextToken(
            {.Type = TokenType::BaseType, .Location = StartLocation, .Value = BaseType::Integer});
      case KeywordType::Float:
        return updateNextToken(
            {.Type = TokenType::BaseType, .Location = StartLocation, .Value = BaseType::Float});
      case KeywordType::Bool:
        return updateNextToken(
            {.Type = TokenType::BaseType, .Location = StartLocation, .Value = BaseType::Boolean});
      default:
        // No other keywords are operators or booleans and don't need special treatment.
      }
      return updateNextToken(
          {.Type = TokenType::Keyword, .Location = StartLocation, .Value = it->second});
    } else {
      return updateNextToken(
          {.Type = TokenType::Identifier, .Location = StartLocation, .Value = std::move(*Name)});
    }
  }

  const auto FloatLiteral = detail::getFloatLiteral(RemainingSource);
  if (FloatLiteral) {
    CurrPos_ += FloatLiteral->second;
    CurrLocation_.Column += FloatLiteral->second;
    return updateNextToken(
        {.Type = TokenType::Value, .Location = StartLocation, .Value = FloatLiteral->first});
  }

  const auto IntegerLiteral = detail::getIntegerLiteral(RemainingSource);
  if (IntegerLiteral) {
    CurrPos_ += IntegerLiteral->second;
    CurrLocation_.Column += IntegerLiteral->second;
    if (IntegerLiteral->first > std::numeric_limits<types::IntegerType>::max()) {
      throw error::CompyledException(
          std::format("Integer '{}' exceeds the maximum 64-bit integer of {}",
                      IntegerLiteral->first, std::numeric_limits<types::IntegerType>::max()),
          CurrLocation_);
    }
    return updateNextToken(
        {.Type = TokenType::Value, .Location = StartLocation, .Value = IntegerLiteral->first});
  }

  throw error::CompyledException(std::format("Unexpected character '{}'", Source_[CurrPos_]),
                                 CurrLocation_);
}

Token Tokenizer::updateNextToken(Token &&NewToken) noexcept {
  const auto PrevToken = NextToken_;
  NextToken_ = std::move(NewToken);
  return PrevToken;
}

namespace detail {

namespace {

std::optional<std::pair<std::string, std::size_t>> matchDigitPart(const std::string_view Source) {
  auto DigitPart = std::string{};
  auto CurrPos = 0uz;
  if (std::isdigit(Source[CurrPos])) {
    DigitPart += Source[CurrPos];
    CurrPos++;
    while (CurrPos < Source.size()) {
      if (Source[CurrPos] == '_') {
        if (Source[CurrPos - 1] == '_') {
          return std::nullopt;
        }
        // Don't add to DigitPart because underscores are not valid C++ numeric literals
        CurrPos++;
      }
      if (CurrPos < Source.size()) {
        if (!std::isdigit(Source[CurrPos])) {
          return std::pair{DigitPart, CurrPos};
        }

        DigitPart += Source[CurrPos];
        CurrPos++;
      }
    }
    return std::pair{DigitPart, CurrPos};
  }
  return std::nullopt;
}

std::optional<std::pair<std::string, std::size_t>> matchExponent(const std::string_view Source) {
  auto Exponent = std::string{};
  auto CurrPos = 0uz;
  if (Source[CurrPos] == 'e' || Source[CurrPos] == 'E') {
    Exponent += Source[CurrPos];
    CurrPos++;
    if (CurrPos < Source.size() && (Source[CurrPos] == '+' || Source[CurrPos] == '-')) {
      Exponent += Source[CurrPos];
      CurrPos++;
    }

    if (CurrPos < Source.size()) {
      const auto DigitPartAndCount = matchDigitPart({Source.cbegin() + CurrPos, Source.cend()});
      if (DigitPartAndCount) {
        Exponent += DigitPartAndCount->first;
        CurrPos += DigitPartAndCount->second;
        return std::pair{Exponent, CurrPos};
      }
    }
  }
  return std::nullopt;
}

} // namespace

// TODO: Implement actual integer parsing as per
// https://docs.python.org/3/reference/lexical_analysis.html#integer-literals
std::optional<std::pair<types::IntegerType, std::size_t>>
getIntegerLiteral(const std::string_view Source) {
  if (Source.empty()) {
    return std::nullopt;
  }

  auto IntValue = types::IntegerType{};
  const auto [ptr, ec] = std::from_chars(Source.cbegin(), Source.cend(), IntValue);
  if (ec == std::errc{}) {
    return std::pair{IntValue, std::distance(Source.cbegin(), ptr)};
  }

  return std::nullopt;
}

// From https://docs.python.org/3/reference/lexical_analysis.html#floating-point-literals
/*
 * floatnumber:
 *    | digitpart "." [digitpart] [exponent]
 *    | "." digitpart [exponent]
 *    | digitpart exponent
 * digitpart: digit (["_"] digit)*
 * exponent:  ("e" | "E") ["+" | "-"] digitpart
 */
std::optional<std::pair<types::FloatType, std::size_t>>
getFloatLiteral(const std::string_view Source) {
  if (Source.empty()) {
    return std::nullopt;
  }

  auto CurrPos = 0uz;
  auto FloatStr = std::string{};

  if (Source[CurrPos] == '.') {
    // | "." digitpart [exponent]
    FloatStr += '.';
    CurrPos++;
    if (CurrPos < Source.size()) {
      const auto DigitPartAndCount = matchDigitPart({Source.cbegin() + CurrPos, Source.cend()});
      if (!DigitPartAndCount) {
        return std::nullopt;
      }
      FloatStr += DigitPartAndCount->first;
      CurrPos += DigitPartAndCount->second;
    } else {
      return std::nullopt;
    }

    if (CurrPos < Source.size()) {
      const auto ExponentAndCount = matchExponent({Source.cbegin() + CurrPos, Source.cend()});
      if (ExponentAndCount) {
        FloatStr += ExponentAndCount->first;
        CurrPos += ExponentAndCount->second;
      }
    }
  } else if (const auto DigitPartAndCount =
                 matchDigitPart({Source.cbegin() + CurrPos, Source.cend()})) {
    FloatStr += DigitPartAndCount->first;
    CurrPos += DigitPartAndCount->second;

    if (CurrPos < Source.size()) {
      if (Source[CurrPos] == '.') {
        // | digitpart "." [digitpart] [exponent]
        FloatStr += '.';
        CurrPos++;
        if (CurrPos < Source.size()) {
          const auto DigitPartAndCount = matchDigitPart({Source.cbegin() + CurrPos, Source.cend()});
          if (DigitPartAndCount) {
            FloatStr += DigitPartAndCount->first;
            CurrPos += DigitPartAndCount->second;
          }
        }

        if (CurrPos < Source.size()) {
          const auto ExponentAndCount = matchExponent({Source.cbegin() + CurrPos, Source.cend()});
          if (ExponentAndCount) {
            FloatStr += ExponentAndCount->first;
            CurrPos += ExponentAndCount->second;
          }
        }
      } else if (const auto ExponentAndCount =
                     matchExponent({Source.cbegin() + CurrPos, Source.cend()})) {
        // | digitpart exponent
        FloatStr += ExponentAndCount->first;
        CurrPos += ExponentAndCount->second;
      } else {
        return std::nullopt;
      }
    } else {
      return std::nullopt;
    }
  } else {
    return std::nullopt;
  }

  auto FloatValue = types::FloatType{};
  const auto [ptr, ec] =
      std::from_chars(FloatStr.data(), FloatStr.data() + FloatStr.size(), FloatValue);
  if (ec == std::errc{}) {
    return std::pair{FloatValue, CurrPos};
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
getOperatorOrDelimiterType(const std::string_view Source) {
  auto CurrPos = 0uz;
  switch (Source[CurrPos]) {
  case '+': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::UnaryAdd, CurrPos};
    }
    return std::pair{OperatorType::Add, CurrPos};
  }
  case '-': {
    CurrPos++;
    if (CurrPos < Source.size()) {
      if (Source[CurrPos] == '=') {
        CurrPos++;
        return std::pair{OperatorType::UnarySubtract, CurrPos};
      } else if (Source[CurrPos] == '>') {
        CurrPos++;
        return std::pair{DelimiterType::Arrow, CurrPos};
      }
    }
    return std::pair{OperatorType::Subtract, CurrPos};
  }
  case '*': {
    CurrPos++;
    if (CurrPos < Source.size()) {
      if (Source[CurrPos] == '*') {
        CurrPos++;
        if (CurrPos < Source.size() && Source[CurrPos] == '=') {
          CurrPos++;
          return std::pair{OperatorType::UnaryExponentiate, CurrPos};
        }
        return std::pair{OperatorType::Exponentiate, CurrPos};
      } else if (Source[CurrPos] == '=') {
        CurrPos++;
        return std::pair{OperatorType::UnaryMultiply, CurrPos};
      }
    }
    return std::pair{OperatorType::Multiply, CurrPos};
  }
  case '/': {
    CurrPos++;
    if (CurrPos < Source.size()) {
      if (Source[CurrPos] == '/') {
        CurrPos++;
        if (CurrPos < Source.size() && Source[CurrPos] == '=') {
          CurrPos++;
          return std::pair{OperatorType::UnaryIntegerDivide, CurrPos};
        }
        return std::pair{OperatorType::IntegerDivide, CurrPos};
      } else if (Source[CurrPos] == '=') {
        CurrPos++;
        return std::pair{OperatorType::UnaryFloatDivide, CurrPos};
      }
    }
    return std::pair{OperatorType::FloatDivide, CurrPos};
  }
  case '%': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::UnaryModulo, CurrPos};
    }
    return std::pair{OperatorType::Modulo, CurrPos};
  }
  case '&': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::UnaryBitwiseAnd, CurrPos};
    }
    return std::pair{OperatorType::BitwiseAnd, CurrPos};
  }
  case '|': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::UnaryBitwiseOr, CurrPos};
    }
    return std::pair{OperatorType::BitwiseOr, CurrPos};
  }
  case '^': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::UnaryBitwiseXor, CurrPos};
    }
    return std::pair{OperatorType::BitwiseXor, CurrPos};
  }
  case '<': {
    CurrPos++;
    if (CurrPos < Source.size()) {
      if (Source[CurrPos] == '<') {
        CurrPos++;
        if (CurrPos < Source.size() && Source[CurrPos] == '=') {
          CurrPos++;
          return std::pair{OperatorType::UnaryLeftShift, CurrPos};
        }
        return std::pair{OperatorType::LeftShift, CurrPos};
      } else if (Source[CurrPos] == '=') {
        CurrPos++;
        return std::pair{OperatorType::LessOrEqual, CurrPos};
      }
      return std::pair{OperatorType::Less, CurrPos};
    }
    return std::pair{OperatorType::Less, CurrPos};
  }
  case '>': {
    CurrPos++;
    if (CurrPos < Source.size()) {
      if (Source[CurrPos] == '>') {
        CurrPos++;
        if (CurrPos < Source.size() && Source[CurrPos] == '=') {
          CurrPos++;
          return std::pair{OperatorType::UnaryRightShift, CurrPos};
        }
        return std::pair{OperatorType::RightShift, CurrPos};
      } else if (Source[CurrPos] == '=') {
        CurrPos++;
        return std::pair{OperatorType::GreaterOrEqual, CurrPos};
      }
    }
    return std::pair{OperatorType::Greater, CurrPos};
  }
  case '~': {
    CurrPos++;
    return std::pair{OperatorType::BitwiseInvert, CurrPos};
  }
  case '=': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::Equal, CurrPos};
    }
    return std::pair{OperatorType::Assign, CurrPos};
  }
  case '!': {
    CurrPos++;
    if (CurrPos < Source.size() && Source[CurrPos] == '=') {
      CurrPos++;
      return std::pair{OperatorType::NotEqual, CurrPos};
    }
    return std::pair{DelimiterType::Exclamation, CurrPos};
  }
  case '(': {
    CurrPos++;
    return std::pair{DelimiterType::LeftParenthesis, CurrPos};
  }
  case ')': {
    CurrPos++;
    return std::pair{DelimiterType::RightParenthesis, CurrPos};
  }
  case '[': {
    CurrPos++;
    return std::pair{DelimiterType::LeftBracket, CurrPos};
  }
  case ']': {
    CurrPos++;
    return std::pair{DelimiterType::RightBracket, CurrPos};
  }
  case '{': {
    CurrPos++;
    return std::pair{DelimiterType::LeftBraces, CurrPos};
  }
  case '}': {
    CurrPos++;
    return std::pair{DelimiterType::RightBraces, CurrPos};
  }
  case ',': {
    CurrPos++;
    return std::pair{DelimiterType::Comma, CurrPos};
  }
  case ':': {
    CurrPos++;
    return std::pair{DelimiterType::Colon, CurrPos};
  }
  case ';': {
    CurrPos++;
    return std::pair{DelimiterType::Semicolon, CurrPos};
  }
  default:
    return std::nullopt;
  };
  std::unreachable();
}

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