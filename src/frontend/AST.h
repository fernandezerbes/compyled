#pragma once

#include "frontend/Tokenizer.h"
#include <memory>
#include <string>
#include <vector>
namespace compyled::ast {

class ASTNode {
public:
  virtual ~ASTNode() = default;
};

class Stmt : public ASTNode {
public:
  virtual ~Stmt() = default;
};

class Expr : public Stmt {
public:
  virtual ~Expr() = default;
};

template <typename T> class Value : public ASTNode {
  T Value_;

public:
  using Type = T;

  explicit Value(T Value);
  virtual ~Value() = default;
};

class ValueType : public ASTNode {
  lexer::BaseType Type_;

public:
  explicit ValueType(const lexer::BaseType Type);
  ~ValueType() override = default;
};

class Identifier : public ASTNode {
  std::string Name_;

public:
  explicit Identifier(std::string &&Name);
  virtual ~Identifier() = default;
};

class Parameter : public ASTNode {
  std::unique_ptr<ASTNode> Name_;
  std::unique_ptr<ASTNode> Type_;
  std::unique_ptr<ASTNode> Default_;

public:
  Parameter(std::unique_ptr<ASTNode> &&Name, std::unique_ptr<ASTNode> &&Type,
            std::unique_ptr<ASTNode> &&Default);
  ~Parameter() override = default;
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
};

class FunctionCall : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Arguments_;

public:
  explicit FunctionCall(std::vector<std::unique_ptr<ASTNode>> &&Arguments);
  virtual ~FunctionCall() = default;
};

class BinaryOperation : public ASTNode {
  std::unique_ptr<ASTNode> Lhs_;
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  BinaryOperation(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                  const lexer::OperatorType Operator);
  virtual ~BinaryOperation() = default;
};

class UnaryOp : public ASTNode {
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  UnaryOp(std::unique_ptr<ASTNode> &&Rhs, const lexer::OperatorType Operator);
  virtual ~UnaryOp() = default;
};

class Augassign : public ASTNode {
  std::unique_ptr<ASTNode> Lhs_;
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  Augassign(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
            const lexer::OperatorType Operator);
  virtual ~Augassign() = default;
};

class TypedAssignment : public ASTNode {
  std::unique_ptr<ASTNode> Lhs_;
  std::unique_ptr<ASTNode> Rhs_;
  std::unique_ptr<ASTNode> Type_;

public:
  TypedAssignment(std::unique_ptr<ASTNode> &&Lhs, std::unique_ptr<ASTNode> &&Rhs,
                  std::unique_ptr<ASTNode> &&Type);
  virtual ~TypedAssignment() = default;
};

class Assignment : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Lhss_;
  std::unique_ptr<ASTNode> Rhs_;

public:
  Assignment(std::vector<std::unique_ptr<ASTNode>> &&Lhss, std::unique_ptr<ASTNode> &&Rhs);
  virtual ~Assignment() = default;
};

class IfExpr : public Expr {
  std::unique_ptr<ASTNode> ExprIfTrue_;
  std::unique_ptr<ASTNode> ExprIfFalse_;
  std::unique_ptr<ASTNode> Condition_;

public:
  IfExpr(std::unique_ptr<ASTNode> &&ExprIfTrue, std::unique_ptr<ASTNode> &&ExprIfFalse,
         std::unique_ptr<ASTNode> &&Condition);
  virtual ~IfExpr() = default;
};

class Block : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Statements_;

public:
  explicit Block(std::vector<std::unique_ptr<ASTNode>> &&Statements);
  virtual ~Block() = default;
};

class IfStmt : public Stmt {
  std::unique_ptr<ASTNode> Condition_;
  std::unique_ptr<ASTNode> StmtIfTrue_;
  std::unique_ptr<ASTNode> StmtIfFalse_;

public:
  IfStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&StmtIfTrue,
         std::unique_ptr<ASTNode> &&StmtIfFalse);
  virtual ~IfStmt() = default;
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
};

class WhileStmt : public Stmt {
  std::unique_ptr<ASTNode> Condition_;
  std::unique_ptr<ASTNode> Block_;
  std::unique_ptr<ASTNode> ElseBlock_;

public:
  WhileStmt(std::unique_ptr<ASTNode> &&Condition, std::unique_ptr<ASTNode> &&Block,
            std::unique_ptr<ASTNode> &&ElseBlock);
  ~WhileStmt() override = default;
};

class GlobalStmt : public Stmt {
  std::vector<std::unique_ptr<ASTNode>> Names_;

public:
  explicit GlobalStmt(std::vector<std::unique_ptr<ASTNode>> Names);
  ~GlobalStmt() override = default;
};

class ContinueStmt : public Stmt {
public:
  ~ContinueStmt() override = default;
};

class BreakStmt : public Stmt {
public:
  ~BreakStmt() override = default;
};

class PassStmt : public Stmt {
public:
  ~PassStmt() override = default;
};

class ReturnStmt : public Stmt {
  std::unique_ptr<ASTNode> Expression_;

public:
  explicit ReturnStmt(std::unique_ptr<ASTNode> &&Expression);
  ~ReturnStmt() override = default;
};

class Program : public ASTNode {
  std::vector<std::unique_ptr<ASTNode>> Statements_;

public:
  Program() = default;
  explicit Program(std::vector<std::unique_ptr<ASTNode>> &&Statements);
  ~Program() override = default;
};

} // namespace compyled::ast
