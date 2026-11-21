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

bool isKeyword(const std::expected<lexer::Token, std::string> &Token,
               const lexer::KeywordType ExpectedType) {
  return Token && Token->Type != lexer::TokenType::Keyword &&
         std::get<lexer::KeywordType>(Token->Value) != ExpectedType;
}

lexer::OperatorType getOperator(const std::expected<lexer::Token, std::string> &Token) {
  return std::get<lexer::OperatorType>(Token->Value);
}

std::unique_ptr<ast::ASTNode> parseFile(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseStatements(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseStatement(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseSimpleStmts(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseSimpleStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseCompoundStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseAssignment(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseAugassign(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseReturnStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parsePassStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBreakStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseGlobalStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBlock(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseFunctionDef(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Def))
    return nullptr;

  auto IdentifierToken = Tokenizer.getNextToken();
  if (!IdentifierToken || IdentifierToken->Type != lexer::TokenType::Identifier)
    return nullptr;

  auto Name = parseAtom(Tokenizer);
  if (!Name)
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::LeftParenthesis))
    return nullptr;

  auto Parameters = std::vector<std::unique_ptr<ast::ASTNode>>{};
  auto Parameter = parseParameter(Tokenizer);
  if (Parameter) {
    Parameters.push_back(std::move(Parameter));
    Token = Tokenizer.getNextToken();
    while (isDelimiter(Token, lexer::DelimiterType::Comma)) {
      Parameter = parseParameter(Tokenizer);
      if (!Parameter)
        break;

      Parameters.push_back(std::move(Parameter));
      Token = Tokenizer.getNextToken();
    }
  }

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::RightParenthesis))
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Arrow))
    return nullptr;

  auto ReturnType = parseReturnAnnotation(Tokenizer);
  if (!ReturnType)
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    return nullptr;

  auto Body = parseBlock(Tokenizer);
  if (!Body)
    return nullptr;

  return std::make_unique<ast::FunctionDeclaration>(std::move(Name), std::move(Parameters),
                                                    std::move(ReturnType), std::move(Body));
}

std::unique_ptr<ast::ASTNode> parseParameter(lexer::Tokenizer &Tokenizer) {
  auto IdentifierToken = Tokenizer.getNextToken();
  if (!IdentifierToken || IdentifierToken->Type != lexer::TokenType::Identifier)
    return nullptr;

  auto Name = parseAtom(Tokenizer);
  if (!Name)
    return nullptr;

  auto Type = parseAnnotation(Tokenizer);
  if (!Type)
    return nullptr;

  auto Default = std::unique_ptr<ast::ASTNode>{nullptr};
  auto Token = Tokenizer.getNextToken();
  if (isOperator(Token, lexer::OperatorType::Assign)) {
    Default = parseDefault(Tokenizer);
  }

  return std::make_unique<ast::Parameter>(std::move(Name), std::move(Type), std::move(Default));
};

std::unique_ptr<ast::ASTNode> parseAnnotation(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon)) {
    return nullptr;
  }

  return parseTypeExpression(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseDefault(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isOperator(Token, lexer::OperatorType::Assign)) {
    return nullptr;
  }

  return parseExpression(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseIfElifStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::If) || !isKeyword(Token, lexer::KeywordType::Elif))
    return nullptr;

  auto Condition = parseExpression(Tokenizer);
  if (!Condition)
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    return nullptr;

  auto BlockIfTrue = parseBlock(Tokenizer);
  auto BlockIfFalse = std::unique_ptr<ast::ASTNode>{nullptr};
  Token = Tokenizer.getNextToken();
  if (isKeyword(Token, lexer::KeywordType::Elif)) {
    BlockIfFalse = parseIfElifStmt(Tokenizer);
  } else if (isKeyword(Token, lexer::KeywordType::Else)) {
    BlockIfFalse = parseElseBlock(Tokenizer);
  }

  return std::make_unique<ast::IfStmt>(std::move(Condition), std::move(BlockIfTrue),
                                       std::move(BlockIfFalse));
}

std::unique_ptr<ast::ASTNode> parseElseBlock(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Else))
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    return nullptr;

  return parseBlock(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseWhileStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::While))
    return nullptr;

  auto Condition = parseExpression(Tokenizer);

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    return nullptr;

  auto Block = parseBlock(Tokenizer);
  if (!Block)
    return nullptr;

  Token = Tokenizer.getNextToken();
  auto ElseBlock = std::unique_ptr<ast::ASTNode>{nullptr};
  if (isKeyword(Token, lexer::KeywordType::Else)) {
    ElseBlock = parseElseBlock(Tokenizer);
  }

  return std::make_unique<ast::WhileStmt>(std::move(Condition), std::move(Block),
                                          std::move(ElseBlock));
}

