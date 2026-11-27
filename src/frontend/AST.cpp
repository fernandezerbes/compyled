#include "AST.h"
#include "Tokenizer.h"
#include "Types.h"

namespace compyled::ast {

template <typename T> Value<T>::Value(T Value) : Value_(Value) {}

template <typename T> void Value<T>::accept(visitor::Visitor &Visitor) const {
  Visitor.visitValue(*this);
}

ValueType::ValueType(const lexer::BaseType Type) : Type_(Type) {}

void ValueType::accept(visitor::Visitor &Visitor) const { Visitor.visitValueType(*this); }

Identifier::Identifier(std::string &&Name) : Name_(std::move(Name)) {}

void Identifier::accept(visitor::Visitor &Visitor) const { Visitor.visitIdentifier(*this); }

Parameter::Parameter(std::unique_ptr<ASTNode> &&Name, std::unique_ptr<ASTNode> &&Type,
                     std::unique_ptr<ASTNode> &&Default)
    : Name_(std::move(Name)), Type_(std::move(Type)), Default_(std::move(Default)) {}

void Parameter::accept(visitor::Visitor &Visitor) const { Visitor.visitParameter(*this); }

FunctionDeclaration::FunctionDeclaration(std::unique_ptr<ASTNode> &&Name,
                                         std::vector<std::unique_ptr<ASTNode>> &&Parameters,
                                         std::unique_ptr<ASTNode> &&ReturnType,
                                         std::unique_ptr<ASTNode> &&Body)
    : Name_(std::move(Name)), Parameters_(std::move(Parameters)),
      ReturnType_(std::move(ReturnType)), Body_(std::move(Body)) {}

void FunctionDeclaration::accept(visitor::Visitor &Visitor) const {
  Visitor.visitFunctionDeclaration(*this);
}

FunctionCall::FunctionCall(std::vector<std::unique_ptr<ASTNode>> &&Arguments)
    : Arguments_(std::move(Arguments)) {}

void FunctionCall::accept(visitor::Visitor &Visitor) const { Visitor.visitFunctionCall(*this); }

BinaryOperation::BinaryOperation(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                                 const lexer::OperatorType Operator)
    : Lhs_(std::move(Lhs)), Rhs_(std::move(Rhs)), Operator_(Operator) {}

void BinaryOperation::accept(visitor::Visitor &Visitor) const {
  Visitor.visitBinaryOperation(*this);
}

UnaryOperation::UnaryOperation(std::unique_ptr<ASTNode> &&Rhs, const lexer::OperatorType Operator)
    : Rhs_(std::move(Rhs)), Operator_(Operator) {}

void UnaryOperation::accept(visitor::Visitor &Visitor) const { Visitor.visitUnaryOperation(*this); }

Augassign::Augassign(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                     const lexer::OperatorType Operator)
    : Lhs_(std::move(Lhs)), Rhs_(std::move(Rhs)), Operator_(Operator) {}

void Augassign::accept(visitor::Visitor &Visitor) const { Visitor.visitAugassign(*this); }

TypedAssignment::TypedAssignment(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                                 std::unique_ptr<ASTNode> &&Type)
    : Lhs_(std::move(Lhs)), Rhs_(std::move(Rhs)), Type_(std::move(Type)) {}

void TypedAssignment::accept(visitor::Visitor &Visitor) const {
  Visitor.visitTypedAssignment(*this);
}

Assignment::Assignment(std::vector<std::unique_ptr<ASTNode>> &&Lhss, std::unique_ptr<ASTNode> &&Rhs)
    : Lhss_(std::move(Lhss)), Rhs_(std::move(Rhs)) {}

void Assignment::accept(visitor::Visitor &Visitor) const { Visitor.visitAssignment(*this); }

IfExpr::IfExpr(std::unique_ptr<ASTNode> &&ExprIfTrue, std::unique_ptr<ASTNode> &&ExprIfFalse,
               std::unique_ptr<ASTNode> &&Condition)
    : ExprIfTrue_(std::move(ExprIfTrue)), ExprIfFalse_(std::move(ExprIfFalse)),
      Condition_(std::move(Condition)) {}

void IfExpr::accept(visitor::Visitor &Visitor) const { Visitor.visitIfExpr(*this); }

Block::Block(std::vector<std::unique_ptr<ASTNode>> &&Statements)
    : Statements_(std::move(Statements)) {}

void Block::accept(visitor::Visitor &Visitor) const { Visitor.visitBlock(*this); }

IfStmt::IfStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&StmtIfTrue,
               std::unique_ptr<ASTNode> &&StmtIfFalse)
    : Condition_(std::move(Condition)), StmtIfTrue_(std::move(StmtIfTrue)),
      StmtIfFalse_(std::move(StmtIfFalse)) {}

void IfStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitIfStmt(*this); }

ForStmt::ForStmt(std::unique_ptr<ASTNode> &&Start, std::unique_ptr<ASTNode> &&Stop,
                 std::unique_ptr<ASTNode> &&Step, std::unique_ptr<ASTNode> &&Block,
                 std::unique_ptr<ASTNode> &&ElseBlock)
    : Start_(std::move(Start)), Stop_(std::move(Stop)), Step_(std::move(Step)),
      Block_(std::move(Block)), ElseBlock_(std::move(ElseBlock)) {}

void ForStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitForStmt(*this); }

WhileStmt::WhileStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&Block,
                     std::unique_ptr<ASTNode> &&ElseBlock)
    : Condition_(std::move(Condition)), Block_(std::move(Block)), ElseBlock_(std::move(ElseBlock)) {
}

void WhileStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitWhileStmt(*this); }

GlobalStmt::GlobalStmt(std::vector<std::unique_ptr<ASTNode>> Names) : Names_(std::move(Names)) {}

void GlobalStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitGlobalStmt(*this); }

void PassStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitPassStmt(*this); }

void BreakStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitBreakStmt(*this); }

void ContinueStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitContinueStmt(*this); }

ReturnStmt::ReturnStmt(std::unique_ptr<ASTNode> &&Expression)
    : Expression_(std::move(Expression)) {}

void ReturnStmt::accept(visitor::Visitor &Visitor) const { Visitor.visitReturnStmt(*this); }

Program::Program(std::vector<std::unique_ptr<ASTNode>> &&Statements)
    : Statements_(std::move(Statements)) {}

void Program::accept(visitor::Visitor &Visitor) const { Visitor.visitProgram(*this); }

// Explicit instantiations
template class Value<types::IntegerType>;
template class Value<types::FloatType>;
template class Value<types::BooleanType>;

} // namespace compyled::ast
