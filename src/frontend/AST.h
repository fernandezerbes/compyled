#pragma once

#include "Tokenizer.h"
#include "frontend/Types.h"
#include <memory>
#include <string>
#include <vector>
namespace compyled::ast {

namespace visitor {

class Visitor;

}

class ASTNode {
public:
  virtual ~ASTNode() = default;

  virtual void accept(visitor::Visitor &Visitor) const {};
};

class Stmt : public ASTNode {
public:
  virtual ~Stmt() = default;

  void accept(visitor::Visitor &Visitor) const override {};
};

class Expr : public Stmt {
public:
  virtual ~Expr() = default;

  void accept(visitor::Visitor &Visitor) const override {};
};

template <typename T> class Value : public ASTNode {
  T Value_;

public:
  using Type = T;

  explicit Value(T Value);
  virtual ~Value() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class ValueType : public ASTNode {
  lexer::BaseType Type_;

public:
  explicit ValueType(const lexer::BaseType Type);
  ~ValueType() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class Identifier : public ASTNode {
  std::string Name_;

public:
  explicit Identifier(std::string &&Name);
  virtual ~Identifier() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class Parameter : public ASTNode {
  std::unique_ptr<ASTNode> Name_;
  std::unique_ptr<ASTNode> Type_;
  std::unique_ptr<ASTNode> Default_;

public:
  Parameter(std::unique_ptr<ASTNode> &&Name, std::unique_ptr<ASTNode> &&Type,
            std::unique_ptr<ASTNode> &&Default);
  ~Parameter() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class FunctionDeclaration : public ASTNode {
  std::unique_ptr<ASTNode> Name_;
  std::vector<std::unique_ptr<ASTNode>> Parameters_;
  std::unique_ptr<ASTNode> ReturnType_;
  std::unique_ptr<ASTNode> Body_;

public:
  FunctionDeclaration(std::unique_ptr<ASTNode> &&Name,
                      std::vector<std::unique_ptr<ASTNode>> &&Parameters,
                      std::unique_ptr<ASTNode> &&ReturnType, std::unique_ptr<ASTNode> &&Body);
  ~FunctionDeclaration() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class FunctionCall : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Arguments_;

public:
  explicit FunctionCall(std::vector<std::unique_ptr<ASTNode>> &&Arguments);
  virtual ~FunctionCall() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class BinaryOperation : public ASTNode {
  std::unique_ptr<ASTNode> Lhs_;
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  BinaryOperation(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                  const lexer::OperatorType Operator);
  virtual ~BinaryOperation() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class UnaryOperation : public ASTNode {
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  UnaryOperation(std::unique_ptr<ASTNode> &&Rhs, const lexer::OperatorType Operator);
  virtual ~UnaryOperation() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class Augassign : public ASTNode {
  std::unique_ptr<ASTNode> Lhs_;
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  Augassign(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
            const lexer::OperatorType Operator);
  virtual ~Augassign() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class TypedAssignment : public ASTNode {
  std::unique_ptr<ASTNode> Lhs_;
  std::unique_ptr<ASTNode> Rhs_;
  std::unique_ptr<ASTNode> Type_;

public:
  TypedAssignment(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                  std::unique_ptr<ASTNode> &&Type);
  virtual ~TypedAssignment() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class Assignment : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Lhss_;
  std::unique_ptr<ASTNode> Rhs_;

public:
  Assignment(std::vector<std::unique_ptr<ASTNode>> &&Lhss, std::unique_ptr<ASTNode> &&Rhs);
  virtual ~Assignment() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class IfExpr : public Expr {
  std::unique_ptr<ASTNode> ExprIfTrue_;
  std::unique_ptr<ASTNode> ExprIfFalse_;
  std::unique_ptr<ASTNode> Condition_;

public:
  IfExpr(std::unique_ptr<ASTNode> &&ExprIfTrue, std::unique_ptr<ASTNode> &&ExprIfFalse,
         std::unique_ptr<ASTNode> &&Condition);
  virtual ~IfExpr() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class Block : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Statements_;

public:
  explicit Block(std::vector<std::unique_ptr<ASTNode>> &&Statements);
  virtual ~Block() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class IfStmt : public Stmt {
  std::unique_ptr<ASTNode> Condition_;
  std::unique_ptr<ASTNode> StmtIfTrue_;
  std::unique_ptr<ASTNode> StmtIfFalse_;

public:
  IfStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&StmtIfTrue,
         std::unique_ptr<ASTNode> &&StmtIfFalse);
  virtual ~IfStmt() = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class ForStmt : public Stmt {
  std::unique_ptr<ASTNode> Start_;
  std::unique_ptr<ASTNode> Stop_;
  std::unique_ptr<ASTNode> Step_;
  std::unique_ptr<ASTNode> Block_;
  std::unique_ptr<ASTNode> ElseBlock_;

public:
  ForStmt(std::unique_ptr<ASTNode> &&Start, std::unique_ptr<ASTNode> &&Stop,
          std::unique_ptr<ASTNode> &&Step, std::unique_ptr<ASTNode> &&Block,
          std::unique_ptr<ASTNode> &&ElseBlock);
  ~ForStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class WhileStmt : public Stmt {
  std::unique_ptr<ASTNode> Condition_;
  std::unique_ptr<ASTNode> Block_;
  std::unique_ptr<ASTNode> ElseBlock_;

public:
  WhileStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&Block,
            std::unique_ptr<ASTNode> &&ElseBlock);
  ~WhileStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class GlobalStmt : public Stmt {
  std::vector<std::unique_ptr<ASTNode>> Names_;

public:
  explicit GlobalStmt(std::vector<std::unique_ptr<ASTNode>> Names);
  ~GlobalStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class ContinueStmt : public Stmt {
public:
  ~ContinueStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class BreakStmt : public Stmt {
public:
  ~BreakStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class PassStmt : public Stmt {
public:
  ~PassStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class ReturnStmt : public Stmt {
  std::unique_ptr<ASTNode> Expression_;

public:
  explicit ReturnStmt(std::unique_ptr<ASTNode> &&Expression);
  ~ReturnStmt() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

class Program : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Statements_;

public:
  Program() = default;
  explicit Program(std::vector<std::unique_ptr<ASTNode>> &&Statements);
  ~Program() override = default;

  void accept(visitor::Visitor &Visitor) const override;
};

namespace visitor {
class Visitor {
public:
  virtual ~Visitor() {};
  virtual void visitStmt(const Stmt &Stmt) {};
  virtual void visitExpr(const Expr &Expr) {};
  virtual void visitValue(const Value<types::IntegerType> &Value) {};
  virtual void visitValue(const Value<types::FloatType> &Value) {};
  virtual void visitValue(const Value<types::BooleanType> &Value) {};
  virtual void visitValueType(const ValueType &ValueType) {};
  virtual void visitIdentifier(const Identifier &Identifier) {};
  virtual void visitParameter(const Parameter &Parameter) {};
  virtual void visitFunctionDeclaration(const FunctionDeclaration &FunctionDeclaration) {};
  virtual void visitFunctionCall(const FunctionCall &FunctionCall) {};
  virtual void visitBinaryOperation(const BinaryOperation &BinaryOperation) {};
  virtual void visitUnaryOperation(const UnaryOperation &UnaryOperation) {};
  virtual void visitAugassign(const Augassign &Augassign) {};
  virtual void visitTypedAssignment(const TypedAssignment &TypedAssignment) {};
  virtual void visitAssignment(const Assignment &Assignment) {};
  virtual void visitIfExpr(const IfExpr &IfExpr) {};
  virtual void visitBlock(const Block &Block) {};
  virtual void visitIfStmt(const IfStmt &IfStmt) {};
  virtual void visitForStmt(const ForStmt &ForStmt) {};
  virtual void visitWhileStmt(const WhileStmt &WhileStmt) {};
  virtual void visitGlobalStmt(const GlobalStmt &GlobalStmt) {};
  virtual void visitContinueStmt(const ContinueStmt &ContinueStmt) {};
  virtual void visitBreakStmt(const BreakStmt &BreakStmt) {};
  virtual void visitPassStmt(const PassStmt &PassStmt) {};
  virtual void visitReturnStmt(const ReturnStmt &ReturnStmt) {};
  virtual void visitProgram(const Program &Program) {};
};

} // namespace visitor

} // namespace compyled::ast
