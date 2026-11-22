#include "Parser.h"
#include "AST.h"
#include "Error.h"
#include "Tokenizer.h"
#include <cassert>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace compyled::parser {

bool isDelimiter(const lexer::Token &Token, const lexer::DelimiterType ExpectedType) {
  return Token.Type != lexer::TokenType::Delimiter &&
         std::get<lexer::DelimiterType>(Token.Value) != ExpectedType;
}

bool isOperator(const lexer::Token &Token, const lexer::OperatorType ExpectedType) {
  return Token.Type != lexer::TokenType::Operator &&
         std::get<lexer::OperatorType>(Token.Value) != ExpectedType;
}

bool isKeyword(const lexer::Token &Token, const lexer::KeywordType ExpectedType) {
  return Token.Type != lexer::TokenType::Keyword &&
         std::get<lexer::KeywordType>(Token.Value) != ExpectedType;
}

bool isAugassign(const lexer::Token &Token) {
  if (Token.Type != lexer::TokenType::Operator)
    return false;

  switch (std::get<lexer::OperatorType>(Token.Value)) {
  case lexer::OperatorType::UnaryAdd:
  case lexer::OperatorType::UnarySubtract:
  case lexer::OperatorType::UnaryMultiply:
  case lexer::OperatorType::UnaryFloatDivide:
  case lexer::OperatorType::UnaryModulo:
  case lexer::OperatorType::UnaryBitwiseAnd:
  case lexer::OperatorType::UnaryBitwiseOr:
  case lexer::OperatorType::UnaryBitwiseXor:
  case lexer::OperatorType::UnaryLeftShift:
  case lexer::OperatorType::UnaryRightShift:
  case lexer::OperatorType::UnaryExponentiate:
  case lexer::OperatorType::UnaryIntegerDivide:
    return true;
  default:
    return false;
  }

  return false;
}

lexer::OperatorType getOperator(const lexer::Token &Token) {
  return std::get<lexer::OperatorType>(Token.Value);
}

std::unique_ptr<ast::ASTNode> parseFile(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  auto Stmts = std::vector<std::unique_ptr<ast::ASTNode>>{};
  while (Token.Type != lexer::TokenType::Eof) {
    auto OtherStatements = parseStatement(Tokenizer);
    if (OtherStatements.empty())
      return nullptr;

    std::ranges::move(OtherStatements, std::back_inserter(Stmts));
    Token = Tokenizer.peekNextToken();
  }

  return std::make_unique<ast::Program>(std::move(Stmts));
}

std::vector<std::unique_ptr<ast::ASTNode>> parseStatement(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  if (isKeyword(Token, lexer::KeywordType::Def) || isKeyword(Token, lexer::KeywordType::If) ||
      isKeyword(Token, lexer::KeywordType::For) || isKeyword(Token, lexer::KeywordType::While)) {
    auto Stmts = std::vector<std::unique_ptr<ast::ASTNode>>{};
    auto Stmt = parseCompoundStmt(Tokenizer);
    Stmts.push_back(std::move(Stmt));
    return Stmts;
  }

  return parseSimpleStmts(Tokenizer);
}

std::vector<std::unique_ptr<ast::ASTNode>> parseSimpleStmts(lexer::Tokenizer &Tokenizer) {
  auto Stmts = std::vector<std::unique_ptr<ast::ASTNode>>{};
  auto SimpleStmt = parseSimpleStmt(Tokenizer);
  Stmts.push_back(std::move(SimpleStmt));
  auto Token = Tokenizer.peekNextToken();
  if (Token.Type == lexer::TokenType::NewLine) {
    return Stmts;
  }

  if (isDelimiter(Token, lexer::DelimiterType::Semicolon)) {
    Tokenizer.consumeNextToken();

    Token = Tokenizer.peekNextToken();
    if (Token.Type == lexer::TokenType::NewLine) {
      return Stmts;
    }

    auto AdditionalStmts = parseSimpleStmts(Tokenizer);
    std::ranges::move(AdditionalStmts, std::back_inserter(Stmts));
  }

  return Stmts;
}

