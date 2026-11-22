#include "AST.h"
#include "Tokenizer.h"
#include "Types.h"

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

UnaryOp::UnaryOp(std::unique_ptr<ASTNode> &&Rhs, const lexer::OperatorType Operator)
    : Rhs_(std::move(Rhs)), Operator_(Operator) {}

Augassign::Augassign(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                     const lexer::OperatorType Operator)
    : Lhs_(std::move(Lhs)), Rhs_(std::move(Rhs)), Operator_(Operator) {}

TypedAssignment::TypedAssignment(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                                 std::unique_ptr<ASTNode> &&Type)
    : Lhs_(std::move(Lhs)), Rhs_(std::move(Rhs)), Type_(std::move(Type)) {}

Assignment::Assignment(std::vector<std::unique_ptr<ASTNode>> &&Lhss, std::unique_ptr<ASTNode> &&Rhs)
    : Lhss_(std::move(Lhss)), Rhs_(std::move(Rhs)) {}

IfExpr::IfExpr(std::unique_ptr<ASTNode> &&ExprIfTrue, std::unique_ptr<ASTNode> &&ExprIfFalse,
               std::unique_ptr<ASTNode> &&Condition)
    : ExprIfTrue_(std::move(ExprIfTrue)), ExprIfFalse_(std::move(ExprIfFalse)),
      Condition_(std::move(Condition)) {}

Block::Block(std::vector<std::unique_ptr<ASTNode>> &&Statements)
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

GlobalStmt::GlobalStmt(std::vector<std::unique_ptr<ASTNode>> Names) : Names_(std::move(Names)) {}

ReturnStmt::ReturnStmt(std::unique_ptr<ASTNode> &&Expression)
    : Expression_(std::move(Expression)) {}

Program::Program(std::vector<std::unique_ptr<ASTNode>> &&Statements)
    : Statements_(std::move(Statements)) {}

// Explicit instantiations
template class Value<types::IntegerType>;
template class Value<types::FloatType>;
template class Value<types::BooleanType>;

} // namespace compyled::ast