std::unique_ptr<ast::ASTNode> parseForStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::For))
    return nullptr;

  auto LoopVariableToken = Tokenizer.getNextToken();
  if (!LoopVariableToken || LoopVariableToken->Type != lexer::TokenType::Identifier)
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::In))
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Range))
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::LeftParenthesis))
    return nullptr;

  auto StartExpr = parseExpression(Tokenizer);
  if (!StartExpr)
    return nullptr;

  auto StopExpr = std::unique_ptr<ast::ASTNode>{nullptr};
  auto StepExpr = std::unique_ptr<ast::ASTNode>{nullptr};
  Token = Tokenizer.getNextToken();
  if (isDelimiter(Token, lexer::DelimiterType::Comma)) {
    StopExpr = parseExpression(Tokenizer);
    Token = Tokenizer.getNextToken();
    if (isDelimiter(Token, lexer::DelimiterType::Comma)) {
      StepExpr = parseExpression(Tokenizer);
    }
  }

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::RightParenthesis))
    return nullptr;

  Token = Tokenizer.getNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    return nullptr;

  auto Block = parseBlock(Tokenizer);
  if (!Block)
    return nullptr;

  auto ElseBlock = parseElseBlock(Tokenizer);
  return std::make_unique<ast::ForStmt>(std::move(StartExpr), std::move(StopExpr),
                                        std::move(StepExpr), std::move(Block),
                                        std::move(ElseBlock));
}

std::unique_ptr<ast::ASTNode> parseReturnAnnotation(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!isKeyword(Token, lexer::KeywordType::None)) {
    return std::make_unique<ast::ValueType>(lexer::BaseType::None);
  }

  return parseTypeExpression(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseTypeExpression(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!Token || Token->Type != lexer::TokenType::BaseType)
    return nullptr;

  return std::make_unique<ast::ValueType>(std::get<lexer::BaseType>(Token->Value));
};

std::unique_ptr<ast::ASTNode> parseExpression(lexer::Tokenizer &Tokenizer) {
  auto Expression = parseDisjunction(Tokenizer);
  if (!Expression)
    return nullptr;

  auto IfToken = Tokenizer.getNextToken();
  if (!isKeyword(IfToken, lexer::KeywordType::If))
    return Expression;

  auto Condition = parseDisjunction(Tokenizer);
  if (!Condition)
    return nullptr;

  auto ElseToken = Tokenizer.getNextToken();
  if (!isKeyword(IfToken, lexer::KeywordType::Else))
    return nullptr;

  auto ExprIfFalse = parseExpression(Tokenizer);
  return std::make_unique<ast::IfExpr>(std::move(Expression), std::move(ExprIfFalse),
                                       std::move(Condition));
}