std::unique_ptr<ast::ASTNode> parseSimpleStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  if (isKeyword(Token, lexer::KeywordType::Return)) {
    return parseReturnStmt(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::Pass)) {
    return parsePassStmt(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::Break)) {
    return parseBreakStmt(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::Continue)) {
    return parseContinueStmt(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::Global)) {
    return parseGlobalStmt(Tokenizer);
  }

  return parseAssignment(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseCompoundStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  if (isKeyword(Token, lexer::KeywordType::Def)) {
    return parseFunctionDef(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::If)) {
    return parseIfElifStmt(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::For)) {
    return parseForStmt(Tokenizer);
  }

  if (isKeyword(Token, lexer::KeywordType::While)) {
    return parseWhileStmt(Tokenizer);
  }

  throw error::CompyledException("Expected 'def', 'if', 'for', or 'while' tokens", Token.Location);
}

std::unique_ptr<ast::ASTNode> parseAssignment(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseExpression(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  if (isAugassign(Token)) {
    Tokenizer.consumeNextToken();
    const auto Operator = getOperator(Token);
    auto Rhs = parseExpression(Tokenizer);
    return std::make_unique<ast::Augassign>(std::move(Lhs), std::move(Rhs), Operator);
  }

  if (isDelimiter(Token, lexer::DelimiterType::Colon)) {
    auto Type = parseAnnotation(Tokenizer);

    Token = Tokenizer.peekNextToken();
    if (!isOperator(Token, lexer::OperatorType::Assign))
      throw error::CompyledException("Expected '=' token", Token.Location);

    auto Rhs = parseExpression(Tokenizer);
    return std::make_unique<ast::TypedAssignment>(std::move(Lhs), std::move(Rhs), std::move(Type));
  }

  if (!isOperator(Token, lexer::OperatorType::Assign))
    throw error::CompyledException("Expected '=' token", Token.Location);

  Tokenizer.consumeNextToken();

  auto Lhss = std::vector<std::unique_ptr<ast::ASTNode>>{};
  Lhss.push_back(std::move(Lhs));
  auto Rhs = parseExpression(Tokenizer);
  Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::Assign)) {
    Tokenizer.consumeNextToken();
    // For multi-assignment, what was previously on the lhs goes to the rhs.
    Lhss.push_back(std::move(Rhs));
    Rhs = parseExpression(Tokenizer);
    Token = Tokenizer.peekNextToken();
  }

  return std::make_unique<ast::Assignment>(std::move(Lhss), std::move(Rhs));
}

std::unique_ptr<ast::ASTNode> parseReturnStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Return))
    throw error::CompyledException("Expected 'return' token", Token.Location);

  auto Expression = parseExpression(Tokenizer);
  return std::make_unique<ast::ReturnStmt>(std::move(Expression));
}

std::unique_ptr<ast::ASTNode> parsePassStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Pass))
    throw error::CompyledException("Expected 'pass' token", Token.Location);

  return std::make_unique<ast::PassStmt>();
}

std::unique_ptr<ast::ASTNode> parseBreakStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Break))
    throw error::CompyledException("Expected 'break' token", Token.Location);

  return std::make_unique<ast::BreakStmt>();
}

std::unique_ptr<ast::ASTNode> parseContinueStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Continue))
    throw error::CompyledException("Expected 'continue' token", Token.Location);

  return std::make_unique<ast::ContinueStmt>();
}

std::unique_ptr<ast::ASTNode> parseGlobalStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Global))
    throw error::CompyledException("Expected 'global' token", Token.Location);

  auto Name = parseIdentifier(Tokenizer);
  auto Names = std::vector<std::unique_ptr<ast::ASTNode>>{};
  Names.push_back(std::move(Name));
  Token = Tokenizer.peekNextToken();
  while (isDelimiter(Token, lexer::DelimiterType::Comma)) {
    Tokenizer.consumeNextToken();
    Name = parseIdentifier(Tokenizer);
    Names.push_back(std::move(Name));
    Token = Tokenizer.peekNextToken();
  }

  return std::make_unique<ast::GlobalStmt>(std::move(Names));
}

