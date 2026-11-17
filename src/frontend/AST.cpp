#include "AST.h"
#include "frontend/Tokenizer.h"

namespace compyled::ast {

template <typename T> Value<T>::Value(T Value) : Value_(Value) {}

Identifier::Identifier(std::string &&Name) : Name_(std::move(Name)) {}

FunctionCall::FunctionCall(std::unique_ptr<ASTNode> &&Arguments)
    : Arguments_(std::move(Arguments)) {}

BinaryOperation::BinaryOperation(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                                 const lexer::OperatorType Operator)
    : Lhs_(std::move(Lhs)), Rhs_(std::move(Rhs)), Operator_(Operator) {}

UnaryOperation::UnaryOperation(std::unique_ptr<ASTNode> &&Rhs, const lexer::OperatorType Operator)
    : Rhs_(std::move(Rhs)), Operator_(Operator) {}

IfExpr::IfExpr(std::unique_ptr<ASTNode> &&ExprIfTrue, std::unique_ptr<ASTNode> &&ExprIfFalse,
               std::unique_ptr<ASTNode> &&Condition)
    : ExprIfTrue_(std::move(ExprIfTrue)), ExprIfFalse_(std::move(ExprIfFalse)),
      Condition_(std::move(Condition)) {}

Block::Block(std::vector<std::unique_ptr<Stmt>> &&Statements)
    : Statements_(std::move(Statements)) {}

IfStmt::IfStmt(std::unique_ptr<Expr> &&Condition, std::unique_ptr<Block> &&StmtIfTrue,
               std::unique_ptr<Stmt> &&StmtIfFalse)
    : Condition_(std::move(Condition)), StmtIfTrue_(std::move(StmtIfTrue)),
      StmtIfFalse_(std::move(StmtIfFalse)) {}

// Explicit instantiations
template class Value<lexer::IntegerType>;
template class Value<lexer::FloatType>;
template class Value<lexer::BooleanType>;

} // namespace compyled::ast
