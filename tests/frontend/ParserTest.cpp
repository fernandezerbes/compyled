// Tests for the parser (frontend/Parser.h).
//
// The expectations are derived from grammar.peg and from the parser/AST interface only. Every test
// parses a source string and compares the resulting AST against an S-expression rendering of the
// tree the grammar mandates (see "AST rendering" below for the format).

#include "frontend/Parser.h"
#include "frontend/AST.h"
#include "frontend/Error.h"
#include "frontend/Tokenizer.h"
#include "frontend/Types.h"

#include "TestUtils.h"

#include <gtest/gtest.h>

#include <exception>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace compyled;
using namespace compyled::lexer;
using namespace compyled::parser;
using namespace compyled::test;

namespace {

using NodePtr = std::unique_ptr<ast::ASTNode>;
using NodeList = std::vector<std::unique_ptr<ast::ASTNode>>;

// -------------------------------------------------------------------------------------------------
// Read-only access to the AST fields
// -------------------------------------------------------------------------------------------------
//
// The AST classes expose no accessors, so the tests read their (private) fields through the
// standard explicit-instantiation trick: access checks do not apply to the template arguments of an
// explicit instantiation, which lets `PrivateField` capture a pointer to a private member and hand
// it out through a friend function found by ADL on the tag type.
//
// If the AST ever grows public getters, only this section needs to change.

template <typename Tag, typename Tag::Type Member> struct PrivateField {
  friend typename Tag::Type memberPtr(Tag) { return Member; }
};

#define AST_FIELD(TagName, Class, FieldType, Field)                                                \
  struct TagName {                                                                                 \
    using Type = FieldType Class::*;                                                               \
    friend Type memberPtr(TagName);                                                                \
  };                                                                                               \
  template struct PrivateField<TagName, &Class::Field>

AST_FIELD(IntValue, ast::Value<types::IntegerType>, types::IntegerType, Value_);
AST_FIELD(FloatValue, ast::Value<types::FloatType>, types::FloatType, Value_);
AST_FIELD(BoolValue, ast::Value<types::BooleanType>, types::BooleanType, Value_);
AST_FIELD(ValueTypeType, ast::ValueType, BaseType, Type_);
AST_FIELD(IdentifierName, ast::Identifier, std::string, Name_);
AST_FIELD(ParameterName, ast::Parameter, NodePtr, Name_);
AST_FIELD(ParameterType, ast::Parameter, NodePtr, Type_);
AST_FIELD(ParameterDefault, ast::Parameter, NodePtr, Default_);
AST_FIELD(FunctionName, ast::FunctionDeclaration, NodePtr, Name_);
AST_FIELD(FunctionParameters, ast::FunctionDeclaration, NodeList, Parameters_);
AST_FIELD(FunctionReturnType, ast::FunctionDeclaration, NodePtr, ReturnType_);
AST_FIELD(FunctionBody, ast::FunctionDeclaration, NodePtr, Body_);
AST_FIELD(CallArguments, ast::FunctionCall, NodeList, Arguments_);
AST_FIELD(BinaryLhs, ast::BinaryOperation, NodePtr, Lhs_);
AST_FIELD(BinaryRhs, ast::BinaryOperation, NodePtr, Rhs_);
AST_FIELD(BinaryOperator, ast::BinaryOperation, const OperatorType, Operator_);
AST_FIELD(UnaryRhs, ast::UnaryOp, NodePtr, Rhs_);
AST_FIELD(UnaryOperator, ast::UnaryOp, const OperatorType, Operator_);
AST_FIELD(AugassignLhs, ast::Augassign, NodePtr, Lhs_);
AST_FIELD(AugassignRhs, ast::Augassign, NodePtr, Rhs_);
AST_FIELD(AugassignOperator, ast::Augassign, const OperatorType, Operator_);
AST_FIELD(TypedAssignmentLhs, ast::TypedAssignment, NodePtr, Lhs_);
AST_FIELD(TypedAssignmentRhs, ast::TypedAssignment, NodePtr, Rhs_);
AST_FIELD(TypedAssignmentType, ast::TypedAssignment, NodePtr, Type_);
AST_FIELD(AssignmentLhss, ast::Assignment, NodeList, Lhss_);
AST_FIELD(AssignmentRhs, ast::Assignment, NodePtr, Rhs_);
AST_FIELD(IfExprIfTrue, ast::IfExpr, NodePtr, ExprIfTrue_);
AST_FIELD(IfExprIfFalse, ast::IfExpr, NodePtr, ExprIfFalse_);
AST_FIELD(IfExprCondition, ast::IfExpr, NodePtr, Condition_);
AST_FIELD(BlockStatements, ast::Block, NodeList, Statements_);
AST_FIELD(IfStmtCondition, ast::IfStmt, NodePtr, Condition_);
AST_FIELD(IfStmtIfTrue, ast::IfStmt, NodePtr, StmtIfTrue_);
AST_FIELD(IfStmtIfFalse, ast::IfStmt, NodePtr, StmtIfFalse_);
AST_FIELD(ForStart, ast::ForStmt, NodePtr, Start_);
AST_FIELD(ForStop, ast::ForStmt, NodePtr, Stop_);
AST_FIELD(ForStep, ast::ForStmt, NodePtr, Step_);
AST_FIELD(ForBlock, ast::ForStmt, NodePtr, Block_);
AST_FIELD(ForElseBlock, ast::ForStmt, NodePtr, ElseBlock_);
AST_FIELD(WhileCondition, ast::WhileStmt, NodePtr, Condition_);
AST_FIELD(WhileBlock, ast::WhileStmt, NodePtr, Block_);
AST_FIELD(WhileElseBlock, ast::WhileStmt, NodePtr, ElseBlock_);
AST_FIELD(GlobalNames, ast::GlobalStmt, NodeList, Names_);
AST_FIELD(ReturnExpression, ast::ReturnStmt, NodePtr, Expression_);
AST_FIELD(ProgramStatements, ast::Program, NodeList, Statements_);

#undef AST_FIELD

template <typename Tag, typename Node> const auto &field(const Node &N) {
  return N.*memberPtr(Tag{});
}

// -------------------------------------------------------------------------------------------------
// AST rendering
// -------------------------------------------------------------------------------------------------
//
// Every node is rendered as an S-expression:
//
//   Identifier            x
//   Value<int>            42
//   Value<float>          1.5             (always has a '.' or an exponent: 2.0, 1e+20)
//   Value<bool>           True / False
//   ValueType             int / float / bool / None
//   UnaryOp               (- x)           (op is the source symbol: + - ~ not)
//   BinaryOperation       (+ a b)         (op is the source symbol: + - * / // % ** & | ^ << >>
//                                          < > <= >= == != and or)
//   IfExpr                (ifexpr Cond IfTrue IfFalse)
//   FunctionCall          (call Arg...)   (the AST does not store the callee)
//   Assignment            (assign [Target...] Value)
//   TypedAssignment       (decl Target Type Value)
//   Augassign             (augassign Op Target Value)   (op is the source symbol: += -= ...)
//   ReturnStmt            (return Value)
//   PassStmt              pass
//   BreakStmt             break
//   ContinueStmt          continue
//   GlobalStmt            (global Name...)
//   Block                 (block Stmt...)
//   IfStmt                (if Cond Block Else)     (Else is a Block, a nested IfStmt for elif, or
//   _) WhileStmt             (while Cond Block Else) ForStmt               (for Start Stop Step
//   Block Else) Parameter             (param Name Type Default) FunctionDeclaration   (def Name
//   [Param...] ReturnType Block) Program               (program Stmt...)
//
// A missing (null) child is rendered as `_`. A list of nodes is rendered as `[a b c]`.

std::string symbol(OperatorType Op) {
  switch (Op) {
  case OperatorType::UnaryAdd:
    return "+=";
  case OperatorType::UnarySubtract:
    return "-=";
  case OperatorType::UnaryMultiply:
    return "*=";
  case OperatorType::UnaryExponentiate:
    return "**=";
  case OperatorType::UnaryFloatDivide:
    return "/=";
  case OperatorType::UnaryIntegerDivide:
    return "//=";
  case OperatorType::UnaryModulo:
    return "%=";
  case OperatorType::UnaryBitwiseAnd:
    return "&=";
  case OperatorType::UnaryBitwiseOr:
    return "|=";
  case OperatorType::UnaryBitwiseXor:
    return "^=";
  case OperatorType::UnaryLeftShift:
    return "<<=";
  case OperatorType::UnaryRightShift:
    return ">>=";
  case OperatorType::BitwiseAnd:
    return "&";
  case OperatorType::BitwiseOr:
    return "|";
  case OperatorType::BitwiseXor:
    return "^";
  case OperatorType::BitwiseInvert:
    return "~";
  case OperatorType::LeftShift:
    return "<<";
  case OperatorType::RightShift:
    return ">>";
  case OperatorType::LessOrEqual:
    return "<=";
  case OperatorType::GreaterOrEqual:
    return ">=";
  case OperatorType::Less:
    return "<";
  case OperatorType::Greater:
    return ">";
  case OperatorType::Equal:
    return "==";
  case OperatorType::NotEqual:
    return "!=";
  case OperatorType::Assign:
    return "=";
  case OperatorType::Add:
    return "+";
  case OperatorType::Subtract:
    return "-";
  case OperatorType::Exponentiate:
    return "**";
  case OperatorType::Multiply:
    return "*";
  case OperatorType::IntegerDivide:
    return "//";
  case OperatorType::FloatDivide:
    return "/";
  case OperatorType::Modulo:
    return "%";
  case OperatorType::LogicalNot:
    return "not";
  case OperatorType::LogicalAnd:
    return "and";
  case OperatorType::LogicalOr:
    return "or";
  }
  return "<op " + std::to_string(static_cast<int>(Op)) + ">";
}

std::string typeName(BaseType Type) {
  switch (Type) {
  case BaseType::Integer:
    return "int";
  case BaseType::Float:
    return "float";
  case BaseType::Boolean:
    return "bool";
  case BaseType::None:
    return "None";
  }
  return "<type " + std::to_string(static_cast<int>(Type)) + ">";
}

std::string floatLiteral(types::FloatType V) {
  auto S = std::format("{}", V);
  if (S.find_first_of(".en") == std::string::npos) {
    S += ".0";
  }
  return S;
}

std::string dump(const ast::ASTNode *N);
std::string dump(const NodePtr &N) { return dump(N.get()); }

std::string dump(const NodeList &Nodes) {
  std::string S = "[";
  for (std::size_t I = 0; I < Nodes.size(); ++I) {
    S += (I == 0 ? "" : " ") + dump(Nodes[I]);
  }
  return S + "]";
}

// Renders "(Head Part...)".
std::string sexpr(std::string_view Head, const std::vector<std::string> &Parts = {}) {
  std::string S = "(" + std::string(Head);
  for (const auto &P : Parts) {
    S += " " + P;
  }
  return S + ")";
}

// Renders "(Head Part... Node...)": the elements of `Nodes` are spliced in, without brackets.
std::string sexpr(std::string_view Head, std::vector<std::string> Parts, const NodeList &Nodes) {
  for (const auto &Node : Nodes) {
    Parts.push_back(dump(Node));
  }
  return sexpr(Head, Parts);
}

std::string dump(const ast::ASTNode *N) {
  if (N == nullptr) {
    return "_";
  }
  if (const auto *V = dynamic_cast<const ast::Identifier *>(N)) {
    return field<IdentifierName>(*V);
  }
  if (const auto *V = dynamic_cast<const ast::Value<types::IntegerType> *>(N)) {
    return std::to_string(field<IntValue>(*V));
  }
  if (const auto *V = dynamic_cast<const ast::Value<types::FloatType> *>(N)) {
    return floatLiteral(field<FloatValue>(*V));
  }
  if (const auto *V = dynamic_cast<const ast::Value<types::BooleanType> *>(N)) {
    return field<BoolValue>(*V) ? "True" : "False";
  }
  if (const auto *V = dynamic_cast<const ast::ValueType *>(N)) {
    return typeName(field<ValueTypeType>(*V));
  }
  if (const auto *V = dynamic_cast<const ast::UnaryOp *>(N)) {
    return sexpr(symbol(field<UnaryOperator>(*V)), {dump(field<UnaryRhs>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::BinaryOperation *>(N)) {
    return sexpr(symbol(field<BinaryOperator>(*V)),
                 {dump(field<BinaryLhs>(*V)), dump(field<BinaryRhs>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::IfExpr *>(N)) {
    return sexpr("ifexpr", {dump(field<IfExprCondition>(*V)), dump(field<IfExprIfTrue>(*V)),
                            dump(field<IfExprIfFalse>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::FunctionCall *>(N)) {
    return sexpr("call", {}, field<CallArguments>(*V));
  }
  if (const auto *V = dynamic_cast<const ast::Assignment *>(N)) {
    return sexpr("assign", {dump(field<AssignmentLhss>(*V)), dump(field<AssignmentRhs>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::TypedAssignment *>(N)) {
    return sexpr("decl", {dump(field<TypedAssignmentLhs>(*V)), dump(field<TypedAssignmentType>(*V)),
                          dump(field<TypedAssignmentRhs>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::Augassign *>(N)) {
    return sexpr("augassign", {symbol(field<AugassignOperator>(*V)), dump(field<AugassignLhs>(*V)),
                               dump(field<AugassignRhs>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::ReturnStmt *>(N)) {
    return sexpr("return", {dump(field<ReturnExpression>(*V))});
  }
  if (dynamic_cast<const ast::PassStmt *>(N)) {
    return "pass";
  }
  if (dynamic_cast<const ast::BreakStmt *>(N)) {
    return "break";
  }
  if (dynamic_cast<const ast::ContinueStmt *>(N)) {
    return "continue";
  }
  if (const auto *V = dynamic_cast<const ast::GlobalStmt *>(N)) {
    return sexpr("global", {}, field<GlobalNames>(*V));
  }
  if (const auto *V = dynamic_cast<const ast::Block *>(N)) {
    return sexpr("block", {}, field<BlockStatements>(*V));
  }
  if (const auto *V = dynamic_cast<const ast::IfStmt *>(N)) {
    return sexpr("if", {dump(field<IfStmtCondition>(*V)), dump(field<IfStmtIfTrue>(*V)),
                        dump(field<IfStmtIfFalse>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::WhileStmt *>(N)) {
    return sexpr("while", {dump(field<WhileCondition>(*V)), dump(field<WhileBlock>(*V)),
                           dump(field<WhileElseBlock>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::ForStmt *>(N)) {
    return sexpr("for",
                 {dump(field<ForStart>(*V)), dump(field<ForStop>(*V)), dump(field<ForStep>(*V)),
                  dump(field<ForBlock>(*V)), dump(field<ForElseBlock>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::Parameter *>(N)) {
    return sexpr("param", {dump(field<ParameterName>(*V)), dump(field<ParameterType>(*V)),
                           dump(field<ParameterDefault>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::FunctionDeclaration *>(N)) {
    return sexpr("def", {dump(field<FunctionName>(*V)), dump(field<FunctionParameters>(*V)),
                         dump(field<FunctionReturnType>(*V)), dump(field<FunctionBody>(*V))});
  }
  if (const auto *V = dynamic_cast<const ast::Program *>(N)) {
    return sexpr("program", {}, field<ProgramStatements>(*V));
  }
  return "<unknown node>";
}

// -------------------------------------------------------------------------------------------------
// Test helpers
// -------------------------------------------------------------------------------------------------

using NodeRule = NodePtr (*)(Tokenizer &);
using ListRule = NodeList (*)(Tokenizer &);

// Structural tokens (NewLine, Indent, Dedent, Eof) are compared by type only; their value carries
// no information.
bool matchesNext(const Token &Actual, const Expected &Want) {
  switch (Want.Type) {
  case TokenType::NewLine:
  case TokenType::Indent:
  case TokenType::Dedent:
  case TokenType::Eof:
    return Actual.Type == Want.Type;
  default:
    return matches(Actual, Want);
  }
}

// Parses `Source` with `Rule` and asserts that the AST renders as `Want` and that the rule stopped
// right before a token matching `Next`.
template <typename Rule>
void expectParse(Rule Parse, const std::string &Source, std::string_view Want,
                 const Expected &Next) {
  SCOPED_TRACE("source: \"" + Source + "\"");
  try {
    Tokenizer T{std::string(Source)};
    const auto Result = Parse(T);
    EXPECT_EQ(dump(Result), Want);
    const auto Got = T.peekNextToken();
    EXPECT_TRUE(matchesNext(Got, Next))
        << "next token after parsing: expected " << toString(Next) << ", got " << toString(Got);
  } catch (const std::exception &E) {
    FAIL() << "parser threw: " << E.what();
  }
}

// Asserts that parsing `Source` with `Rule` fails with a compiler error.
template <typename Rule> void expectParseError(Rule Parse, const std::string &Source) {
  SCOPED_TRACE("source: \"" + Source + "\"");
  EXPECT_THROW(
      {
        Tokenizer T{std::string(Source)};
        Parse(T);
      },
      error::CompyledException);
}

// Parses `Source` (a single line without '\n') as an expression, which must span all of it.
void expectExpr(const std::string &Source, std::string_view Want) {
  expectParse(NodeRule{parseExpression}, Source, Want, Eof());
}

void expectExprError(const std::string &Source) {
  expectParseError(NodeRule{parseExpression}, Source);
}

// Parses `Source` as a whole file and asserts that it contains exactly the statements `Want`.
void expectFile(const std::string &Source, const std::vector<std::string> &Want) {
  expectParse(NodeRule{parseFile}, Source, sexpr("program", Want), Eof());
}

// Parses `Source` as a whole file that contains exactly one statement.
void expectStmt(const std::string &Source, const std::string &Want) { expectFile(Source, {Want}); }

void expectFileError(const std::string &Source) { expectParseError(NodeRule{parseFile}, Source); }

// Returns the first token of `Source`.
Token firstToken(const std::string &Source) {
  return Tokenizer{std::string(Source)}.peekNextToken();
}

} // namespace

// =================================================================================================
// Token predicates
// =================================================================================================

TEST(ParserTokenPredicates, IsDelimiterMatchesTheDelimiterType) {
  EXPECT_TRUE(isDelimiter(firstToken("("), DelimiterType::LeftParenthesis));
  EXPECT_TRUE(isDelimiter(firstToken(")"), DelimiterType::RightParenthesis));
  EXPECT_TRUE(isDelimiter(firstToken(","), DelimiterType::Comma));
  EXPECT_TRUE(isDelimiter(firstToken(":"), DelimiterType::Colon));
  EXPECT_TRUE(isDelimiter(firstToken(";"), DelimiterType::Semicolon));
  EXPECT_TRUE(isDelimiter(firstToken("->"), DelimiterType::Arrow));
}

TEST(ParserTokenPredicates, IsDelimiterRejectsOtherDelimiters) {
  EXPECT_FALSE(isDelimiter(firstToken("("), DelimiterType::RightParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken(","), DelimiterType::Colon));
}

TEST(ParserTokenPredicates, IsDelimiterRejectsOtherTokenTypes) {
  EXPECT_FALSE(isDelimiter(firstToken("x"), DelimiterType::LeftParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken("1"), DelimiterType::LeftParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken("if"), DelimiterType::LeftParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken("+="), DelimiterType::LeftParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken("+"), DelimiterType::LeftParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken("int"), DelimiterType::LeftParenthesis));
  EXPECT_FALSE(isDelimiter(firstToken(""), DelimiterType::LeftParenthesis));
}

TEST(ParserTokenPredicates, IsOperatorMatchesTheOperatorType) {
  EXPECT_TRUE(isOperator(firstToken("+"), OperatorType::Add));
  EXPECT_TRUE(isOperator(firstToken("-"), OperatorType::Subtract));
  EXPECT_TRUE(isOperator(firstToken("**"), OperatorType::Exponentiate));
  EXPECT_TRUE(isOperator(firstToken("=="), OperatorType::Equal));
  EXPECT_TRUE(isOperator(firstToken("="), OperatorType::Assign));
  EXPECT_TRUE(isOperator(firstToken("+="), OperatorType::UnaryAdd));
  EXPECT_TRUE(isOperator(firstToken("not"), OperatorType::LogicalNot));
  EXPECT_TRUE(isOperator(firstToken("and"), OperatorType::LogicalAnd));
  EXPECT_TRUE(isOperator(firstToken("or"), OperatorType::LogicalOr));
}

TEST(ParserTokenPredicates, IsOperatorRejectsOtherOperators) {
  EXPECT_FALSE(isOperator(firstToken("+"), OperatorType::Subtract));
  EXPECT_FALSE(isOperator(firstToken("+="), OperatorType::Add));
  EXPECT_FALSE(isOperator(firstToken("*"), OperatorType::Exponentiate));
}

TEST(ParserTokenPredicates, IsOperatorRejectsOtherTokenTypes) {
  EXPECT_FALSE(isOperator(firstToken("x"), OperatorType::UnaryAdd));
  EXPECT_FALSE(isOperator(firstToken("("), OperatorType::UnaryAdd));
  EXPECT_FALSE(isOperator(firstToken("False"), OperatorType::UnaryAdd));
  EXPECT_FALSE(isOperator(firstToken("1"), OperatorType::UnaryAdd));
  EXPECT_FALSE(isOperator(firstToken(""), OperatorType::UnaryAdd));
}

TEST(ParserTokenPredicates, IsKeywordMatchesTheKeywordType) {
  EXPECT_TRUE(isKeyword(firstToken("if"), KeywordType::If));
  EXPECT_TRUE(isKeyword(firstToken("def"), KeywordType::Def));
  EXPECT_TRUE(isKeyword(firstToken("None"), KeywordType::None));
  EXPECT_TRUE(isKeyword(firstToken("range"), KeywordType::Range));
  EXPECT_TRUE(isKeyword(firstToken("return"), KeywordType::Return));
}

TEST(ParserTokenPredicates, IsKeywordRejectsOtherKeywords) {
  EXPECT_FALSE(isKeyword(firstToken("if"), KeywordType::Elif));
  EXPECT_FALSE(isKeyword(firstToken("while"), KeywordType::For));
}

TEST(ParserTokenPredicates, IsKeywordRejectsOtherTokenTypes) {
  EXPECT_FALSE(isKeyword(firstToken("x"), KeywordType::False));
  EXPECT_FALSE(isKeyword(firstToken("+="), KeywordType::False));
  EXPECT_FALSE(isKeyword(firstToken("("), KeywordType::False));
  EXPECT_FALSE(isKeyword(firstToken("1"), KeywordType::False));
  EXPECT_FALSE(isKeyword(firstToken("iff"), KeywordType::If));
  EXPECT_FALSE(isKeyword(firstToken(""), KeywordType::False));
}

TEST(ParserTokenPredicates, IsAugassignAcceptsEveryAugmentedAssignment) {
  for (const auto *Text :
       {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "**=", "//="}) {
    EXPECT_TRUE(isAugassign(firstToken(Text))) << Text;
  }
}

TEST(ParserTokenPredicates, IsAugassignRejectsEverythingElse) {
  for (const auto *Text : {"=", "==", "+", "-", "*", "<<", "<=", ">=", "!=", "not", "x", "(", ":",
                           "1", "if", "int", ""}) {
    EXPECT_FALSE(isAugassign(firstToken(Text))) << Text;
  }
}

// =================================================================================================
// Expressions
// =================================================================================================

// Atoms
// -----

TEST(ParserAtoms, Identifier) {
  expectExpr("x", "x");
  expectExpr("snake_case_1", "snake_case_1");
}

TEST(ParserAtoms, IntegerLiterals) {
  expectExpr("0", "0");
  expectExpr("42", "42");
  expectExpr("0x10", "16");
  expectExpr("1_000", "1000");
}

TEST(ParserAtoms, FloatLiterals) {
  expectExpr("1.5", "1.5");
  expectExpr("2.0", "2.0");
  expectExpr("1e3", "1000.0");
}

TEST(ParserAtoms, BooleanLiterals) {
  expectExpr("True", "True");
  expectExpr("False", "False");
}

TEST(ParserAtoms, NoneIsNotAnExpression) { expectExprError("None"); }

TEST(ParserAtoms, TypeNamesAreNotExpressions) {
  expectExprError("int");
  expectExprError("float");
  expectExprError("bool");
}

TEST(ParserAtoms, KeywordsAreNotExpressions) {
  expectExprError("if");
  expectExprError("pass");
  expectExprError("range");
  expectExprError("return");
}

TEST(ParserAtoms, MissingExpressionIsAnError) {
  expectExprError("");
  expectExprError(")");
  expectExprError(",");
  expectExprError(":");
}

// Groups
// ------

TEST(ParserGroups, ParenthesesProduceNoNode) {
  expectExpr("(x)", "x");
  expectExpr("((x))", "x");
  expectExpr("(1)", "1");
}

TEST(ParserGroups, ParenthesesOverridePrecedence) {
  expectExpr("(a + b) * c", "(* (+ a b) c)");
  expectExpr("a * (b + c)", "(* a (+ b c))");
  expectExpr("-(a + b)", "(- (+ a b))");
  expectExpr("(a or b) and c", "(and (or a b) c)");
  expectExpr("(a ** b) ** c", "(** (** a b) c)");
}

TEST(ParserGroups, ParenthesesOverrideAssociativity) {
  expectExpr("a - (b - c)", "(- a (- b c))");
  expectExpr("a or (b or c)", "(or a (or b c))");
}

TEST(ParserGroups, GroupMayContainAConditionalExpression) {
  expectExpr("(a if b else c) + d", "(+ (ifexpr b a c) d)");
}

TEST(ParserGroups, UnbalancedParenthesesAreAnError) {
  expectExprError("(a");
  expectExprError("((a)");
  expectExprError("(a + b");
}

TEST(ParserGroups, EmptyParenthesesAreAnError) { expectExprError("()"); }

TEST(ParserGroups, TuplesAreNotSupported) {
  expectExprError("(a, b)");
  expectExprError("(a,)");
}

// Binary operators
// ----------------

struct BinaryOperatorCase {
  const char *Symbol;
  const char *Name;
};

const BinaryOperatorCase AllBinaryOperators[] = {
    {"or", "Or"},
    {"and", "And"},
    {"==", "Equal"},
    {"!=", "NotEqual"},
    {"<=", "LessOrEqual"},
    {"<", "Less"},
    {">=", "GreaterOrEqual"},
    {">", "Greater"},
    {"|", "BitwiseOr"},
    {"^", "BitwiseXor"},
    {"&", "BitwiseAnd"},
    {"<<", "LeftShift"},
    {">>", "RightShift"},
    {"+", "Add"},
    {"-", "Subtract"},
    {"*", "Multiply"},
    {"/", "FloatDivide"},
    {"//", "IntegerDivide"},
    {"%", "Modulo"},
    {"**", "Exponentiate"},
};

class ParserBinaryOperatorTest : public ::testing::TestWithParam<BinaryOperatorCase> {};

TEST_P(ParserBinaryOperatorTest, ParsesBetweenIdentifiers) {
  const std::string Op = GetParam().Symbol;
  expectExpr("a " + Op + " b", "(" + Op + " a b)");
}

TEST_P(ParserBinaryOperatorTest, ParsesBetweenLiterals) {
  const std::string Op = GetParam().Symbol;
  expectExpr("1 " + Op + " 2.5", "(" + Op + " 1 2.5)");
  expectExpr("True " + Op + " False", "(" + Op + " True False)");
}

TEST_P(ParserBinaryOperatorTest, MissingRightOperandIsAnError) {
  const std::string Op = GetParam().Symbol;
  expectExprError("a " + Op);
}

TEST_P(ParserBinaryOperatorTest, MissingLeftOperandIsAnError) {
  const std::string Op = GetParam().Symbol;
  // '+' and '-' are also prefix operators, so they are fine without a left operand.
  if (Op == "+" || Op == "-") {
    GTEST_SKIP();
  }
  expectExprError(Op + " b");
}

INSTANTIATE_TEST_SUITE_P(AllBinaryOperators, ParserBinaryOperatorTest,
                         ::testing::ValuesIn(AllBinaryOperators),
                         [](const ::testing::TestParamInfo<BinaryOperatorCase> &Info) {
                           return Info.param.Name;
                         });

// Precedence levels, from the loosest to the tightest binding, as given by the grammar.
const std::vector<std::vector<std::string>> PrecedenceLevels = {
    {"or"},       {"and"},    {"==", "!=", "<=", "<", ">=", ">"},
    {"|"},        {"^"},      {"&"},
    {"<<", ">>"}, {"+", "-"}, {"*", "/", "//", "%"},
    {"**"},
};

TEST(ParserPrecedence, TighterOperatorOnTheRightBindsFirst) {
  for (std::size_t Loose = 0; Loose < PrecedenceLevels.size(); ++Loose) {
    for (std::size_t Tight = Loose + 1; Tight < PrecedenceLevels.size(); ++Tight) {
      for (const auto &L : PrecedenceLevels[Loose]) {
        for (const auto &T : PrecedenceLevels[Tight]) {
          expectExpr("a " + L + " b " + T + " c", "(" + L + " a (" + T + " b c))");
        }
      }
    }
  }
}

TEST(ParserPrecedence, TighterOperatorOnTheLeftBindsFirst) {
  for (std::size_t Loose = 0; Loose < PrecedenceLevels.size(); ++Loose) {
    for (std::size_t Tight = Loose + 1; Tight < PrecedenceLevels.size(); ++Tight) {
      for (const auto &L : PrecedenceLevels[Loose]) {
        for (const auto &T : PrecedenceLevels[Tight]) {
          expectExpr("a " + T + " b " + L + " c", "(" + L + " (" + T + " a b) c)");
        }
      }
    }
  }
}

TEST(ParserPrecedence, OperatorsOfTheSameLevelAreLeftAssociative) {
  for (const auto &Level : PrecedenceLevels) {
    for (const auto &First : Level) {
      for (const auto &Second : Level) {
        if (First == "**") {
          continue; // right-associative, tested below
        }
        expectExpr("a " + First + " b " + Second + " c", "(" + Second + " (" + First + " a b) c)");
      }
    }
  }
}

TEST(ParserPrecedence, LongChainsAreLeftAssociative) {
  expectExpr("a - b - c - d", "(- (- (- a b) c) d)");
  expectExpr("a or b or c or d", "(or (or (or a b) c) d)");
  expectExpr("a + b * c - d", "(- (+ a (* b c)) d)");
}

// The grammar says all binary operator rules fold into left-nested nodes, comparisons included.
TEST(ParserPrecedence, ChainedComparisonsFoldToTheLeft) {
  expectExpr("a < b < c", "(< (< a b) c)");
  expectExpr("a == b != c <= d", "(<= (!= (== a b) c) d)");
}

TEST(ParserPrecedence, ExponentiationIsRightAssociative) {
  expectExpr("a ** b ** c", "(** a (** b c))");
  expectExpr("a ** b ** c ** d", "(** a (** b (** c d)))");
}

TEST(ParserPrecedence, MixedExpression) {
  expectExpr("a or b and not c == d | e ^ f & g << h + i * -j ** k",
             "(or a (and b (not (== c (| d (^ e (& f (<< g (+ h (* i (- (** j k))))))))))))");
  expectExpr("a * b + c << d & e ^ f | g == h and i or j",
             "(or (and (== (| (^ (& (<< (+ (* a b) c) d) e) f) g) h) i) j)");
}

// Unary operators
// ---------------

TEST(ParserUnaryOperators, Prefixes) {
  expectExpr("-a", "(- a)");
  expectExpr("+a", "(+ a)");
  expectExpr("~a", "(~ a)");
  expectExpr("not a", "(not a)");
  expectExpr("-1", "(- 1)");
  expectExpr("-1.5", "(- 1.5)");
}

TEST(ParserUnaryOperators, Nesting) {
  expectExpr("--a", "(- (- a))");
  expectExpr("- - a", "(- (- a))");
  expectExpr("-+~a", "(- (+ (~ a)))");
  expectExpr("not not a", "(not (not a))");
  expectExpr("not -a", "(not (- a))");
}

TEST(ParserUnaryOperators, ArithmeticPrefixesBindTighterThanBinaryOperatorsExceptPower) {
  expectExpr("-a * b", "(* (- a) b)");
  expectExpr("a * -b", "(* a (- b))");
  expectExpr("-a + -b", "(+ (- a) (- b))");
  expectExpr("~a & b", "(& (~ a) b)");
  expectExpr("a - -b", "(- a (- b))");
}

TEST(ParserUnaryOperators, PowerBindsTighterThanPrefixOnItsLeft) {
  expectExpr("-a ** b", "(- (** a b))");
  expectExpr("~a ** b", "(~ (** a b))");
}

TEST(ParserUnaryOperators, PowerRightOperandMayHaveAPrefix) {
  expectExpr("a ** -b", "(** a (- b))");
  expectExpr("a ** -b ** c", "(** a (- (** b c)))");
  expectExpr("a ** ~+b", "(** a (~ (+ b)))");
}

TEST(ParserUnaryOperators, NotBindsLooserThanComparisons) {
  expectExpr("not a == b", "(not (== a b))");
  expectExpr("not a < b | c", "(not (< a (| b c)))");
}

TEST(ParserUnaryOperators, NotBindsTighterThanAndOr) {
  expectExpr("not a and b", "(and (not a) b)");
  expectExpr("a and not b", "(and a (not b))");
  expectExpr("not a or not b", "(or (not a) (not b))");
}

TEST(ParserUnaryOperators, NotIsNotAValidOperandOfArithmeticOperators) {
  expectExprError("-not a");
  expectExprError("a + not b");
  expectExprError("a == not b");
  expectExprError("a ** not b");
}

TEST(ParserUnaryOperators, MissingOperandIsAnError) {
  expectExprError("-");
  expectExprError("not");
  expectExprError("~");
}

// Conditional expressions
// -----------------------

TEST(ParserConditionalExpressions, Basic) { expectExpr("a if b else c", "(ifexpr b a c)"); }

TEST(ParserConditionalExpressions, BindsLooserThanOr) {
  expectExpr("a or b if c or d else e or f", "(ifexpr (or c d) (or a b) (or e f))");
  expectExpr("not a if not b else not c", "(ifexpr (not b) (not a) (not c))");
}

TEST(ParserConditionalExpressions, IsRightAssociative) {
  expectExpr("a if b else c if d else e", "(ifexpr b a (ifexpr d c e))");
}

TEST(ParserConditionalExpressions, OperandsMayBeArbitraryDisjunctions) {
  expectExpr("x + 1 if x > 0 else -x", "(ifexpr (> x 0) (+ x 1) (- x))");
}

TEST(ParserConditionalExpressions, ConditionMayBeAParenthesizedConditional) {
  expectExpr("a if (b if c else d) else e", "(ifexpr (ifexpr c b d) a e)");
}

TEST(ParserConditionalExpressions, ConditionMustBeADisjunction) {
  expectExprError("a if b if c else d else e");
}

TEST(ParserConditionalExpressions, MissingPartsAreAnError) {
  expectExprError("a if b");
  expectExprError("a if b else");
  expectExprError("a if else c");
  expectExprError("if b else c");
}

// Calls
// -----

TEST(ParserCalls, NoArguments) { expectExpr("f()", "(call)"); }

TEST(ParserCalls, Arguments) {
  expectExpr("f(1)", "(call 1)");
  expectExpr("f(1, 2)", "(call 1 2)");
  expectExpr("f(a, b, c)", "(call a b c)");
}

TEST(ParserCalls, TrailingComma) {
  expectExpr("f(1,)", "(call 1)");
  expectExpr("f(1, 2,)", "(call 1 2)");
}

TEST(ParserCalls, ArgumentsAreExpressions) {
  expectExpr("f(a + b, not c, d if e else f)", "(call (+ a b) (not c) (ifexpr e d f))");
}

TEST(ParserCalls, NestedCalls) { expectExpr("f(g(1), h())", "(call (call 1) (call))"); }

TEST(ParserCalls, CallBindsTighterThanEveryOperator) {
  expectExpr("-f(x)", "(- (call x))");
  expectExpr("f(x) ** 2", "(** (call x) 2)");
  expectExpr("2 ** f(x)", "(** 2 (call x))");
  expectExpr("f(a) + g(b)", "(+ (call a) (call b))");
  expectExpr("not f(x)", "(not (call x))");
}

TEST(ParserCalls, DISABLED_ChainedCallsTakeTheirOwnArguments) {
  // The callee is not stored in the AST, so only the outermost call's arguments are visible.
  expectExpr("f(1)(2, 3)", "(call 2 3)");
}

TEST(ParserCalls, MalformedArgumentListsAreAnError) {
  expectExprError("f(");
  expectExprError("f(1");
  expectExprError("f(1,");
  expectExprError("f(,)");
  expectExprError("f(, 1)");
  expectExprError("f(1,,)");
  expectExprError("f(1 2)");
}

TEST(ParserCalls, KeywordArgumentsAreNotSupported) { expectExprError("f(a=1)"); }

// The expression rule stops at the first token that cannot continue the expression
// ------------------------------------------------------------------------------------

TEST(ParserExpressionEnd, StopsBeforeTokensThatCannotContinueTheExpression) {
  expectParse(NodeRule{parseExpression}, "a b", "a", Id("b"));
  expectParse(NodeRule{parseExpression}, "a + b = c", "(+ a b)", Op(OperatorType::Assign));
  expectParse(NodeRule{parseExpression}, "a += 1", "a", Op(OperatorType::UnaryAdd));
  expectParse(NodeRule{parseExpression}, "a, b", "a", Delim(DelimiterType::Comma));
  expectParse(NodeRule{parseExpression}, "a: int", "a", Delim(DelimiterType::Colon));
  expectParse(NodeRule{parseExpression}, "a)", "a", Delim(DelimiterType::RightParenthesis));
  expectParse(NodeRule{parseExpression}, "a; b", "a", Delim(DelimiterType::Semicolon));
  expectParse(NodeRule{parseExpression}, "a else b", "a", Kw(KeywordType::Else));
}

// =================================================================================================
// Expression sub-rules
// =================================================================================================

TEST(ParserExpressionRules, Disjunction) {
  expectParse(NodeRule{parseDisjunction}, "a or b", "(or a b)", Eof());
  expectParse(NodeRule{parseDisjunction}, "a or b if c else d", "(or a b)", Kw(KeywordType::If));
}

TEST(ParserExpressionRules, Conjunction) {
  expectParse(NodeRule{parseConjunction}, "a and b and c", "(and (and a b) c)", Eof());
  expectParse(NodeRule{parseConjunction}, "a and b or c", "(and a b)", Op(OperatorType::LogicalOr));
}

TEST(ParserExpressionRules, Inversion) {
  expectParse(NodeRule{parseInversion}, "not a", "(not a)", Eof());
  expectParse(NodeRule{parseInversion}, "a < b", "(< a b)", Eof());
  expectParse(NodeRule{parseInversion}, "not a and b", "(not a)", Op(OperatorType::LogicalAnd));
}

TEST(ParserExpressionRules, Comparison) {
  expectParse(NodeRule{parseComparison}, "a < b", "(< a b)", Eof());
  expectParse(NodeRule{parseComparison}, "a | b", "(| a b)", Eof());
  expectParse(NodeRule{parseComparison}, "a < b and c", "(< a b)", Op(OperatorType::LogicalAnd));
  expectParseError(NodeRule{parseComparison}, "not a");
}

TEST(ParserExpressionRules, BitwiseOr) {
  expectParse(NodeRule{parseBitwiseOr}, "a | b ^ c", "(| a (^ b c))", Eof());
  expectParse(NodeRule{parseBitwiseOr}, "a | b < c", "(| a b)", Op(OperatorType::Less));
}

TEST(ParserExpressionRules, BitwiseXor) {
  expectParse(NodeRule{parseBitwiseXor}, "a ^ b & c", "(^ a (& b c))", Eof());
  expectParse(NodeRule{parseBitwiseXor}, "a ^ b | c", "(^ a b)", Op(OperatorType::BitwiseOr));
}

TEST(ParserExpressionRules, BitwiseAnd) {
  expectParse(NodeRule{parseBitwiseAnd}, "a & b << c", "(& a (<< b c))", Eof());
  expectParse(NodeRule{parseBitwiseAnd}, "a & b ^ c", "(& a b)", Op(OperatorType::BitwiseXor));
}

TEST(ParserExpressionRules, ShiftExpr) {
  expectParse(NodeRule{parseShiftExpr}, "a << b + c", "(<< a (+ b c))", Eof());
  expectParse(NodeRule{parseShiftExpr}, "a >> b & c", "(>> a b)", Op(OperatorType::BitwiseAnd));
}

TEST(ParserExpressionRules, Sum) {
  expectParse(NodeRule{parseSum}, "a + b * c", "(+ a (* b c))", Eof());
  expectParse(NodeRule{parseSum}, "a - b << c", "(- a b)", Op(OperatorType::LeftShift));
}

TEST(ParserExpressionRules, Term) {
  expectParse(NodeRule{parseTerm}, "a * -b", "(* a (- b))", Eof());
  expectParse(NodeRule{parseTerm}, "a // b + c", "(// a b)", Op(OperatorType::Add));
}

TEST(ParserExpressionRules, Factor) {
  expectParse(NodeRule{parseFactor}, "-a", "(- a)", Eof());
  expectParse(NodeRule{parseFactor}, "a ** b", "(** a b)", Eof());
  expectParse(NodeRule{parseFactor}, "-a * b", "(- a)", Op(OperatorType::Multiply));
}

TEST(ParserExpressionRules, Power) {
  expectParse(NodeRule{parsePower}, "a ** b", "(** a b)", Eof());
  expectParse(NodeRule{parsePower}, "a ** -b", "(** a (- b))", Eof());
  expectParse(NodeRule{parsePower}, "a ** b * c", "(** a b)", Op(OperatorType::Multiply));
  expectParseError(NodeRule{parsePower}, "-a");
}

TEST(ParserExpressionRules, Primary) {
  expectParse(NodeRule{parsePrimary}, "a", "a", Eof());
  expectParse(NodeRule{parsePrimary}, "f(1)", "(call 1)", Eof());
  expectParse(NodeRule{parsePrimary}, "f(1) ** 2", "(call 1)", Op(OperatorType::Exponentiate));
  expectParseError(NodeRule{parsePrimary}, "-a");
}

TEST(ParserExpressionRules, Atom) {
  expectParse(NodeRule{parseAtom}, "a", "a", Eof());
  expectParse(NodeRule{parseAtom}, "7", "7", Eof());
  expectParse(NodeRule{parseAtom}, "0.25", "0.25", Eof());
  expectParse(NodeRule{parseAtom}, "True", "True", Eof());
  expectParse(NodeRule{parseAtom}, "False", "False", Eof());
  expectParse(NodeRule{parseAtom}, "(a + b)", "(+ a b)", Eof());
  expectParse(NodeRule{parseAtom}, "f(1)", "f", Delim(DelimiterType::LeftParenthesis));
  expectParseError(NodeRule{parseAtom}, "-a");
  expectParseError(NodeRule{parseAtom}, "None");
}

TEST(ParserExpressionRules, Group) {
  expectParse(NodeRule{parseGroup}, "(a)", "a", Eof());
  expectParse(NodeRule{parseGroup}, "(a if b else c)", "(ifexpr b a c)", Eof());
  expectParse(NodeRule{parseGroup}, "(a) + b", "a", Op(OperatorType::Add));
  expectParseError(NodeRule{parseGroup}, "a");
  expectParseError(NodeRule{parseGroup}, "(a");
}

TEST(ParserExpressionRules, Identifier) {
  expectParse(NodeRule{parseIdentifier}, "x", "x", Eof());
  expectParse(NodeRule{parseIdentifier}, "x + 1", "x", Op(OperatorType::Add));
  expectParseError(NodeRule{parseIdentifier}, "1");
  expectParseError(NodeRule{parseIdentifier}, "True");
  expectParseError(NodeRule{parseIdentifier}, "int");
  expectParseError(NodeRule{parseIdentifier}, "if");
  expectParseError(NodeRule{parseIdentifier}, "(x)");
}

TEST(ParserExpressionRules, Arguments) {
  expectParse(ListRule{parseArguments}, "1)", "[1]", Delim(DelimiterType::RightParenthesis));
  expectParse(ListRule{parseArguments}, "1, 2)", "[1 2]", Delim(DelimiterType::RightParenthesis));
  expectParse(ListRule{parseArguments}, "1,)", "[1]", Delim(DelimiterType::RightParenthesis));
  expectParse(ListRule{parseArguments}, "a + b, f(c),)", "[(+ a b) (call c)]",
              Delim(DelimiterType::RightParenthesis));
  expectParseError(ListRule{parseArguments}, ", 1)");
  expectParseError(ListRule{parseArguments}, "1,,)");
}

// =================================================================================================
// Types
// =================================================================================================

TEST(ParserTypes, TypeExpression) {
  expectParse(NodeRule{parseTypeExpression}, "int", "int", Eof());
  expectParse(NodeRule{parseTypeExpression}, "float", "float", Eof());
  expectParse(NodeRule{parseTypeExpression}, "bool", "bool", Eof());
  expectParse(NodeRule{parseTypeExpression}, "int = 1", "int", Op(OperatorType::Assign));
}

TEST(ParserTypes, NoneIsNotATypeExpression) {
  expectParseError(NodeRule{parseTypeExpression}, "None");
}

TEST(ParserTypes, OtherTokensAreNotTypeExpressions) {
  expectParseError(NodeRule{parseTypeExpression}, "x");
  expectParseError(NodeRule{parseTypeExpression}, "string");
  expectParseError(NodeRule{parseTypeExpression}, "1");
  expectParseError(NodeRule{parseTypeExpression}, "True");
  expectParseError(NodeRule{parseTypeExpression}, "(int)");
  expectParseError(NodeRule{parseTypeExpression}, "");
}

TEST(ParserTypes, ReturnAnnotation) {
  expectParse(NodeRule{parseReturnAnnotation}, "int", "int", Eof());
  expectParse(NodeRule{parseReturnAnnotation}, "float", "float", Eof());
  expectParse(NodeRule{parseReturnAnnotation}, "bool", "bool", Eof());
  expectParse(NodeRule{parseReturnAnnotation}, "None", "None", Eof());
  expectParse(NodeRule{parseReturnAnnotation}, "None:", "None", Delim(DelimiterType::Colon));
  expectParseError(NodeRule{parseReturnAnnotation}, "x");
  expectParseError(NodeRule{parseReturnAnnotation}, "False");
  expectParseError(NodeRule{parseReturnAnnotation}, ":");
}

TEST(ParserTypes, Annotation) {
  expectParse(NodeRule{parseAnnotation}, ": int", "int", Eof());
  expectParse(NodeRule{parseAnnotation}, ":float", "float", Eof());
  expectParse(NodeRule{parseAnnotation}, ": bool = True", "bool", Op(OperatorType::Assign));
  expectParseError(NodeRule{parseAnnotation}, "int");
  expectParseError(NodeRule{parseAnnotation}, ": None");
  expectParseError(NodeRule{parseAnnotation}, ": x");
  expectParseError(NodeRule{parseAnnotation}, ":");
  expectParseError(NodeRule{parseAnnotation}, "-> int");
}

// =================================================================================================
// Simple statements
// =================================================================================================

// Expression statements
// ---------------------

TEST(ParserExpressionStatements, ExpressionIsAStatement) {
  expectStmt("f(x)\n", "(call x)");
  expectStmt("x\n", "x");
  expectStmt("1 + 2\n", "(+ 1 2)");
  expectStmt("a if b else c\n", "(ifexpr b a c)");
}

TEST(ParserExpressionStatements, IncompleteExpressionIsAnError) {
  expectFileError("1 +\n");
  expectFileError("f(\n");
}

TEST(ParserExpressionStatements, TwoExpressionsOnALineAreAnError) {
  expectFileError("a b\n");
  expectFileError("1 2\n");
  expectFileError("a, b\n");
}

// Assignments
// -----------

TEST(ParserAssignments, SimpleAssignment) {
  expectStmt("x = 1\n", "(assign [x] 1)");
  expectStmt("x = y\n", "(assign [x] y)");
}

TEST(ParserAssignments, ValueIsAnExpression) {
  expectStmt("x = a + b * c\n", "(assign [x] (+ a (* b c)))");
  expectStmt("x = a if b else c\n", "(assign [x] (ifexpr b a c))");
  expectStmt("x = f(1, 2)\n", "(assign [x] (call 1 2))");
  expectStmt("x = not a\n", "(assign [x] (not a))");
}

TEST(ParserAssignments, ChainedAssignment) {
  expectStmt("x = y = 1\n", "(assign [x y] 1)");
  expectStmt("a = b = c = d\n", "(assign [a b c] d)");
}

// Only a NAME is a valid target, but that is a semantic check done after parsing.
TEST(ParserAssignments, NonNameTargetsAreAcceptedByTheParser) {
  expectStmt("1 = x\n", "(assign [1] x)");
  expectStmt("f(x) = 1\n", "(assign [(call x)] 1)");
  expectStmt("a + b = c = 1\n", "(assign [(+ a b) c] 1)");
}

TEST(ParserAssignments, MissingValueIsAnError) {
  expectFileError("x =\n");
  expectFileError("x = y =\n");
}

TEST(ParserAssignments, MissingTargetIsAnError) { expectFileError("= 1\n"); }

TEST(ParserAssignments, MultipleTargetsAreNotSupported) {
  expectFileError("x, y = 1, 2\n");
  expectFileError("x, y = 1\n");
}

TEST(ParserAssignments, MixingWithAugmentedAssignmentIsAnError) {
  expectFileError("x = y += 1\n");
  expectFileError("x = 1 += 2\n");
}

TEST(ParserAssignments, WalrusIsNotSupported) { expectFileError("x := 1\n"); }

// Declarations
// ------------

TEST(ParserDeclarations, DeclarationWithValue) {
  expectStmt("x: int = 1\n", "(decl x int 1)");
  expectStmt("x: float = 1.5\n", "(decl x float 1.5)");
  expectStmt("x: bool = True\n", "(decl x bool True)");
}

TEST(ParserDeclarations, DeclarationWithoutValue) {
  expectStmt("x: int\n", "(decl x int _)");
  expectStmt("x: float\n", "(decl x float _)");
  expectStmt("x: bool\n", "(decl x bool _)");
}

TEST(ParserDeclarations, ValueIsAnExpression) {
  expectStmt("x: int = a * (b + 1)\n", "(decl x int (* a (+ b 1)))");
  expectStmt("x: bool = a < b and c\n", "(decl x bool (and (< a b) c))");
}

// Only a NAME is a valid target, but that is a semantic check done after parsing.
TEST(ParserDeclarations, NonNameTargetsAreAcceptedByTheParser) {
  expectStmt("f(x): int = 1\n", "(decl (call x) int 1)");
}

TEST(ParserDeclarations, NoneIsNotAVariableType) {
  expectFileError("x: None\n");
  expectFileError("x: None = 1\n");
}

TEST(ParserDeclarations, InvalidTypesAreAnError) {
  expectFileError("x: y = 1\n");
  expectFileError("x: 1 = 1\n");
  expectFileError("x: = 1\n");
  expectFileError("x:\n");
}

TEST(ParserDeclarations, ChainedAssignmentAfterDeclarationIsAnError) {
  expectFileError("x: int = y = 1\n");
}

TEST(ParserDeclarations, MissingValueAfterEqualsIsAnError) { expectFileError("x: int =\n"); }

TEST(ParserDeclarations, AugmentedAssignmentAfterDeclarationIsAnError) {
  expectFileError("x: int += 1\n");
}

// Augmented assignments
// ---------------------

struct AugassignCase {
  const char *Symbol;
  const char *Name;
};

class ParserAugassignTest : public ::testing::TestWithParam<AugassignCase> {};

TEST_P(ParserAugassignTest, Parses) {
  const std::string Op = GetParam().Symbol;
  expectStmt("x " + Op + " 1\n", "(augassign " + Op + " x 1)");
}

TEST_P(ParserAugassignTest, ValueIsAnExpression) {
  const std::string Op = GetParam().Symbol;
  expectStmt("x " + Op + " a + f(b)\n", "(augassign " + Op + " x (+ a (call b)))");
}

TEST_P(ParserAugassignTest, MissingValueIsAnError) {
  const std::string Op = GetParam().Symbol;
  expectFileError("x " + Op + "\n");
}

TEST_P(ParserAugassignTest, ChainingIsAnError) {
  const std::string Op = GetParam().Symbol;
  expectFileError("x " + Op + " y " + Op + " 1\n");
  expectFileError("x " + Op + " y = 1\n");
}

INSTANTIATE_TEST_SUITE_P(
    AllAugassigns, ParserAugassignTest,
    ::testing::Values(AugassignCase{"+=", "Add"}, AugassignCase{"-=", "Subtract"},
                      AugassignCase{"*=", "Multiply"}, AugassignCase{"/=", "FloatDivide"},
                      AugassignCase{"%=", "Modulo"}, AugassignCase{"&=", "BitwiseAnd"},
                      AugassignCase{"|=", "BitwiseOr"}, AugassignCase{"^=", "BitwiseXor"},
                      AugassignCase{"<<=", "LeftShift"}, AugassignCase{">>=", "RightShift"},
                      AugassignCase{"**=", "Exponentiate"}, AugassignCase{"//=", "IntegerDivide"}),
    [](const ::testing::TestParamInfo<AugassignCase> &Info) { return Info.param.Name; });

// Only a NAME is a valid target, but that is a semantic check done after parsing.
TEST(ParserAugassign, NonNameTargetsAreAcceptedByTheParser) {
  expectStmt("f(x) += 1\n", "(augassign += (call x) 1)");
}

// Assignment rule
// ---------------

TEST(ParserAssignmentRule, ParsesEveryVariant) {
  expectParse(NodeRule{parseAssignment}, "f(x)", "(call x)", Eof());
  expectParse(NodeRule{parseAssignment}, "x: int", "(decl x int _)", Eof());
  expectParse(NodeRule{parseAssignment}, "x: int = 1", "(decl x int 1)", Eof());
  expectParse(NodeRule{parseAssignment}, "x -= 1", "(augassign -= x 1)", Eof());
  expectParse(NodeRule{parseAssignment}, "x = y = 1", "(assign [x y] 1)", Eof());
}

TEST(ParserAssignmentRule, StopsAtTheEndOfTheStatement) {
  expectParse(NodeRule{parseAssignment}, "x = 1; y = 2", "(assign [x] 1)",
              Delim(DelimiterType::Semicolon));
}

// Return
// ------

TEST(ParserReturn, WithoutValue) {
  expectStmt("return\n", "(return _)");
  expectParse(NodeRule{parseReturnStmt}, "return", "(return _)", Eof());
}

TEST(ParserReturn, WithValue) {
  expectStmt("return 1\n", "(return 1)");
  expectStmt("return a + b\n", "(return (+ a b))");
  expectStmt("return a if b else c\n", "(return (ifexpr b a c))");
  expectParse(NodeRule{parseReturnStmt}, "return x", "(return x)", Eof());
}

TEST(ParserReturn, MultipleValuesAreAnError) { expectFileError("return a, b\n"); }

TEST(ParserReturn, RuleRequiresTheKeyword) {
  expectParseError(NodeRule{parseReturnStmt}, "x");
  expectParseError(NodeRule{parseReturnStmt}, "pass");
}

// pass, break, continue
// ---------------------

TEST(ParserKeywordStatements, Pass) {
  expectStmt("pass\n", "pass");
  expectParse(NodeRule{parsePassStmt}, "pass", "pass", Eof());
  expectParseError(NodeRule{parsePassStmt}, "break");
  expectParseError(NodeRule{parsePassStmt}, "x");
}

TEST(ParserKeywordStatements, Break) {
  expectStmt("break\n", "break");
  expectParse(NodeRule{parseBreakStmt}, "break", "break", Eof());
  expectParseError(NodeRule{parseBreakStmt}, "continue");
  expectParseError(NodeRule{parseBreakStmt}, "x");
}

TEST(ParserKeywordStatements, Continue) {
  expectStmt("continue\n", "continue");
  expectParse(NodeRule{parseContinueStmt}, "continue", "continue", Eof());
  expectParseError(NodeRule{parseContinueStmt}, "pass");
  expectParseError(NodeRule{parseContinueStmt}, "x");
}

TEST(ParserKeywordStatements, TrailingTokensAreAnError) {
  expectFileError("pass x\n");
  expectFileError("break 1\n");
  expectFileError("continue x\n");
  expectFileError("pass = 1\n");
}

// global
// ------

TEST(ParserGlobal, SingleName) {
  expectStmt("global x\n", "(global x)");
  expectParse(NodeRule{parseGlobalStmt}, "global x", "(global x)", Eof());
}

TEST(ParserGlobal, SeveralNames) {
  expectStmt("global x, y\n", "(global x y)");
  expectStmt("global a, b, c\n", "(global a b c)");
}

TEST(ParserGlobal, RuleStopsAfterTheLastName) {
  expectParse(NodeRule{parseGlobalStmt}, "global x y", "(global x)", Id("y"));
  expectParse(NodeRule{parseGlobalStmt}, "global x; y", "(global x)",
              Delim(DelimiterType::Semicolon));
}

TEST(ParserGlobal, MissingNameIsAnError) {
  expectFileError("global\n");
  expectParseError(NodeRule{parseGlobalStmt}, "global");
}

TEST(ParserGlobal, TrailingCommaIsAnError) {
  expectFileError("global x,\n");
  expectParseError(NodeRule{parseGlobalStmt}, "global x,");
}

TEST(ParserGlobal, NonNamesAreAnError) {
  expectFileError("global 1\n");
  expectFileError("global x, 1\n");
  expectFileError("global f(x)\n");
  expectFileError("global x + 1\n");
  expectFileError("global (x)\n");
}

TEST(ParserGlobal, MissingCommaIsAnError) { expectFileError("global x y\n"); }

TEST(ParserGlobal, RuleRequiresTheKeyword) { expectParseError(NodeRule{parseGlobalStmt}, "x"); }

// Simple statement rule
// ---------------------

TEST(ParserSimpleStmtRule, ParsesOneSimpleStatement) {
  expectParse(NodeRule{parseSimpleStmt}, "pass", "pass", Eof());
  expectParse(NodeRule{parseSimpleStmt}, "break", "break", Eof());
  expectParse(NodeRule{parseSimpleStmt}, "continue", "continue", Eof());
  expectParse(NodeRule{parseSimpleStmt}, "return 1", "(return 1)", Eof());
  expectParse(NodeRule{parseSimpleStmt}, "global x", "(global x)", Eof());
  expectParse(NodeRule{parseSimpleStmt}, "x = 1", "(assign [x] 1)", Eof());
  expectParse(NodeRule{parseSimpleStmt}, "pass; break", "pass", Delim(DelimiterType::Semicolon));
}

TEST(ParserSimpleStmtRule, CompoundStatementsAreNotSimple) {
  expectParseError(NodeRule{parseSimpleStmt}, "if x: pass");
  expectParseError(NodeRule{parseSimpleStmt}, "while x: pass");
  expectParseError(NodeRule{parseSimpleStmt}, "for i in range(3): pass");
  expectParseError(NodeRule{parseSimpleStmt}, "def f() -> None: pass");
}

// Several simple statements on one line
// -------------------------------------

TEST(ParserSimpleStmts, SemicolonSeparatesStatements) {
  expectFile("pass; break\n", {"pass", "break"});
  expectFile("x = 1; y = 2; z = 3\n", {"(assign [x] 1)", "(assign [y] 2)", "(assign [z] 3)"});
  expectFile("x: int = 1; x += 1; f(x)\n", {"(decl x int 1)", "(augassign += x 1)", "(call x)"});
}

TEST(ParserSimpleStmts, TrailingSemicolon) {
  expectFile("pass;\n", {"pass"});
  expectFile("pass; break;\n", {"pass", "break"});
}

TEST(ParserSimpleStmts, DoubleSemicolonIsAnError) {
  expectFileError("pass;;\n");
  expectFileError("pass;; break\n");
}

TEST(ParserSimpleStmts, LeadingSemicolonIsAnError) {
  expectFileError(";\n");
  expectFileError("; pass\n");
}

TEST(ParserSimpleStmts, CompoundStatementAfterSemicolonIsAnError) {
  expectFileError("pass; if x: pass\n");
  expectFileError("x = 1; while x: pass\n");
}

TEST(ParserSimpleStmts, RuleConsumesTheNewLine) {
  expectParse(ListRule{parseSimpleStmts}, "pass\n", "[pass]", Eof());
  expectParse(ListRule{parseSimpleStmts}, "pass; break\n", "[pass break]", Eof());
  expectParse(ListRule{parseSimpleStmts}, "pass;\n", "[pass]", Eof());
  expectParse(ListRule{parseSimpleStmts}, "pass\nbreak\n", "[pass]", Kw(KeywordType::Break));
  expectParse(ListRule{parseSimpleStmts}, "pass; x = 1\nbreak\n", "[pass (assign [x] 1)]",
              Kw(KeywordType::Break));
}

TEST(ParserSimpleStmts, RuleRejectsMalformedLines) {
  expectParseError(ListRule{parseSimpleStmts}, "pass break\n");
  expectParseError(ListRule{parseSimpleStmts}, "pass;;\n");
  expectParseError(ListRule{parseSimpleStmts}, "if x: pass\n");
}

// =================================================================================================
// Compound statements
// =================================================================================================

// if / elif / else
// ----------------

TEST(ParserIf, IndentedBlock) { expectStmt("if x:\n    pass\n", "(if x (block pass) _)"); }

TEST(ParserIf, BlockWithSeveralStatements) {
  expectStmt("if x:\n    y = 1\n    z = 2\n", "(if x (block (assign [y] 1) (assign [z] 2)) _)");
  expectStmt("if x:\n    y = 1; z = 2\n    pass\n",
             "(if x (block (assign [y] 1) (assign [z] 2) pass) _)");
}

TEST(ParserIf, BlockOnTheSameLine) {
  expectStmt("if x: pass\n", "(if x (block pass) _)");
  expectStmt("if x: y = 1; z = 2\n", "(if x (block (assign [y] 1) (assign [z] 2)) _)");
  expectStmt("if x: pass;\n", "(if x (block pass) _)");
}

TEST(ParserIf, ConditionIsAnExpression) {
  expectStmt("if a < b and not c: pass\n", "(if (and (< a b) (not c)) (block pass) _)");
  expectStmt("if a if b else c: pass\n", "(if (ifexpr b a c) (block pass) _)");
  expectStmt("if f(x): pass\n", "(if (call x) (block pass) _)");
}

TEST(ParserIf, Else) {
  expectStmt("if x:\n    a = 1\nelse:\n    a = 2\n",
             "(if x (block (assign [a] 1)) (block (assign [a] 2)))");
  expectStmt("if x: pass\nelse: break\n", "(if x (block pass) (block break))");
}

TEST(ParserIf, ElifIsANestedIf) {
  expectStmt("if a:\n    x = 1\nelif b:\n    x = 2\n",
             "(if a (block (assign [x] 1)) (if b (block (assign [x] 2)) _))");
}

TEST(ParserIf, ElifElse) {
  expectStmt("if a:\n    x = 1\nelif b:\n    x = 2\nelse:\n    x = 3\n",
             "(if a (block (assign [x] 1)) (if b (block (assign [x] 2)) (block (assign [x] 3))))");
}

TEST(ParserIf, SeveralElifs) {
  expectStmt("if a: pass\nelif b: break\nelif c: continue\nelse: return\n",
             "(if a (block pass) (if b (block break) (if c (block continue) (block (return _)))))");
}

TEST(ParserIf, NestedIfInsideABlock) {
  expectStmt("if a:\n    if b:\n        pass\n    else:\n        break\nelse:\n    continue\n",
             "(if a (block (if b (block pass) (block break))) (block continue))");
}

TEST(ParserIf, ElseAfterDedentBelongsToTheOuterIf) {
  expectStmt("if a:\n    if b:\n        pass\nelse:\n    break\n",
             "(if a (block (if b (block pass) _)) (block break))");
}

TEST(ParserIf, StatementAfterTheBlock) {
  expectFile("if x:\n    pass\ny = 1\n", {"(if x (block pass) _)", "(assign [y] 1)"});
  expectFile("if x: pass\ny = 1\n", {"(if x (block pass) _)", "(assign [y] 1)"});
}

TEST(ParserIf, MissingColonIsAnError) {
  expectFileError("if x\n    pass\n");
  expectFileError("if x pass\n");
}

TEST(ParserIf, MissingConditionIsAnError) { expectFileError("if: pass\n"); }

TEST(ParserIf, MissingBlockIsAnError) {
  expectFileError("if x:\n");
  expectFileError("if x:\n");
  expectFileError("if x:\npass\n");
}

TEST(ParserIf, CompoundStatementOnTheSameLineIsAnError) {
  expectFileError("if x: if y: pass\n");
  expectFileError("if x: while y: pass\n");
}

TEST(ParserIf, ElseOrElifWithoutIfIsAnError) {
  expectFileError("else: pass\n");
  expectFileError("elif x: pass\n");
  expectFileError("pass\nelse: pass\n");
}

TEST(ParserIf, ElifAfterElseIsAnError) {
  expectFileError("if a: pass\nelse: pass\nelif b: pass\n");
}

TEST(ParserIf, ElseWithConditionIsAnError) { expectFileError("if a: pass\nelse b: pass\n"); }

TEST(ParserIf, ElseWithoutColonIsAnError) { expectFileError("if a: pass\nelse pass\n"); }

TEST(ParserIf, RuleParsesTheWholeStatement) {
  expectParse(NodeRule{parseIfElifStmt}, "if a: pass\n", "(if a (block pass) _)", Eof());
  expectParse(NodeRule{parseIfElifStmt}, "if a: pass\nelif b: pass\nelse: pass\nx\n",
              "(if a (block pass) (if b (block pass) (block pass)))", Id("x"));
  expectParseError(NodeRule{parseIfElifStmt}, "while a: pass\n");
}

TEST(ParserElseBlock, Rule) {
  expectParse(NodeRule{parseElseBlock}, "else: pass\n", "(block pass)", Eof());
  expectParse(NodeRule{parseElseBlock}, "else:\n    pass\nx\n", "(block pass)", Id("x"));
  expectParseError(NodeRule{parseElseBlock}, "else pass\n");
  expectParseError(NodeRule{parseElseBlock}, "elif x: pass\n");
  expectParseError(NodeRule{parseElseBlock}, "pass\n");
}

// while
// -----

TEST(ParserWhile, IndentedBlock) {
  expectStmt("while x:\n    x -= 1\n", "(while x (block (augassign -= x 1)) _)");
}

TEST(ParserWhile, BlockOnTheSameLine) {
  expectStmt("while True: pass\n", "(while True (block pass) _)");
}

TEST(ParserWhile, ConditionIsAnExpression) {
  expectStmt("while i < n and not done: i += 1\n",
             "(while (and (< i n) (not done)) (block (augassign += i 1)) _)");
}

TEST(ParserWhile, Else) {
  expectStmt("while x:\n    break\nelse:\n    pass\n", "(while x (block break) (block pass))");
  expectStmt("while x: break\nelse: pass\n", "(while x (block break) (block pass))");
}

TEST(ParserWhile, BreakAndContinueInsideTheBlock) {
  expectStmt("while x:\n    if y:\n        break\n    continue\n",
             "(while x (block (if y (block break) _) continue) _)");
}

TEST(ParserWhile, ElifIsAnError) { expectFileError("while x: pass\nelif y: pass\n"); }

TEST(ParserWhile, MalformedStatementsAreAnError) {
  expectFileError("while: pass\n");
  expectFileError("while x pass\n");
  expectFileError("while x:\n");
  expectFileError("while x: pass\nelse pass\n");
}

TEST(ParserWhile, Rule) {
  expectParse(NodeRule{parseWhileStmt}, "while x: pass\ny\n", "(while x (block pass) _)", Id("y"));
  expectParseError(NodeRule{parseWhileStmt}, "if x: pass\n");
}

// for
// ---

TEST(ParserFor, RangeWithStop) {
  expectStmt("for i in range(10):\n    pass\n", "(for _ 10 _ (block pass) _)");
}

TEST(ParserFor, RangeWithStartAndStop) {
  expectStmt("for i in range(1, 10): pass\n", "(for 1 10 _ (block pass) _)");
}

TEST(ParserFor, RangeWithStartStopAndStep) {
  expectStmt("for i in range(1, 10, 2): pass\n", "(for 1 10 2 (block pass) _)");
}

TEST(ParserFor, RangeArgumentsAreExpressions) {
  expectStmt("for i in range(a + 1, f(n), -1): pass\n",
             "(for (+ a 1) (call n) (- 1) (block pass) _)");
}

TEST(ParserFor, Else) {
  expectStmt("for i in range(3):\n    break\nelse:\n    pass\n",
             "(for _ 3 _ (block break) (block pass))");
}

TEST(ParserFor, BlockWithSeveralStatements) {
  expectStmt("for i in range(n):\n    s += i\n    t *= i\n",
             "(for _ n _ (block (augassign += s i) (augassign *= t i)) _)");
}

TEST(ParserFor, WrongNumberOfRangeArgumentsIsAnError) {
  expectFileError("for i in range(): pass\n");
  expectFileError("for i in range(1, 2, 3, 4): pass\n");
}

TEST(ParserFor, TrailingCommaInRangeIsAnError) {
  expectFileError("for i in range(1,): pass\n");
  expectFileError("for i in range(1, 2,): pass\n");
  expectFileError("for i in range(1, 2, 3,): pass\n");
}

TEST(ParserFor, IterableMustBeRange) {
  expectFileError("for i in f(10): pass\n");
  expectFileError("for i in x: pass\n");
  expectFileError("for i in range: pass\n");
  expectFileError("for i in range 10: pass\n");
  expectFileError("for i in (range(10)): pass\n");
}

TEST(ParserFor, LoopVariableMustBeAName) {
  expectFileError("for 1 in range(3): pass\n");
  expectFileError("for f(x) in range(3): pass\n");
  expectFileError("for i, j in range(3): pass\n");
  expectFileError("for in range(3): pass\n");
}

TEST(ParserFor, MissingPartsAreAnError) {
  expectFileError("for i range(3): pass\n");
  expectFileError("for i in range(3) pass\n");
  expectFileError("for i in range(3):\n");
  expectFileError("for i in range(3: pass\n");
}

TEST(ParserFor, Rule) {
  expectParse(NodeRule{parseForStmt}, "for i in range(3): pass\nx\n", "(for _ 3 _ (block pass) _)",
              Id("x"));
  expectParseError(NodeRule{parseForStmt}, "while x: pass\n");
}

// Function definitions
// --------------------

TEST(ParserFunctionDef, NoParameters) {
  expectStmt("def f() -> None:\n    pass\n", "(def f [] None (block pass))");
  expectStmt("def f() -> None: pass\n", "(def f [] None (block pass))");
}

TEST(ParserFunctionDef, ReturnTypes) {
  expectStmt("def f() -> int: return 1\n", "(def f [] int (block (return 1)))");
  expectStmt("def f() -> float: return 1.5\n", "(def f [] float (block (return 1.5)))");
  expectStmt("def f() -> bool: return True\n", "(def f [] bool (block (return True)))");
  expectStmt("def f() -> None: return\n", "(def f [] None (block (return _)))");
}

TEST(ParserFunctionDef, Parameters) {
  expectStmt("def f(a: int) -> None: pass\n", "(def f [(param a int _)] None (block pass))");
  expectStmt("def f(a: int, b: float, c: bool) -> None: pass\n",
             "(def f [(param a int _) (param b float _) (param c bool _)] None (block pass))");
}

TEST(ParserFunctionDef, DefaultValues) {
  expectStmt("def f(a: int = 1) -> None: pass\n", "(def f [(param a int 1)] None (block pass))");
  expectStmt("def f(a: int, b: float = 2.0 * x) -> None: pass\n",
             "(def f [(param a int _) (param b float (* 2.0 x))] None (block pass))");
}

// A parameter without default after one with a default is a semantic check done after parsing.
TEST(ParserFunctionDef, DefaultOrderIsNotCheckedByTheParser) {
  expectStmt("def f(a: int = 1, b: int) -> None: pass\n",
             "(def f [(param a int 1) (param b int _)] None (block pass))");
}

TEST(ParserFunctionDef, TrailingCommaInParameters) {
  expectStmt("def f(a: int,) -> None: pass\n", "(def f [(param a int _)] None (block pass))");
  expectStmt("def f(a: int, b: int = 1,) -> None: pass\n",
             "(def f [(param a int _) (param b int 1)] None (block pass))");
}

TEST(ParserFunctionDef, BodyWithSeveralStatements) {
  expectStmt("def f(n: int) -> int:\n    r: int = n * 2\n    global g\n    return r\n",
             "(def f [(param n int _)] int (block (decl r int (* n 2)) (global g) (return r)))");
}

TEST(ParserFunctionDef, BodyWithCompoundStatements) {
  expectStmt("def f(n: int) -> int:\n"
             "    s: int = 0\n"
             "    for i in range(n):\n"
             "        if i % 2 == 0:\n"
             "            s += i\n"
             "    return s\n",
             "(def f [(param n int _)] int (block (decl s int 0) (for _ n _ (block (if (== (% i 2) "
             "0) (block (augassign += s i)) _)) _) (return s)))");
}

TEST(ParserFunctionDef, NestedFunction) {
  expectStmt("def f() -> None:\n    def g() -> None:\n        pass\n    g()\n",
             "(def f [] None (block (def g [] None (block pass)) (call)))");
}

TEST(ParserFunctionDef, StatementAfterTheBody) {
  expectFile("def f() -> int:\n    return 1\nx: int = f()\n",
             {"(def f [] int (block (return 1)))", "(decl x int (call))"});
}

TEST(ParserFunctionDef, MissingReturnAnnotationIsAnError) {
  expectFileError("def f(): pass\n");
  expectFileError("def f() ->: pass\n");
}

TEST(ParserFunctionDef, InvalidReturnAnnotationIsAnError) {
  expectFileError("def f() -> x: pass\n");
  expectFileError("def f() -> 1: pass\n");
  expectFileError("def f() -> int float: pass\n");
}

TEST(ParserFunctionDef, MissingNameIsAnError) {
  expectFileError("def () -> None: pass\n");
  expectFileError("def 1() -> None: pass\n");
  expectFileError("def int() -> None: pass\n");
}

TEST(ParserFunctionDef, MissingParenthesesAreAnError) {
  expectFileError("def f -> None: pass\n");
  expectFileError("def f( -> None: pass\n");
  expectFileError("def f) -> None: pass\n");
}

TEST(ParserFunctionDef, MissingColonIsAnError) { expectFileError("def f() -> None pass\n"); }

TEST(ParserFunctionDef, MissingBodyIsAnError) {
  expectFileError("def f() -> None:\n");
  expectFileError("def f() -> None:\npass\n");
}

TEST(ParserFunctionDef, ParameterAnnotationIsMandatory) {
  expectFileError("def f(a) -> None: pass\n");
  expectFileError("def f(a = 1) -> None: pass\n");
  expectFileError("def f(a: int, b) -> None: pass\n");
}

TEST(ParserFunctionDef, NoneIsNotAParameterType) {
  expectFileError("def f(a: None) -> None: pass\n");
}

TEST(ParserFunctionDef, MalformedParameterListsAreAnError) {
  expectFileError("def f(,) -> None: pass\n");
  expectFileError("def f(a: int,,) -> None: pass\n");
  expectFileError("def f(a: int b: int) -> None: pass\n");
  expectFileError("def f(a: int =) -> None: pass\n");
  expectFileError("def f(1: int) -> None: pass\n");
}

TEST(ParserFunctionDef, Rule) {
  expectParse(NodeRule{parseFunctionDef}, "def f() -> None: pass\nx\n",
              "(def f [] None (block pass))", Id("x"));
  expectParseError(NodeRule{parseFunctionDef}, "f() -> None: pass\n");
}

TEST(ParserParameter, Rule) {
  expectParse(NodeRule{parseParameter}, "a: int", "(param a int _)", Eof());
  expectParse(NodeRule{parseParameter}, "a: float = 1.5", "(param a float 1.5)", Eof());
  expectParse(NodeRule{parseParameter}, "a: bool = x or y", "(param a bool (or x y))", Eof());
  expectParse(NodeRule{parseParameter}, "a: int, b: int", "(param a int _)",
              Delim(DelimiterType::Comma));
  expectParse(NodeRule{parseParameter}, "a: int)", "(param a int _)",
              Delim(DelimiterType::RightParenthesis));
  expectParseError(NodeRule{parseParameter}, "a");
  expectParseError(NodeRule{parseParameter}, "a = 1");
  expectParseError(NodeRule{parseParameter}, "a: None");
  expectParseError(NodeRule{parseParameter}, "a: int =");
  expectParseError(NodeRule{parseParameter}, "1: int");
}

TEST(ParserDefault, Rule) {
  expectParse(NodeRule{parseDefault}, "= 1", "1", Eof());
  expectParse(NodeRule{parseDefault}, "= a + b", "(+ a b)", Eof());
  expectParse(NodeRule{parseDefault}, "= 1, b: int", "1", Delim(DelimiterType::Comma));
  expectParseError(NodeRule{parseDefault}, "1");
  expectParseError(NodeRule{parseDefault}, "=");
  expectParseError(NodeRule{parseDefault}, "== 1");
}

// Compound statement rule
// -----------------------

TEST(ParserCompoundStmtRule, ParsesEveryCompoundStatement) {
  expectParse(NodeRule{parseCompoundStmt}, "if x: pass\n", "(if x (block pass) _)", Eof());
  expectParse(NodeRule{parseCompoundStmt}, "while x: pass\n", "(while x (block pass) _)", Eof());
  expectParse(NodeRule{parseCompoundStmt}, "for i in range(3): pass\n",
              "(for _ 3 _ (block pass) _)", Eof());
  expectParse(NodeRule{parseCompoundStmt}, "def f() -> None: pass\n",
              "(def f [] None (block pass))", Eof());
}

TEST(ParserCompoundStmtRule, SimpleStatementsAreNotCompound) {
  expectParseError(NodeRule{parseCompoundStmt}, "pass\n");
  expectParseError(NodeRule{parseCompoundStmt}, "x = 1\n");
  expectParseError(NodeRule{parseCompoundStmt}, "else: pass\n");
}

// Block rule
// ----------

TEST(ParserBlockRule, SimpleStatementsOnTheSameLine) {
  expectParse(NodeRule{parseBlock}, "pass\n", "(block pass)", Eof());
  expectParse(NodeRule{parseBlock}, "pass; break\n", "(block pass break)", Eof());
  expectParse(NodeRule{parseBlock}, "pass\nx\n", "(block pass)", Id("x"));
}

TEST(ParserBlockRule, CompoundStatementIsNotASameLineBlock) {
  expectParseError(NodeRule{parseBlock}, "if x: pass\n");
}

// Statement rule
// --------------

TEST(ParserStatementRule, SimpleStatements) {
  expectParse(ListRule{parseStatement}, "pass\n", "[pass]", Eof());
  expectParse(ListRule{parseStatement}, "pass; x = 1\n", "[pass (assign [x] 1)]", Eof());
  expectParse(ListRule{parseStatement}, "pass\nbreak\n", "[pass]", Kw(KeywordType::Break));
}

TEST(ParserStatementRule, CompoundStatement) {
  expectParse(ListRule{parseStatement}, "if x: pass\n", "[(if x (block pass) _)]", Eof());
  expectParse(ListRule{parseStatement}, "if x:\n    pass\nbreak\n", "[(if x (block pass) _)]",
              Kw(KeywordType::Break));
}

TEST(ParserStatementRule, InvalidStatementsAreAnError) {
  expectParseError(ListRule{parseStatement}, "else: pass\n");
  expectParseError(ListRule{parseStatement}, ")\n");
}

// =================================================================================================
// Files
// =================================================================================================

TEST(ParserFile, EmptyFile) {
  expectFile("\n", {});
  expectFile("\n\n", {});
  expectFile("# just a comment\n", {});
  expectFile("   \n  # comment\n\n", {});
}

TEST(ParserFile, ReturnsAProgram) {
  Tokenizer T{"x = 1\n"};
  const auto Result = parseFile(T);
  EXPECT_NE(dynamic_cast<const ast::Program *>(Result.get()), nullptr) << dump(Result);
}

TEST(ParserFile, SeveralStatements) {
  expectFile("x: int = 1\ny: float = 2.0\nx += 1\n",
             {"(decl x int 1)", "(decl y float 2.0)", "(augassign += x 1)"});
}

TEST(ParserFile, DISABLED_BlankLinesAndCommentsBetweenStatements) {
  expectFile("x = 1\n\n# comment\n\ny = 2\n", {"(assign [x] 1)", "(assign [y] 2)"});
}

TEST(ParserFile, DISABLED_BlankLinesAndCommentsInsideBlocks) {
  expectStmt("if x:\n    a = 1\n\n    # comment\n    b = 2\n",
             "(if x (block (assign [a] 1) (assign [b] 2)) _)");
}

// simple_stmts must end with NEWLINE, and the tokenizer only emits one for an actual '\n'.
TEST(ParserFile, MissingFinalNewLineIsAnError) {
  expectFileError("pass");
  expectFileError("x = 1\ny = 2");
  expectFileError("pass;");
  expectFileError("if x: pass");
  expectFileError("if x:\n    pass");
}

TEST(ParserFile, SemicolonsAndNewLinesMix) {
  expectFile("a = 1; b = 2\nc = 3\n", {"(assign [a] 1)", "(assign [b] 2)", "(assign [c] 3)"});
}

TEST(ParserFile, DeeplyNestedBlocksCloseTogether) {
  expectFile("if a:\n    while b:\n        for i in range(c):\n            pass\nx = 1\n",
             {"(if a (block (while b (block (for _ c _ (block pass) _)) _)) _)", "(assign [x] 1)"});
}

TEST(ParserFile, SiblingCompoundStatements) {
  expectFile("if a:\n    pass\nif b:\n    pass\n",
             {"(if a (block pass) _)", "(if b (block pass) _)"});
  expectFile("while a: pass\nwhile b: pass\n",
             {"(while a (block pass) _)", "(while b (block pass) _)"});
}

TEST(ParserFile, CompleteProgram) {
  const std::string Source = "counter: int = 0\n"
                             "\n"
                             "def increment(by: int = 1) -> None:\n"
                             "    global counter\n"
                             "    counter += by\n"
                             "\n"
                             "def is_even(n: int) -> bool:\n"
                             "    return n % 2 == 0\n"
                             "\n"
                             "for i in range(10):\n"
                             "    if is_even(i):\n"
                             "        increment(2)\n"
                             "    elif i > 5:\n"
                             "        break\n"
                             "    else:\n"
                             "        continue\n"
                             "else:\n"
                             "    pass\n"
                             "\n"
                             "while counter > 0: counter -= 1\n";
  expectFile(Source, {
                         "(decl counter int 0)",
                         "(def increment [(param by int 1)] None (block (global counter) "
                         "(augassign += counter by)))",
                         "(def is_even [(param n int _)] bool (block (return (== (% n 2) 0))))",
                         "(for _ 10 _ (block (if (call i) (block (call 2)) (if (> i 5) (block "
                         "break) (block continue)))) (block pass))",
                         "(while (> counter 0) (block (augassign -= counter 1)) _)",
                     });
}

TEST(ParserFile, UnexpectedIndentIsAnError) {
  expectFileError("x = 1\n    y = 2\n");
  expectFileError("    x = 1\n");
}

TEST(ParserFile, ErrorsOnAnyLineAreReported) {
  expectFileError("x = 1\ny = \nz = 3\n");
  expectFileError("if x:\n    pass\n    y =\n");
}

TEST(ParserFile, UnsupportedStatementsAreAnError) {
  expectFileError("import x\n");
  expectFileError("class A: pass\n");
  expectFileError("del x\n");
  expectFileError("assert x\n");
  expectFileError("raise x\n");
  expectFileError("nonlocal x\n");
  expectFileError("lambda: 1\n");
  expectFileError("with x: pass\n");
  expectFileError("try: pass\n");
}

TEST(ParserFile, UnsupportedExpressionsAreAnError) {
  expectFileError("x = [1]\n");
  expectFileError("x = {1}\n");
  expectFileError("x = a[0]\n");
  expectFileError("x = a is b\n");
  expectFileError("x = a in b\n");
  expectFileError("x = f(a=1)\n");
}