std::unique_ptr<ast::ASTNode> parseBlock(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  if (Token.Type != lexer::TokenType::NewLine) {
    auto Statements = parseSimpleStmts(Tokenizer);
    return std::make_unique<ast::Block>(std::move(Statements));
  }

  Tokenizer.consumeNextToken();
  Token = Tokenizer.consumeNextToken();
  if (Token.Type != lexer::TokenType::Indent)
    throw error::CompyledException("Expected indent token", Token.Location);

  auto Statements = std::vector<std::unique_ptr<ast::ASTNode>>{};
  Token = Tokenizer.peekNextToken();
  while (Token.Type != lexer::TokenType::Dedent) {
    auto OtherStatements = parseStatement(Tokenizer);
    std::ranges::move(OtherStatements, std::back_inserter(Statements));
    Token = Tokenizer.peekNextToken();
  }
  Tokenizer.consumeNextToken();

  if (Token.Type != lexer::TokenType::Dedent)
    throw error::CompyledException("Expected dedent token", Token.Location);

  return std::make_unique<ast::Block>(std::move(Statements));
}

std::unique_ptr<ast::ASTNode> parseFunctionDef(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Def))
    throw error::CompyledException("Expected 'def' token", Token.Location);

  auto Name = parseIdentifier(Tokenizer);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::LeftParenthesis))
    throw error::CompyledException("Expected '(' token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  auto Parameters = std::vector<std::unique_ptr<ast::ASTNode>>{};
  if (!isDelimiter(Token, lexer::DelimiterType::LeftParenthesis)) {
    auto Parameter = parseParameter(Tokenizer);
    Parameters.push_back(std::move(Parameter));
    Token = Tokenizer.peekNextToken();
    while (isDelimiter(Token, lexer::DelimiterType::Comma)) {
      Tokenizer.consumeNextToken();
      Parameter = parseParameter(Tokenizer);
      Parameters.push_back(std::move(Parameter));
      Token = Tokenizer.peekNextToken();
    }
  }

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::RightParenthesis))
    throw error::CompyledException("Expected ')' token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Arrow))
    throw error::CompyledException("Expected '->' token", Token.Location);

  auto ReturnType = parseReturnAnnotation(Tokenizer);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    throw error::CompyledException("Expected ':' token", Token.Location);

  auto Body = parseBlock(Tokenizer);
  return std::make_unique<ast::FunctionDeclaration>(std::move(Name), std::move(Parameters),
                                                    std::move(ReturnType), std::move(Body));
}

std::unique_ptr<ast::ASTNode> parseParameter(lexer::Tokenizer &Tokenizer) {
  auto Name = parseIdentifier(Tokenizer);
  auto Type = parseAnnotation(Tokenizer);
  auto Default = std::unique_ptr<ast::ASTNode>{nullptr};
  auto Token = Tokenizer.peekNextToken();
  if (isOperator(Token, lexer::OperatorType::Assign)) {
    Tokenizer.consumeNextToken();
    Default = parseDefault(Tokenizer);
  }

  return std::make_unique<ast::Parameter>(std::move(Name), std::move(Type), std::move(Default));
};

std::unique_ptr<ast::ASTNode> parseAnnotation(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    throw error::CompyledException("Expected ':' token", Token.Location);

  return parseTypeExpression(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseDefault(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isOperator(Token, lexer::OperatorType::Assign))
    throw error::CompyledException("Expected '=' token", Token.Location);

  return parseExpression(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseIfElifStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::If) || !isKeyword(Token, lexer::KeywordType::Elif))
    throw error::CompyledException("Expected 'if' or 'elif' token", Token.Location);

  auto Condition = parseExpression(Tokenizer);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    throw error::CompyledException("Expected ':' token", Token.Location);

  auto BlockIfTrue = parseBlock(Tokenizer);

  auto BlockIfFalse = std::unique_ptr<ast::ASTNode>{nullptr};
  Token = Tokenizer.peekNextToken();
  if (isKeyword(Token, lexer::KeywordType::Elif)) {
    Tokenizer.consumeNextToken();
    BlockIfFalse = parseIfElifStmt(Tokenizer);
  } else if (isKeyword(Token, lexer::KeywordType::Else)) {
    Tokenizer.consumeNextToken();
    BlockIfFalse = parseElseBlock(Tokenizer);
  }

  return std::make_unique<ast::IfStmt>(std::move(Condition), std::move(BlockIfTrue),
                                       std::move(BlockIfFalse));
}

