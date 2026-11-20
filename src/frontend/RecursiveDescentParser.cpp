#include "RecursiveDescentParser.h"
#include "frontend/AST.h"
#include "frontend/Tokenizer.h"
#include <cassert>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace compyled::parser {

RecursiveDescentParser::RecursiveDescentParser(lexer::Tokenizer &Tokenizer)
    : Tokenizer_(Tokenizer) {}

std::unique_ptr<ast::ASTNode> RecursiveDescentParser::BuildAST() {
  auto Root = std::make_unique<ast::ASTNode>();
  auto Token = Tokenizer_.getNextToken();
  while (Token) {
    if (!Token) {
      return nullptr;
    }

    if (Token->Type == lexer::TokenType::Eof) {
      return Root;
    }
  }

  return Root;
}

namespace detail {

bool isDelimiter(const std::expected<lexer::Token, std::string> &Token,
                 const lexer::DelimiterType ExpectedType) {
  return Token && Token->Type != lexer::TokenType::Delimiter &&
         std::get<lexer::DelimiterType>(Token->Value) != ExpectedType;
}

bool isOperator(const std::expected<lexer::Token, std::string> &Token,
                const lexer::OperatorType ExpectedType) {
  return Token && Token->Type != lexer::TokenType::Operator &&
         std::get<lexer::OperatorType>(Token->Value) != ExpectedType;
}

std::unique_ptr<ast::ASTNode> parseSum(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseTerm(Tokenizer);

  while (auto Operator = Tokenizer.getNextToken()) {
    if (!isOperator(Operator, lexer::OperatorType::Add) ||
        !isOperator(Operator, lexer::OperatorType::Subtract)) {
      break;
    }

    auto Rhs = parseTerm(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Operator->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseTerm(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseFactor(Tokenizer);

  while (auto Operator = Tokenizer.getNextToken()) {
    if (!isOperator(Operator, lexer::OperatorType::Multiply) ||
        !isOperator(Operator, lexer::OperatorType::FloatDivide) ||
        !isOperator(Operator, lexer::OperatorType::IntegerDivide) ||
        !isOperator(Operator, lexer::OperatorType::Modulo)) {
      break;
    }

    auto Rhs = parseFactor(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Operator->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseFactor(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!Token)
    return nullptr;

  if (isOperator(Token, lexer::OperatorType::Add) ||
      isOperator(Token, lexer::OperatorType::Subtract) ||
      isOperator(Token, lexer::OperatorType::BitwiseInvert)) {
    return parseFactor(Tokenizer);
  }

  return parsePower(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parsePower(lexer::Tokenizer &Tokenizer) {
  auto Primary = parsePrimary(Tokenizer);
  if (!Primary)
    return nullptr;

  const auto PowerOp = Tokenizer.getNextToken();
  if (!isOperator(PowerOp, lexer::OperatorType::Exponentiate)) {
    return nullptr;
  }

  auto Factor = parseFactor(Tokenizer);
  return std::make_unique<ast::BinaryOperation>(std::move(Primary), std::move(Factor),
                                                lexer::OperatorType::Exponentiate);
}

std::unique_ptr<ast::ASTNode> parsePrimary(lexer::Tokenizer &Tokenizer) {
  auto Atom = parseAtom(Tokenizer);
  if (!Atom) {
    return nullptr;
  }

  if (auto *Identifier = dynamic_cast<ast::Identifier *>(Atom.get())) {
    auto Token = Tokenizer.getNextToken();
    if (!isDelimiter(Token, lexer::DelimiterType::LeftParenthesis)) {
      return Atom;
    }

    auto Arguments = parseArguments(Tokenizer);

    Token = Tokenizer.getNextToken();
    if (!isDelimiter(Token, lexer::DelimiterType::RightParenthesis)) {
      return nullptr;
    }

    return std::make_unique<ast::FunctionCall>(std::move(Arguments));
  }

  return Atom;
}

std::vector<std::unique_ptr<ast::ASTNode>> parseArguments(lexer::Tokenizer &Tokenizer) {
  auto Expr = parseExpression(Tokenizer);
  if (!Expr) {
    // TODO: Return unexpected
    return {};
  }

  auto Args = std::vector<std::unique_ptr<ast::ASTNode>>{};
  Args.push_back(std::move(Expr));

  auto Token = Tokenizer.getNextToken();
  while (isDelimiter(Token, lexer::DelimiterType::Comma)) {
    Expr = parseExpression(Tokenizer);
    if (!Expr) {
      // TODO: Return unexpected
      return {};
    }

    Args.push_back(std::move(Expr));
  }

  return Args;
}

std::unique_ptr<ast::ASTNode> parseAtom(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!Token)
    return nullptr;

  switch (Token->Type) {
  case compyled::lexer::TokenType::Boolean:
    assert(std::holds_alternative<lexer::BooleanType>(Token->Value));
    return std::make_unique<ast::Value<lexer::BooleanType>>(
        std::get<lexer::BooleanType>(Token->Value));
  case compyled::lexer::TokenType::Integer:
    assert(std::holds_alternative<lexer::IntegerType>(Token->Value));
    return std::make_unique<ast::Value<lexer::IntegerType>>(
        std::get<lexer::IntegerType>(Token->Value));
  case compyled::lexer::TokenType::Float:
    assert(std::holds_alternative<lexer::FloatType>(Token->Value));
    return std::make_unique<ast::Value<lexer::FloatType>>(std::get<lexer::FloatType>(Token->Value));
  case compyled::lexer::TokenType::Identifier:
    assert(std::holds_alternative<std::string>(Token->Value));
    return std::make_unique<ast::Identifier>(std::move(std::get<std::string>(Token->Value)));
  case compyled::lexer::TokenType::Delimiter:
    return parseGroup(Tokenizer, std::move(*Token));
  default:
    return nullptr;
  }

  std::unreachable();
}

std::unique_ptr<ast::ASTNode> parseGroup(lexer::Tokenizer &Tokenizer, lexer::Token &&FirstToken) {
  assert(std::holds_alternative<lexer::DelimiterType>(FirstToken.Value));

  if (FirstToken.Type != lexer::TokenType::Delimiter ||
      std::get<lexer::DelimiterType>(FirstToken.Value) != lexer::DelimiterType::LeftParenthesis) {
    return nullptr;
  }

  auto Expression = parseExpression(Tokenizer);

  const auto Token = Tokenizer.getNextToken();
  if (!Token || Token->Type != lexer::TokenType::Delimiter ||
      std::get<lexer::DelimiterType>(Token->Value) != lexer::DelimiterType::RightParenthesis) {
    return nullptr;
  }

  return Expression;
}

} // namespace detail

} // namespace compyled::parser
