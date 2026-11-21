#include "AST.h"
#include "frontend/Tokenizer.h"

namespace compyled::ast {

template <typename T> Value<T>::Value(T Value) : Value_(Value) {}

ValueType::ValueType(const lexer::BaseType Type) : Type_(Type) {}

Identifier::Identifier(std::string &&Name) : Name_(std::move(Name)) {}

Parameter::Parameter(std::unique_ptr<ASTNode> &&Name, std::unique_ptr<ASTNode> &&Type,
                     std::unique_ptr<ASTNode> &&Default)
    : Name_(std::move(Name)), Type_(std::move(Type)), Default_(std::move(Default)) {}

FunctionDeclaration::FunctionDeclaration(std::unique_ptr<ASTNode> &&Name,
                                         std::vector<std::unique_ptr<ASTNode>> &&Parameters,
                                         std::unique_ptr<ASTNode> &&ReturnType,
                                         std::unique_ptr<ASTNode> &&Body)
    : Name_(std::move(Name)), Parameters_(std::move(Parameters)),
      ReturnType_(std::move(ReturnType)), Body_(std::move(Body)) {}

FunctionCall::FunctionCall(std::vector<std::unique_ptr<ASTNode>> &&Arguments)
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

IfStmt::IfStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&StmtIfTrue,
               std::unique_ptr<ASTNode> &&StmtIfFalse)
    : Condition_(std::move(Condition)), StmtIfTrue_(std::move(StmtIfTrue)),
      StmtIfFalse_(std::move(StmtIfFalse)) {}

ForStmt::ForStmt(std::unique_ptr<ASTNode> &&Start, std::unique_ptr<ASTNode> &&Stop,
                 std::unique_ptr<ASTNode> &&Step, std::unique_ptr<ASTNode> &&Block,
                 std::unique_ptr<ASTNode> &&ElseBlock)
    : Start_(std::move(Start)), Stop_(std::move(Stop)), Step_(std::move(Step)),
      Block_(std::move(Block)), ElseBlock_(std::move(ElseBlock)) {}

WhileStmt::WhileStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&Block,
                     std::unique_ptr<ASTNode> &&ElseBlock)
    : Condition_(std::move(Condition)), Block_(std::move(Block)), ElseBlock_(std::move(ElseBlock)) {
}

// Explicit instantiations
template class Value<lexer::IntegerType>;
template class Value<lexer::FloatType>;
template class Value<lexer::BooleanType>;

} // namespace compyled::ast