std::unique_ptr<ast::ASTNode> parseElseBlock(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Else))
    throw error::CompyledException("Expected 'else' token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    throw error::CompyledException("Expected ':' token", Token.Location);

  return parseBlock(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseWhileStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::While))
    throw error::CompyledException("Expected 'while' token", Token.Location);

  auto Condition = parseExpression(Tokenizer);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    throw error::CompyledException("Expected ':' token", Token.Location);

  auto Block = parseBlock(Tokenizer);

  Token = Tokenizer.peekNextToken();
  auto ElseBlock = std::unique_ptr<ast::ASTNode>{nullptr};
  if (isKeyword(Token, lexer::KeywordType::Else)) {
    Tokenizer.consumeNextToken();
    ElseBlock = parseElseBlock(Tokenizer);
  }

  return std::make_unique<ast::WhileStmt>(std::move(Condition), std::move(Block),
                                          std::move(ElseBlock));
}

std::unique_ptr<ast::ASTNode> parseForStmt(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::For))
    throw error::CompyledException("Expected 'for' token", Token.Location);

  auto LoopVariableToken = Tokenizer.consumeNextToken();
  if (LoopVariableToken.Type != lexer::TokenType::Identifier)
    throw error::CompyledException("Expected identifier token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::In))
    throw error::CompyledException("Expected 'in' token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  if (!isKeyword(Token, lexer::KeywordType::Range))
    throw error::CompyledException("Expected 'range' token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::LeftParenthesis))
    throw error::CompyledException("Expected '(' token", Token.Location);

  auto StartExpr = parseExpression(Tokenizer);
  auto StopExpr = std::unique_ptr<ast::ASTNode>{nullptr};
  auto StepExpr = std::unique_ptr<ast::ASTNode>{nullptr};
  Token = Tokenizer.peekNextToken();
  if (isDelimiter(Token, lexer::DelimiterType::Comma)) {
    Tokenizer.consumeNextToken();
    StopExpr = parseExpression(Tokenizer);
    Token = Tokenizer.peekNextToken();
    if (isDelimiter(Token, lexer::DelimiterType::Comma)) {
      Tokenizer.consumeNextToken();
      StepExpr = parseExpression(Tokenizer);
    }
  }

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::RightParenthesis))
    throw error::CompyledException("Expected ')' token", Token.Location);

  Token = Tokenizer.consumeNextToken();
  if (!isDelimiter(Token, lexer::DelimiterType::Colon))
    throw error::CompyledException("Expected ':' token", Token.Location);

  auto Block = parseBlock(Tokenizer);

  Token = Tokenizer.peekNextToken();
  auto ElseBlock = std::unique_ptr<ast::ASTNode>{nullptr};
  if (isKeyword(Token, lexer::KeywordType::Else)) {
    ElseBlock = parseElseBlock(Tokenizer);
  }

  return std::make_unique<ast::ForStmt>(std::move(StartExpr), std::move(StopExpr),
                                        std::move(StepExpr), std::move(Block),
                                        std::move(ElseBlock));
}