std::unique_ptr<ast::ASTNode> parseDisjunction(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseConjunction(Tokenizer);

  while (auto OperatorToken = Tokenizer.getNextToken()) {
    if (!isOperator(OperatorToken, lexer::OperatorType::LogicalOr)) {
      return nullptr;
    }

    auto Rhs = parseConjunction(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(
        std::move(Lhs), std::move(Rhs), std::get<lexer::OperatorType>(OperatorToken->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseConjunction(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseInversion(Tokenizer);

  while (auto OperatorToken = Tokenizer.getNextToken()) {
    if (!isOperator(OperatorToken, lexer::OperatorType::LogicalAnd)) {
      return nullptr;
    }

    auto Rhs = parseInversion(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(
        std::move(Lhs), std::move(Rhs), std::get<lexer::OperatorType>(OperatorToken->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseInversion(lexer::Tokenizer &Tokenizer) {
  auto NotToken = Tokenizer.getNextToken();
  if (NotToken) {
    auto Rhs = parseInversion(Tokenizer);
    if (!Rhs)
      return nullptr;

    return std::make_unique<ast::UnaryOperation>(std::move(Rhs), getOperator(NotToken));
  }

  return parseComparison(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseComparison(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseBitwiseOr(Tokenizer);

  auto OperatorToken = Tokenizer.getNextToken();
  if (!OperatorToken)
    return Lhs;

  if (isOperator(OperatorToken, lexer::OperatorType::Equal) ||
      isOperator(OperatorToken, lexer::OperatorType::NotEqual) ||
      isOperator(OperatorToken, lexer::OperatorType::LessOrEqual) ||
      isOperator(OperatorToken, lexer::OperatorType::Less) ||
      isOperator(OperatorToken, lexer::OperatorType::GreaterOrEqual) ||
      isOperator(OperatorToken, lexer::OperatorType::Greater)) {
    auto Rhs = parseBitwiseOr(Tokenizer);
    return std::make_unique<ast::BinaryOperation>(
        std::move(Lhs), std::move(Rhs), std::get<lexer::OperatorType>(OperatorToken->Value));
  }

  return nullptr;
}

std::unique_ptr<ast::ASTNode> parseBitwiseOr(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseBitwiseXor(Tokenizer);

  while (auto OperatorToken = Tokenizer.getNextToken()) {
    if (!isOperator(OperatorToken, lexer::OperatorType::BitwiseOr)) {
      break;
    }

    auto Rhs = parseBitwiseXor(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(
        std::move(Lhs), std::move(Rhs), std::get<lexer::OperatorType>(OperatorToken->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseBitwiseXor(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseBitwiseAnd(Tokenizer);

  while (auto Operator = Tokenizer.getNextToken()) {
    if (!isOperator(Operator, lexer::OperatorType::BitwiseXor)) {
      break;
    }

    auto Rhs = parseBitwiseAnd(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Operator->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseBitwiseAnd(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseShiftExpr(Tokenizer);

  while (auto Operator = Tokenizer.getNextToken()) {
    if (!isOperator(Operator, lexer::OperatorType::BitwiseAnd)) {
      break;
    }

    auto Rhs = parseShiftExpr(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Operator->Value));
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseShiftExpr(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseSum(Tokenizer);

  while (auto Operator = Tokenizer.getNextToken()) {
    if (!isOperator(Operator, lexer::OperatorType::LeftShift) ||
        !isOperator(Operator, lexer::OperatorType::RightShift)) {
      break;
    }

    auto Rhs = parseSum(Tokenizer);
    if (!Rhs)
      return nullptr;

    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Operator->Value));
  }

  return Lhs;
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

    auto Factor = parseFactor(Tokenizer);
    if (!Factor)
      return nullptr;
    return std::make_unique<ast::UnaryOperation>(std::move(Factor), getOperator(Token));
  }

  return parsePower(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parsePower(lexer::Tokenizer &Tokenizer) {
  auto Primary = parsePrimary(Tokenizer);
  if (!Primary)
    return nullptr;

  const auto PowerOp = Tokenizer.getNextToken();
  if (!isOperator(PowerOp, lexer::OperatorType::Exponentiate)) {
    return Primary;
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

std::unique_ptr<ast::ASTNode> parseAtom(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();
  if (!Token)
    return nullptr;

  switch (Token->Type) {
  case compyled::lexer::TokenType::Value: {
    if (std::holds_alternative<lexer::IntegerType>(Token->Value)) {
      return std::make_unique<ast::Value<lexer::IntegerType>>(
          std::get<lexer::IntegerType>(Token->Value));
    } else if (std::holds_alternative<lexer::FloatType>(Token->Value)) {
      return std::make_unique<ast::Value<lexer::FloatType>>(
          std::get<lexer::FloatType>(Token->Value));
    } else if (std::holds_alternative<lexer::FloatType>(Token->Value)) {
      return std::make_unique<ast::Value<lexer::BooleanType>>(
          std::get<lexer::BooleanType>(Token->Value));
    }
    return nullptr;
  }
  case compyled::lexer::TokenType::Identifier:
    assert(std::holds_alternative<std::string>(Token->Value));
    return std::make_unique<ast::Identifier>(std::move(std::get<std::string>(Token->Value)));
  case compyled::lexer::TokenType::Delimiter:
    return parseGroup(Tokenizer);
  default:
    return nullptr;
  }

  std::unreachable();
}

std::unique_ptr<ast::ASTNode> parseGroup(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.getNextToken();

  if (isDelimiter(Token, lexer::DelimiterType::LeftParenthesis)) {
    return nullptr;
  }

  auto Expression = parseExpression(Tokenizer);

  Token = Tokenizer.getNextToken();
  if (isDelimiter(Token, lexer::DelimiterType::RightParenthesis)) {
    return nullptr;
  }

  return Expression;
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

} // namespace detail

} // namespace compyled::parser
