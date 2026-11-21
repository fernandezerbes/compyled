#pragma once
#include "AST.h"
#include "Tokenizer.h"
#include <memory>

namespace compyled::parser {

bool isDelimiter(const lexer::Token &Token, const lexer::DelimiterType ExpectedType);
bool isOperator(const lexer::Token &Token, const lexer::OperatorType ExpectedType);
bool isKeyword(const lexer::Token &Token, const lexer::KeywordType ExpectedType);
bool isAugassign(const std::expected<lexer::Token, std::string> &Token);

std::unique_ptr<ast::ASTNode> parseFile(lexer::Tokenizer &Tokenizer);
std::vector<std::unique_ptr<ast::ASTNode>> parseStatement(lexer::Tokenizer &Tokenizer);
std::vector<std::unique_ptr<ast::ASTNode>> parseSimpleStmts(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseSimpleStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseCompoundStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseAssignment(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseReturnStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parsePassStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBreakStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseContinueStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseGlobalStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBlock(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseFunctionDef(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseParameter(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseAnnotation(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseDefault(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseIfElifStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseElseBlock(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseWhileStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseForStmt(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseReturnAnnotation(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseTypeExpression(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseExpression(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseDisjunction(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseConjunction(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseInversion(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseComparison(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBitwiseOr(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBitwiseXor(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseBitwiseAnd(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseShiftExpr(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseSum(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseTerm(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseFactor(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parsePower(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parsePrimary(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseAtom(lexer::Tokenizer &Tokenizer);
std::unique_ptr<ast::ASTNode> parseGroup(lexer::Tokenizer &Tokenizer);
std::vector<std::unique_ptr<ast::ASTNode>> parseArguments(lexer::Tokenizer &Tokenizer);

} // namespace compyled::parser
