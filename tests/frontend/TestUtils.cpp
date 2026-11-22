#include "TestUtils.h"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <variant>

namespace compyled::test {

using namespace compyled::lexer;

Expected Id(std::string Name) { return {TokenType::Identifier, std::move(Name)}; }
Expected Kw(KeywordType K) { return {TokenType::Keyword, K}; }
Expected Op(OperatorType O) { return {TokenType::Operator, O}; }
Expected Delim(DelimiterType D) { return {TokenType::Delimiter, D}; }
Expected Base(BaseType B) { return {TokenType::BaseType, B}; }
Expected Int(types::IntegerType V) { return {TokenType::Value, V}; }
Expected Flt(types::FloatType V) { return {TokenType::Value, V}; }
Expected Bool(types::BooleanType V) { return {TokenType::Value, V}; }
Expected NL() { return {TokenType::NewLine}; }
Expected Indent(std::size_t S) { return {TokenType::Indent, S}; }
Expected Dedent(std::size_t S) { return {TokenType::Dedent, S}; }
Expected Eof() { return {TokenType::Eof}; }

bool matches(const Token &Actual, const Expected &Want) {
  return Actual.Type == Want.Type && Actual.Value == Want.Value;
}

std::string toString(TokenType Type) {
  switch (Type) {
  case TokenType::BaseType:
    return "BaseType";
  case TokenType::Value:
    return "Value";
  case TokenType::Keyword:
    return "Keyword";
  case TokenType::Operator:
    return "Operator";
  case TokenType::Delimiter:
    return "Delimiter";
  case TokenType::Indent:
    return "Indent";
  case TokenType::Dedent:
    return "Dedent";
  case TokenType::NewLine:
    return "NewLine";
  case TokenType::Identifier:
    return "Identifier";
  case TokenType::Whitespace:
    return "Whitespace";
  case TokenType::Eof:
    return "Eof";
  }
  return "<unknown>";
}

std::string toString(const Token::ValueType &Value) {
  struct Visitor {
    std::string operator()(const std::string &S) const { return "\"" + S + "\""; }
    std::string operator()(types::IntegerType V) const { return "int " + std::to_string(V); }
    std::string operator()(types::FloatType V) const { return "float " + std::to_string(V); }
    std::string operator()(types::BooleanType V) const { return V ? "True" : "False"; }
    std::string operator()(std::size_t V) const { return "size " + std::to_string(V); }
    std::string operator()(OperatorType V) const {
      return "OperatorType " + std::to_string(static_cast<int>(V));
    }
    std::string operator()(KeywordType V) const {
      return "KeywordType " + std::to_string(static_cast<int>(V));
    }
    std::string operator()(DelimiterType V) const {
      return "DelimiterType " + std::to_string(static_cast<int>(V));
    }
    std::string operator()(BaseType V) const {
      return "BaseType " + std::to_string(static_cast<int>(V));
    }
  };
  return std::visit(Visitor{}, Value);
}

std::string toString(const Expected &E) { return toString(E.Type) + "(" + toString(E.Value) + ")"; }

std::string toString(const Token &T) {
  return toString(T.Type) + "(" + toString(T.Value) + ") at " + std::to_string(T.Location.Line) +
         ":" + std::to_string(T.Location.Column);
}

std::vector<Token> tokenize(std::string Source) {
  Tokenizer T(std::move(Source));
  std::vector<Token> Tokens;
  while (Tokens.size() < MAX_TOKENS) {
    Tokens.push_back(T.consumeNextToken());
    if (Tokens.back().Type == TokenType::Eof) {
      return Tokens;
    }
  }
  throw std::runtime_error("Tokenizer did not reach Eof");
}

} // namespace compyled::test
