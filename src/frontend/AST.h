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
  lexer::ValueType Type_;

public:
  explicit ValueType(const lexer::ValueType Type);
  ~ValueType() override = default;
};

class Identifier : public ASTNode {
  std::string Name_;

public:
  explicit Identifier(std::string &&Name);
  virtual ~Identifier() = default;
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

class UnaryOperation : public ASTNode {
  std::unique_ptr<ASTNode> Rhs_;
  const lexer::OperatorType Operator_;

public:
  UnaryOperation(std::unique_ptr<ASTNode> &&Rhs, const lexer::OperatorType Operator);
  virtual ~UnaryOperation() = default;
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
  std::vector<std::unique_ptr<Stmt>> Statements_;

public:
  explicit Block(std::vector<std::unique_ptr<Stmt>> &&Statements);
  virtual ~Block() = default;
};

class IfStmt : public Stmt {
  std::unique_ptr<Expr> Condition_;
  std::unique_ptr<Block> StmtIfTrue_;
  std::unique_ptr<Stmt> StmtIfFalse_;

public:
  IfStmt(std::unique_ptr<Expr> &&Condition, std::unique_ptr<Block> &&StmtIfTrue,
         std::unique_ptr<Stmt> &&StmtIfFalse);
  virtual ~IfStmt() = default;
};

} // namespace compyled::ast