std::unique_ptr<ast::ASTNode> parseReturnAnnotation(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  if (isKeyword(Token, lexer::KeywordType::None)) {
    Tokenizer.consumeNextToken();
    return std::make_unique<ast::ValueType>(lexer::BaseType::None);
  }

  return parseTypeExpression(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseTypeExpression(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (Token.Type != lexer::TokenType::BaseType) {
    throw error::CompyledException("Expected type token", Token.Location);
  }

  return std::make_unique<ast::ValueType>(std::get<lexer::BaseType>(Token.Value));
};

std::unique_ptr<ast::ASTNode> parseExpression(lexer::Tokenizer &Tokenizer) {
  auto Expression = parseDisjunction(Tokenizer);

  auto IfToken = Tokenizer.peekNextToken();
  if (!isKeyword(IfToken, lexer::KeywordType::If))
    return Expression;

  auto Condition = parseDisjunction(Tokenizer);
  auto ElseToken = Tokenizer.peekNextToken();
  if (!isKeyword(IfToken, lexer::KeywordType::Else))
    throw error::CompyledException("Expected 'else' token", ElseToken.Location);

  auto ExprIfFalse = parseExpression(Tokenizer);
  return std::make_unique<ast::IfExpr>(std::move(Expression), std::move(ExprIfFalse),
                                       std::move(Condition));
}

std::unique_ptr<ast::ASTNode> parseDisjunction(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseConjunction(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::LogicalOr)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseConjunction(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseConjunction(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseInversion(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::LogicalAnd)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseInversion(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseInversion(lexer::Tokenizer &Tokenizer) {
  auto NotToken = Tokenizer.peekNextToken();
  if (isKeyword(NotToken, lexer::KeywordType::Not)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseInversion(Tokenizer);
    return std::make_unique<ast::UnaryOp>(std::move(Rhs), getOperator(NotToken));
  }

  return parseComparison(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parseComparison(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseBitwiseOr(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  if (isOperator(Token, lexer::OperatorType::Equal) ||
      isOperator(Token, lexer::OperatorType::NotEqual) ||
      isOperator(Token, lexer::OperatorType::LessOrEqual) ||
      isOperator(Token, lexer::OperatorType::Less) ||
      isOperator(Token, lexer::OperatorType::GreaterOrEqual) ||
      isOperator(Token, lexer::OperatorType::Greater)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseBitwiseOr(Tokenizer);
    return std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                  std::get<lexer::OperatorType>(Token.Value));
  }

  return nullptr;
}

std::unique_ptr<ast::ASTNode> parseBitwiseOr(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseBitwiseXor(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::BitwiseOr)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseBitwiseXor(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseBitwiseXor(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseBitwiseAnd(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::BitwiseAnd)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseBitwiseAnd(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseBitwiseAnd(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseShiftExpr(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::BitwiseAnd)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseShiftExpr(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseShiftExpr(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseSum(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::LeftShift) ||
         isOperator(Token, lexer::OperatorType::RightShift)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseSum(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseSum(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseTerm(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::Add) ||
         isOperator(Token, lexer::OperatorType::Subtract)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseTerm(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.consumeNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseTerm(lexer::Tokenizer &Tokenizer) {
  auto Lhs = parseFactor(Tokenizer);

  auto Token = Tokenizer.peekNextToken();
  while (isOperator(Token, lexer::OperatorType::Multiply) ||
         isOperator(Token, lexer::OperatorType::FloatDivide) ||
         isOperator(Token, lexer::OperatorType::IntegerDivide) ||
         isOperator(Token, lexer::OperatorType::Modulo)) {
    Tokenizer.consumeNextToken();
    auto Rhs = parseFactor(Tokenizer);
    Lhs = std::make_unique<ast::BinaryOperation>(std::move(Lhs), std::move(Rhs),
                                                 std::get<lexer::OperatorType>(Token.Value));
    Token = Tokenizer.peekNextToken();
  }

  return Lhs;
}

std::unique_ptr<ast::ASTNode> parseFactor(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  if (isOperator(Token, lexer::OperatorType::Add) ||
      isOperator(Token, lexer::OperatorType::Subtract) ||
      isOperator(Token, lexer::OperatorType::BitwiseInvert)) {
    Tokenizer.consumeNextToken();
    auto Factor = parseFactor(Tokenizer);
    return std::make_unique<ast::UnaryOp>(std::move(Factor), getOperator(Token));
  }

  return parsePower(Tokenizer);
}

std::unique_ptr<ast::ASTNode> parsePower(lexer::Tokenizer &Tokenizer) {
  auto Primary = parsePrimary(Tokenizer);
  if (!isOperator(Tokenizer.peekNextToken(), lexer::OperatorType::Exponentiate)) {
    return Primary;
  }
  Tokenizer.consumeNextToken();
  auto Factor = parseFactor(Tokenizer);
  return std::make_unique<ast::BinaryOperation>(std::move(Primary), std::move(Factor),
                                                lexer::OperatorType::Exponentiate);
}

std::unique_ptr<ast::ASTNode> parsePrimary(lexer::Tokenizer &Tokenizer) {
  auto Atom = parseAtom(Tokenizer);
  if (isDelimiter(Tokenizer.peekNextToken(), lexer::DelimiterType::LeftParenthesis)) {
    Tokenizer.consumeNextToken();
    auto Arguments = parseArguments(Tokenizer);
    const auto Token = Tokenizer.peekNextToken();
    if (!isDelimiter(Token, lexer::DelimiterType::RightParenthesis)) {
      throw error::CompyledException("Expected '(' token", Token.Location);
    }

    return std::make_unique<ast::FunctionCall>(std::move(Arguments));
  }

  return Atom;
}

std::unique_ptr<ast::ASTNode> parseIdentifier(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (Token.Type != lexer::TokenType::Identifier) {
    throw error::CompyledException("Expected a token identifier.", Token.Location);
  }
  return std::make_unique<ast::Identifier>(std::move(std::get<std::string>(Token.Value)));
}

std::unique_ptr<ast::ASTNode> parseAtom(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.peekNextToken();
  switch (Token.Type) {
  case compyled::lexer::TokenType::Value: {
    Tokenizer.consumeNextToken();
    if (std::holds_alternative<types::IntegerType>(Token.Value)) {
      return std::make_unique<ast::Value<types::IntegerType>>(
          std::get<types::IntegerType>(Token.Value));
    } else if (std::holds_alternative<types::FloatType>(Token.Value)) {
      return std::make_unique<ast::Value<types::FloatType>>(
          std::get<types::FloatType>(Token.Value));
    } else if (std::holds_alternative<types::FloatType>(Token.Value)) {
      return std::make_unique<ast::Value<types::BooleanType>>(
          std::get<types::BooleanType>(Token.Value));
    }
    throw error::CompyledException("Unexpected value type", Token.Location);
  }
  case compyled::lexer::TokenType::Identifier:
    return parseIdentifier(Tokenizer);
  case compyled::lexer::TokenType::Delimiter:
    return parseGroup(Tokenizer);
  default:
    throw error::CompyledException("Unexpected an identifier, literal, or '(' token.",
                                   Token.Location);
  }

  std::unreachable();
}

std::unique_ptr<ast::ASTNode> parseGroup(lexer::Tokenizer &Tokenizer) {
  auto Token = Tokenizer.consumeNextToken();
  if (isDelimiter(Token, lexer::DelimiterType::LeftParenthesis)) {
    throw error::CompyledException("Expected '(' token", Token.Location);
  }

  auto Expression = parseExpression(Tokenizer);

  Token = Tokenizer.consumeNextToken();
  if (isDelimiter(Token, lexer::DelimiterType::RightParenthesis)) {
    throw error::CompyledException("Expected ')' token", Token.Location);
  }

  return Expression;
}

std::vector<std::unique_ptr<ast::ASTNode>> parseArguments(lexer::Tokenizer &Tokenizer) {
  auto Expression = parseExpression(Tokenizer);
  auto Arguments = std::vector<std::unique_ptr<ast::ASTNode>>{};
  Arguments.push_back(std::move(Expression));

  while (isDelimiter(Tokenizer.peekNextToken(), lexer::DelimiterType::Comma)) {
    Tokenizer.consumeNextToken();
    Expression = parseExpression(Tokenizer);
    Arguments.push_back(std::move(Expression));
  }

  return Arguments;
}

} // namespace compyled::parser
