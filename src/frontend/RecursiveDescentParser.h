#pragma once
#include "AST.h"
#include "Tokenizer.h"
#include <memory>

namespace compyled::parser {

class RecursiveDescentParser {
  lexer::Tokenizer &Tokenizer_;

public:
  RecursiveDescentParser(lexer::Tokenizer &Tokenizer);

  std::unique_ptr<ast::ASTNode> BuildAST();
};

namespace detail {

bool isDelimiter(const lexer::Token &Token, const lexer::DelimiterType ExpectedType);
bool isOperator(const lexer::Token &Token, const lexer::OperatorType ExpectedType);
std::unique_ptr<ast::ASTNode> parseExpression(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseIfExpression(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseDisjunction(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseConjunction(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseInversion(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseComparison(lexer::Tokenizer &Tokenizer);

std::unique_ptr<ast::ASTNode> parseSum(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseTerm(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseFactor(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parsePower(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parsePrimary(lexer::Tokenizer &Tokenizer);
std::vector<std::unique_ptr<ast::ASTNode>> parseArguments(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseAtom(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseGroup(lexer::Tokenizer &Tokenizer, lexer::Token &&FirstToken);

} // namespace detail

} // namespace compyled::parser
