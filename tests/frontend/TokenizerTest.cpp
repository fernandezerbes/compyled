#include "frontend/Tokenizer.h"
#include <gtest/gtest.h>

#include <cctype>
#include <cstddef>
#include <expected>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace compyled::lexer;

namespace {

// Maximum number of tokens pulled from a tokenizer; protects the tests against a tokenizer that
// never makes progress.
constexpr std::size_t MAX_TOKENS = 10'000;

// What we expect from a token: its type and (optionally) value. Locations are checked separately.
struct Expected {
  TokenType Type;
  Token::ValueType Value{};
};

bool operator==(const Token &Actual, const Expected &Exp) {
  return Actual.Type == Exp.Type && Actual.Value == Exp.Value;
}

std::string describe(const Token &Tok) {
  return "Token{type=" + std::to_string(static_cast<int>(Tok.Type)) +
         ", value_index=" + std::to_string(Tok.Value.index()) +
         ", line=" + std::to_string(Tok.Location.Line) +
         ", col=" + std::to_string(Tok.Location.Column) + "}";
}

Expected Kw(KeywordType K) { return {TokenType::Keyword, K}; }
Expected Op(OperatorType O) { return {TokenType::Operator, O}; }
Expected Delim(DelimiterType D) { return {TokenType::Delimiter, D}; }
Expected Id(std::string Name) { return {TokenType::Identifier, std::move(Name)}; }
Expected Int(IntegerType V) { return {TokenType::Value, V}; }
Expected Flt(FloatType V) { return {TokenType::Value, V}; }
Expected Bool(BooleanType V) { return {TokenType::Value, V}; }
Expected NL() { return {TokenType::NewLine}; }
Expected Indent() { return {TokenType::Indent}; }
Expected WS() { return {TokenType::Whitespace}; }

// Tokenizes the whole source, stopping at (and excluding) the Eof token.
// Returns the error message if any token failed to be produced.
std::expected<std::vector<Token>, std::string> lexAll(std::string Source, bool KeepWhitespace,
                                                      std::size_t IndentSize = 4) {
  Tokenizer T(std::move(Source), IndentSize);
  std::vector<Token> Tokens;
  while (Tokens.size() < MAX_TOKENS) {
    auto Next = T.getNextToken();
    if (!Next) {
      return std::unexpected(Next.error());
    }
    if (Next->Type == TokenType::Eof) {
      return Tokens;
    }
    if (!KeepWhitespace && Next->Type == TokenType::Whitespace) {
      continue;
    }
    Tokens.push_back(std::move(*Next));
  }
  return std::unexpected("Tokenizer did not reach Eof (no progress?)");
}

// Asserts that `Source` tokenizes into exactly `Want` (inter-token whitespace is dropped).
void expectTokens(const std::string &Source, const std::vector<Expected> &Want) {
  SCOPED_TRACE("source: " + Source);
  auto Got = lexAll(Source, /*KeepWhitespace=*/false);
  ASSERT_TRUE(Got.has_value()) << "tokenizer error: " << Got.error();
  ASSERT_EQ(Got->size(), Want.size());
  for (std::size_t I = 0; I < Want.size(); ++I) {
    EXPECT_TRUE((*Got)[I] == Want[I]) << "mismatch at token " << I << ": " << describe((*Got)[I]);
  }
}

// Same as expectTokens but whitespace tokens are preserved.
void expectTokensWithWhitespace(const std::string &Source, const std::vector<Expected> &Want,
                                std::size_t IndentSize = 4) {
  SCOPED_TRACE("source: " + Source);
  auto Got = lexAll(Source, /*KeepWhitespace=*/true, IndentSize);
  ASSERT_TRUE(Got.has_value()) << "tokenizer error: " << Got.error();
  ASSERT_EQ(Got->size(), Want.size());
  for (std::size_t I = 0; I < Want.size(); ++I) {
    EXPECT_TRUE((*Got)[I] == Want[I]) << "mismatch at token " << I << ": " << describe((*Got)[I]);
  }
}

void expectError(const std::string &Source) {
  SCOPED_TRACE("source: " + Source);
  auto Got = lexAll(Source, /*KeepWhitespace=*/false);
  EXPECT_FALSE(Got.has_value()) << "expected a tokenizer error";
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Construction and end of input
// ---------------------------------------------------------------------------------------------

TEST(TokenizerConstruction, DefaultIndentSizeIsAccepted) { EXPECT_NO_THROW(Tokenizer{"x"}); }

TEST(TokenizerConstruction, EvenIndentSizesAreAccepted) {
  EXPECT_NO_THROW((Tokenizer{"x", 2}));
  EXPECT_NO_THROW((Tokenizer{"x", 4}));
  EXPECT_NO_THROW((Tokenizer{"x", 8}));
}

TEST(TokenizerConstruction, OddIndentSizesAreRejected) {
  EXPECT_THROW((Tokenizer{"x", 3}), std::invalid_argument);
  EXPECT_THROW((Tokenizer{"x", 5}), std::invalid_argument);
  EXPECT_THROW((Tokenizer{"x", 7}), std::invalid_argument);
}

TEST(TokenizerEof, EmptySourceYieldsEof) {
  Tokenizer T("");
  auto Tok = T.getNextToken();
  ASSERT_TRUE(Tok.has_value());
  EXPECT_EQ(Tok->Type, TokenType::Eof);
}

TEST(TokenizerEof, EofIsStableWhenCalledRepeatedly) {
  Tokenizer T("");
  for (int I = 0; I < 3; ++I) {
    auto Tok = T.getNextToken();
    ASSERT_TRUE(Tok.has_value());
    EXPECT_EQ(Tok->Type, TokenType::Eof);
  }
}

TEST(TokenizerEof, EofFollowsLastToken) {
  Tokenizer T("x");
  auto First = T.getNextToken();
  ASSERT_TRUE(First.has_value());
  EXPECT_EQ(First->Type, TokenType::Identifier);
  auto Second = T.getNextToken();
  ASSERT_TRUE(Second.has_value());
  EXPECT_EQ(Second->Type, TokenType::Eof);
}

// ---------------------------------------------------------------------------------------------
// Integer literals
// ---------------------------------------------------------------------------------------------

TEST(TokenizerIntegers, SingleDigit) { expectTokens("7", {Int(7)}); }
TEST(TokenizerIntegers, Zero) { expectTokens("0", {Int(0)}); }
TEST(TokenizerIntegers, MultiDigit) { expectTokens("1234567890", {Int(1234567890)}); }
TEST(TokenizerIntegers, LargerThan32Bits) { expectTokens("4294967296", {Int(4294967296)}); }
TEST(TokenizerIntegers, MaxInt64) {
  expectTokens("9223372036854775807", {Int(9223372036854775807)});
}

TEST(TokenizerIntegers, SeveralSeparatedBySpaces) {
  expectTokens("1 22 333", {Int(1), Int(22), Int(333)});
}

TEST(TokenizerIntegers, SeveralSeparatedByCommas) {
  expectTokens("1,2,3",
               {Int(1), Delim(DelimiterType::Comma), Int(2), Delim(DelimiterType::Comma), Int(3)});
}

TEST(TokenizerIntegers, NegativeNumberIsUnaryMinusFollowedByInteger) {
  expectTokens("-5", {Op(OperatorType::Subtract), Int(5)});
}

TEST(TokenizerIntegers, PositiveSignIsOperator) {
  expectTokens("+5", {Op(OperatorType::Add), Int(5)});
}

TEST(TokenizerIntegers, IntegerIsNotMistakenForFloat) {
  auto Got = lexAll("42", false);
  ASSERT_TRUE(Got.has_value()) << Got.error();
  ASSERT_EQ(Got->size(), 1u);
  ASSERT_EQ((*Got)[0].Type, TokenType::Value);
  EXPECT_TRUE(std::holds_alternative<IntegerType>((*Got)[0].Value));
}

// ---------------------------------------------------------------------------------------------
// Float literals
// ---------------------------------------------------------------------------------------------

TEST(TokenizerFloats, Simple) { expectTokens("3.14", {Flt(3.14)}); }
TEST(TokenizerFloats, ZeroPointZero) { expectTokens("0.0", {Flt(0.0)}); }
TEST(TokenizerFloats, LeadingZeroFraction) { expectTokens("0.5", {Flt(0.5)}); }
TEST(TokenizerFloats, NoLeadingDigit) { expectTokens(".5", {Flt(0.5)}); }
TEST(TokenizerFloats, TrailingDot) { expectTokens("5.", {Flt(5.0)}); }
TEST(TokenizerFloats, Exponent) { expectTokens("1e3", {Flt(1000.0)}); }
TEST(TokenizerFloats, UpperCaseExponent) { expectTokens("1E3", {Flt(1000.0)}); }
TEST(TokenizerFloats, NegativeExponent) { expectTokens("1e-3", {Flt(0.001)}); }
TEST(TokenizerFloats, ExplicitPositiveExponent) { expectTokens("2.5e+2", {Flt(250.0)}); }
TEST(TokenizerFloats, FractionAndExponent) { expectTokens("1.5e3", {Flt(1500.0)}); }

TEST(TokenizerFloats, NegativeFloatIsUnaryMinusFollowedByFloat) {
  expectTokens("-2.5", {Op(OperatorType::Subtract), Flt(2.5)});
}

TEST(TokenizerFloats, FloatInExpression) {
  expectTokens("1.5 + 2.25", {Flt(1.5), Op(OperatorType::Add), Flt(2.25)});
}

TEST(TokenizerFloats, FloatTypeIsFloat) {
  auto Got = lexAll("3.14", false);
  ASSERT_TRUE(Got.has_value()) << Got.error();
  ASSERT_EQ(Got->size(), 1u);
  ASSERT_EQ((*Got)[0].Type, TokenType::Value);
  EXPECT_TRUE(std::holds_alternative<FloatType>((*Got)[0].Value));
}

// ---------------------------------------------------------------------------------------------
// Identifiers
// ---------------------------------------------------------------------------------------------

TEST(TokenizerIdentifiers, SingleLetter) { expectTokens("x", {Id("x")}); }
TEST(TokenizerIdentifiers, LongName) { expectTokens("some_long_name", {Id("some_long_name")}); }
TEST(TokenizerIdentifiers, WithDigits) { expectTokens("var123", {Id("var123")}); }
TEST(TokenizerIdentifiers, LeadingUnderscore) { expectTokens("_private", {Id("_private")}); }
TEST(TokenizerIdentifiers, DunderName) { expectTokens("__init__", {Id("__init__")}); }
TEST(TokenizerIdentifiers, OnlyUnderscore) { expectTokens("_", {Id("_")}); }
TEST(TokenizerIdentifiers, UpperCase) { expectTokens("CONSTANT", {Id("CONSTANT")}); }
TEST(TokenizerIdentifiers, MixedCase) { expectTokens("camelCase", {Id("camelCase")}); }
TEST(TokenizerIdentifiers, ClassStyleName) { expectTokens("MyClass", {Id("MyClass")}); }

TEST(TokenizerIdentifiers, ContainingKeywordAsPrefix) {
  expectTokens("iffy", {Id("iffy")});
  expectTokens("format", {Id("format")});
  expectTokens("important", {Id("important")});
  expectTokens("passing", {Id("passing")});
}

TEST(TokenizerIdentifiers, ContainingKeywordAsSuffix) {
  expectTokens("elif_", {Id("elif_")});
  expectTokens("my_if", {Id("my_if")});
  expectTokens("notin", {Id("notin")});
}

TEST(TokenizerIdentifiers, KeywordsAreCaseSensitive) {
  expectTokens("If", {Id("If")});
  expectTokens("WHILE", {Id("WHILE")});
  expectTokens("Def", {Id("Def")});
  expectTokens("true", {Id("true")});
  expectTokens("none", {Id("none")});
  expectTokens("false", {Id("false")});
}

TEST(TokenizerIdentifiers, DigitsCannotStartAnIdentifier) {
  expectTokens("1abc", {Int(1), Id("abc")});
}

TEST(TokenizerIdentifiers, MultipleSeparatedBySpaces) {
  expectTokens("a b c", {Id("a"), Id("b"), Id("c")});
}

// ---------------------------------------------------------------------------------------------
// Keywords
// ---------------------------------------------------------------------------------------------

struct KeywordCase {
  const char *Text;
  KeywordType Type;
};

class KeywordTest : public ::testing::TestWithParam<KeywordCase> {};

TEST_P(KeywordTest, IsRecognisedAlone) {
  const auto &[Text, Type] = GetParam();
  expectTokens(Text, {Kw(Type)});
}

TEST_P(KeywordTest, IsRecognisedBeforeIdentifier) {
  const auto &[Text, Type] = GetParam();
  expectTokens(std::string(Text) + " x", {Kw(Type), Id("x")});
}

TEST_P(KeywordTest, IsRecognisedAfterIdentifier) {
  const auto &[Text, Type] = GetParam();
  expectTokens("x " + std::string(Text), {Id("x"), Kw(Type)});
}

TEST_P(KeywordTest, IsNotRecognisedWhenSuffixed) {
  const auto &[Text, Type] = GetParam();
  expectTokens(std::string(Text) + "_x", {Id(std::string(Text) + "_x")});
}

TEST_P(KeywordTest, IsNotRecognisedWhenPrefixed) {
  const auto &[Text, Type] = GetParam();
  expectTokens("x_" + std::string(Text), {Id("x_" + std::string(Text))});
}

INSTANTIATE_TEST_SUITE_P(
    AllKeywords, KeywordTest,
    ::testing::Values(
        KeywordCase{"await", KeywordType::Await}, KeywordCase{"else", KeywordType::Else},
        KeywordCase{"import", KeywordType::Import}, KeywordCase{"pass", KeywordType::Pass},
        KeywordCase{"None", KeywordType::None}, KeywordCase{"break", KeywordType::Break},
        KeywordCase{"except", KeywordType::Except}, KeywordCase{"in", KeywordType::In},
        KeywordCase{"raise", KeywordType::Raise}, KeywordCase{"class", KeywordType::Class},
        KeywordCase{"finally", KeywordType::Finally}, KeywordCase{"is", KeywordType::Is},
        KeywordCase{"return", KeywordType::Return}, KeywordCase{"continue", KeywordType::Continue},
        KeywordCase{"for", KeywordType::For}, KeywordCase{"lambda", KeywordType::Lambda},
        KeywordCase{"try", KeywordType::Try}, KeywordCase{"as", KeywordType::As},
        KeywordCase{"def", KeywordType::Def}, KeywordCase{"from", KeywordType::From},
        KeywordCase{"nonlocal", KeywordType::Nonlocal}, KeywordCase{"while", KeywordType::While},
        KeywordCase{"assert", KeywordType::Assert}, KeywordCase{"del", KeywordType::Del},
        KeywordCase{"global", KeywordType::Global}, KeywordCase{"with", KeywordType::With},
        KeywordCase{"async", KeywordType::Async}, KeywordCase{"elif", KeywordType::Elif},
        KeywordCase{"if", KeywordType::If}, KeywordCase{"yield", KeywordType::Yield}),
    [](const ::testing::TestParamInfo<KeywordCase> &Info) { return std::string(Info.param.Text); });

// ---------------------------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------------------------

struct OperatorCase {
  const char *Text;
  OperatorType Type;
  const char *Name;
};

class OperatorTest : public ::testing::TestWithParam<OperatorCase> {};

TEST_P(OperatorTest, IsRecognisedAlone) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(Text, {Op(Type)});
}

TEST_P(OperatorTest, IsRecognisedBetweenOperands) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(std::string("a ") + Text + " b", {Id("a"), Op(Type), Id("b")});
}

TEST_P(OperatorTest, IsRecognisedWithoutSurroundingSpaces) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(std::string("a") + Text + "b", {Id("a"), Op(Type), Id("b")});
}

TEST_P(OperatorTest, IsRecognisedBeforeNumber) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(std::string(Text) + "1", {Op(Type), Int(1)});
}

TEST_P(OperatorTest, IsRecognisedAfterNumber) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(std::string("1") + Text, {Int(1), Op(Type)});
}

INSTANTIATE_TEST_SUITE_P(
    AllOperators, OperatorTest,
    ::testing::Values(
        // Arithmetic
        OperatorCase{"+", OperatorType::Add, "Add"},
        OperatorCase{"-", OperatorType::Subtract, "Subtract"},
        OperatorCase{"*", OperatorType::Multiply, "Multiply"},
        OperatorCase{"**", OperatorType::Exponentiate, "Exponentiate"},
        OperatorCase{"/", OperatorType::FloatDivide, "FloatDivide"},
        OperatorCase{"//", OperatorType::IntegerDivide, "IntegerDivide"},
        OperatorCase{"%", OperatorType::Modulo, "Modulo"},
        // Bitwise
        OperatorCase{"&", OperatorType::BitwiseAnd, "BitwiseAnd"},
        OperatorCase{"|", OperatorType::BitwiseOr, "BitwiseOr"},
        OperatorCase{"^", OperatorType::BitwiseXor, "BitwiseXor"},
        OperatorCase{"~", OperatorType::BitwiseInvert, "BitwiseInvert"},
        OperatorCase{"<<", OperatorType::LeftShift, "LeftShift"},
        OperatorCase{">>", OperatorType::RightShift, "RightShift"},
        // Comparison
        OperatorCase{"<", OperatorType::Less, "Less"},
        OperatorCase{">", OperatorType::Greater, "Greater"},
        OperatorCase{"<=", OperatorType::LessOrEqual, "LessOrEqual"},
        OperatorCase{">=", OperatorType::GreaterOrEqual, "GreaterOrEqual"},
        OperatorCase{"==", OperatorType::Equal, "Equal"},
        OperatorCase{"!=", OperatorType::NotEqual, "NotEqual"},
        // Assignment
        OperatorCase{"=", OperatorType::Assign, "Assign"},
        OperatorCase{"+=", OperatorType::UnaryAdd, "AddAssign"},
        OperatorCase{"-=", OperatorType::UnarySubtract, "SubtractAssign"},
        OperatorCase{"*=", OperatorType::UnaryMultiply, "MultiplyAssign"},
        OperatorCase{"**=", OperatorType::UnaryExponentiate, "ExponentiateAssign"},
        OperatorCase{"/=", OperatorType::UnaryFloatDivide, "FloatDivideAssign"},
        OperatorCase{"//=", OperatorType::UnaryIntegerDivide, "IntegerDivideAssign"},
        OperatorCase{"%=", OperatorType::UnaryModulo, "ModuloAssign"},
        OperatorCase{"&=", OperatorType::UnaryBitwiseAnd, "BitwiseAndAssign"},
        OperatorCase{"|=", OperatorType::UnaryBitwiseOr, "BitwiseOrAssign"},
        OperatorCase{"^=", OperatorType::UnaryBitwiseXor, "BitwiseXorAssign"},
        OperatorCase{"<<=", OperatorType::UnaryLeftShift, "LeftShiftAssign"},
        OperatorCase{">>=", OperatorType::UnaryRightShift, "RightShiftAssign"}),
    [](const ::testing::TestParamInfo<OperatorCase> &Info) {
      return std::string(Info.param.Name);
    });

TEST(TokenizerOperators, MaximalMunchPrefersLongestOperator) {
  expectTokens("a**b", {Id("a"), Op(OperatorType::Exponentiate), Id("b")});
  expectTokens("a***b",
               {Id("a"), Op(OperatorType::Exponentiate), Op(OperatorType::Multiply), Id("b")});
  expectTokens("a//b", {Id("a"), Op(OperatorType::IntegerDivide), Id("b")});
  expectTokens("a<<=b", {Id("a"), Op(OperatorType::UnaryLeftShift), Id("b")});
  expectTokens("a>>=b", {Id("a"), Op(OperatorType::UnaryRightShift), Id("b")});
}

TEST(TokenizerOperators, AdjacentOperatorsAreSplitCorrectly) {
  expectTokens("a+-b", {Id("a"), Op(OperatorType::Add), Op(OperatorType::Subtract), Id("b")});
  expectTokens("a*-b", {Id("a"), Op(OperatorType::Multiply), Op(OperatorType::Subtract), Id("b")});
  expectTokens("a==-b", {Id("a"), Op(OperatorType::Equal), Op(OperatorType::Subtract), Id("b")});
  expectTokens("a=~b",
               {Id("a"), Op(OperatorType::Assign), Op(OperatorType::BitwiseInvert), Id("b")});
  expectTokens("a<-b", {Id("a"), Op(OperatorType::Less), Op(OperatorType::Subtract), Id("b")});
}

TEST(TokenizerOperators, OperatorAtVeryEndOfInput) {
  expectTokens("<", {Op(OperatorType::Less)});
  expectTokens(">", {Op(OperatorType::Greater)});
  expectTokens("*", {Op(OperatorType::Multiply)});
  expectTokens("/", {Op(OperatorType::FloatDivide)});
  expectTokens("**", {Op(OperatorType::Exponentiate)});
  expectTokens("//", {Op(OperatorType::IntegerDivide)});
  expectTokens("<<", {Op(OperatorType::LeftShift)});
  expectTokens(">>", {Op(OperatorType::RightShift)});
  expectTokens("-", {Op(OperatorType::Subtract)});
  expectTokens("=", {Op(OperatorType::Assign)});
}

TEST(TokenizerOperators, LessFollowedByNonOperatorCharacter) {
  expectTokens("a<b", {Id("a"), Op(OperatorType::Less), Id("b")});
  expectTokens("a<1", {Id("a"), Op(OperatorType::Less), Int(1)});
  expectTokens("a>b", {Id("a"), Op(OperatorType::Greater), Id("b")});
  expectTokens("a>1", {Id("a"), Op(OperatorType::Greater), Int(1)});
}

TEST(TokenizerOperators, StarFollowedByNonOperatorCharacter) {
  expectTokens("a*b", {Id("a"), Op(OperatorType::Multiply), Id("b")});
  expectTokens("a*1", {Id("a"), Op(OperatorType::Multiply), Int(1)});
  expectTokens("a/b", {Id("a"), Op(OperatorType::FloatDivide), Id("b")});
  expectTokens("a/1", {Id("a"), Op(OperatorType::FloatDivide), Int(1)});
}

TEST(TokenizerOperators, LogicalOperatorsKeywordsAreOperators) {
  expectTokens("a and b", {Id("a"), Op(OperatorType::LogicalAnd), Id("b")});
  expectTokens("a or b", {Id("a"), Op(OperatorType::LogicalOr), Id("b")});
  expectTokens("not a", {Op(OperatorType::LogicalNot), Id("a")});
}

TEST(TokenizerOperators, IsAndInAreKeywords) {
  expectTokens("a is b", {Id("a"), Kw(KeywordType::Is), Id("b")});
  expectTokens("a is not b", {Id("a"), Kw(KeywordType::Is), Op(OperatorType::LogicalNot), Id("b")});
  expectTokens("a in b", {Id("a"), Kw(KeywordType::In), Id("b")});
  expectTokens("a not in b", {Id("a"), Op(OperatorType::LogicalNot), Kw(KeywordType::In), Id("b")});
}

// ---------------------------------------------------------------------------------------------
// Delimiters
// ---------------------------------------------------------------------------------------------

struct DelimiterCase {
  const char *Text;
  DelimiterType Type;
  const char *Name;
};

class DelimiterTest : public ::testing::TestWithParam<DelimiterCase> {};

TEST_P(DelimiterTest, IsRecognisedAlone) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(Text, {Delim(Type)});
}

TEST_P(DelimiterTest, IsRecognisedBetweenIdentifiers) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(std::string("a") + Text + "b", {Id("a"), Delim(Type), Id("b")});
}

TEST_P(DelimiterTest, IsRecognisedBetweenIdentifiersWithSpaces) {
  const auto &[Text, Type, Name] = GetParam();
  expectTokens(std::string("a ") + Text + " b", {Id("a"), Delim(Type), Id("b")});
}

INSTANTIATE_TEST_SUITE_P(
    AllDelimiters, DelimiterTest,
    ::testing::Values(DelimiterCase{"(", DelimiterType::LeftParenthesis, "LeftParenthesis"},
                      DelimiterCase{")", DelimiterType::RightParenthesis, "RightParenthesis"},
                      DelimiterCase{"[", DelimiterType::LeftBracket, "LeftBracket"},
                      DelimiterCase{"]", DelimiterType::RightBracket, "RightBracket"},
                      DelimiterCase{"{", DelimiterType::LeftBraces, "LeftBraces"},
                      DelimiterCase{"}", DelimiterType::RightBraces, "RightBraces"},
                      DelimiterCase{",", DelimiterType::Comma, "Comma"},
                      DelimiterCase{":", DelimiterType::Colon, "Colon"},
                      DelimiterCase{";", DelimiterType::Semicolon, "Semicolon"},
                      DelimiterCase{"!", DelimiterType::Exclamation, "Exclamation"},
                      DelimiterCase{"->", DelimiterType::Arrow, "Arrow"}),
    [](const ::testing::TestParamInfo<DelimiterCase> &Info) {
      return std::string(Info.param.Name);
    });

TEST(TokenizerDelimiters, ArrowIsNotSubtractThenGreater) {
  expectTokens("->", {Delim(DelimiterType::Arrow)});
  expectTokens("a->b", {Id("a"), Delim(DelimiterType::Arrow), Id("b")});
}

TEST(TokenizerDelimiters, SubtractFollowedBySpaceThenGreater) {
  expectTokens("- >", {Op(OperatorType::Subtract), Op(OperatorType::Greater)});
}

TEST(TokenizerDelimiters, ExclamationNotFollowedByEqualsIsDelimiter) {
  expectTokens("!a", {Delim(DelimiterType::Exclamation), Id("a")});
}

TEST(TokenizerDelimiters, NestedBrackets) {
  expectTokens("([{}])",
               {Delim(DelimiterType::LeftParenthesis), Delim(DelimiterType::LeftBracket),
                Delim(DelimiterType::LeftBraces), Delim(DelimiterType::RightBraces),
                Delim(DelimiterType::RightBracket), Delim(DelimiterType::RightParenthesis)});
}

TEST(TokenizerDelimiters, EmptyPairs) {
  expectTokens("()",
               {Delim(DelimiterType::LeftParenthesis), Delim(DelimiterType::RightParenthesis)});
  expectTokens("[]", {Delim(DelimiterType::LeftBracket), Delim(DelimiterType::RightBracket)});
  expectTokens("{}", {Delim(DelimiterType::LeftBraces), Delim(DelimiterType::RightBraces)});
}

TEST(TokenizerDelimiters, ColonAtEndOfInput) {
  expectTokens("else:", {Kw(KeywordType::Else), Delim(DelimiterType::Colon)});
}

TEST(TokenizerDelimiters, SemicolonSeparatedStatements) {
  expectTokens("a = 1; b = 2",
               {Id("a"), Op(OperatorType::Assign), Int(1), Delim(DelimiterType::Semicolon), Id("b"),
                Op(OperatorType::Assign), Int(2)});
}

// ---------------------------------------------------------------------------------------------
// Newlines, whitespace and indentation
// ---------------------------------------------------------------------------------------------

TEST(TokenizerWhitespace, LoneNewline) { expectTokens("\n", {NL()}); }

TEST(TokenizerWhitespace, SeveralNewlines) { expectTokens("\n\n\n", {NL(), NL(), NL()}); }

TEST(TokenizerWhitespace, NewlineBetweenStatements) {
  expectTokens("a\nb", {Id("a"), NL(), Id("b")});
}

TEST(TokenizerWhitespace, TrailingNewline) { expectTokens("a\n", {Id("a"), NL()}); }

TEST(TokenizerWhitespace, BlankLinesBetweenStatements) {
  expectTokens("a\n\nb", {Id("a"), NL(), NL(), Id("b")});
}

TEST(TokenizerWhitespace, SingleSpaceYieldsWhitespaceToken) {
  expectTokensWithWhitespace("a b", {Id("a"), WS(), Id("b")});
}

TEST(TokenizerWhitespace, SpacesAroundOperator) {
  expectTokensWithWhitespace("a + b", {Id("a"), WS(), Op(OperatorType::Add), WS(), Id("b")});
}

TEST(TokenizerWhitespace, NoSpacesMeansNoWhitespaceTokens) {
  expectTokensWithWhitespace("a+b", {Id("a"), Op(OperatorType::Add), Id("b")});
}

TEST(TokenizerIndentation, FourSpacesIsOneIndent) {
  expectTokensWithWhitespace("    x", {Indent(), Id("x")});
}

TEST(TokenizerIndentation, EightSpacesIsTwoIndents) {
  expectTokensWithWhitespace("        x", {Indent(), Indent(), Id("x")});
}

TEST(TokenizerIndentation, TwelveSpacesIsThreeIndents) {
  expectTokensWithWhitespace("            x", {Indent(), Indent(), Indent(), Id("x")});
}

TEST(TokenizerIndentation, CustomIndentSizeTwo) {
  expectTokensWithWhitespace("  x", {Indent(), Id("x")}, 2);
  expectTokensWithWhitespace("    x", {Indent(), Indent(), Id("x")}, 2);
}

TEST(TokenizerIndentation, CustomIndentSizeEight) {
  expectTokensWithWhitespace("        x", {Indent(), Id("x")}, 8);
}

TEST(TokenizerIndentation, IndentAfterNewline) {
  expectTokensWithWhitespace(
      "if x:\n    y",
      {Kw(KeywordType::If), WS(), Id("x"), Delim(DelimiterType::Colon), NL(), Indent(), Id("y")});
}

TEST(TokenizerIndentation, DedentBackToZero) {
  expectTokensWithWhitespace("if x:\n    y\nz",
                             {Kw(KeywordType::If), WS(), Id("x"), Delim(DelimiterType::Colon), NL(),
                              Indent(), Id("y"), NL(), Id("z")});
}

TEST(TokenizerIndentation, NestedBlocks) {
  expectTokensWithWhitespace("if a:\n    if b:\n        c",
                             {Kw(KeywordType::If), WS(), Id("a"), Delim(DelimiterType::Colon), NL(),
                              Indent(), Kw(KeywordType::If), WS(), Id("b"),
                              Delim(DelimiterType::Colon), NL(), Indent(), Indent(), Id("c")});
}

TEST(TokenizerIndentation, IndentedBlankLineIsJustIndentThenNewline) {
  expectTokensWithWhitespace("    \n", {Indent(), NL()});
}

TEST(TokenizerIndentation, PartialIndentationIsAnError) {
  expectError("  x");
  expectError("   x");
}

TEST(TokenizerIndentation, PartialIndentationAfterFullIndentIsAnError) { expectError("      x"); }

TEST(TokenizerIndentation, PartialIndentationWithCustomSizeIsAnError) {
  auto Got = lexAll("    x", /*KeepWhitespace=*/true, 8);
  EXPECT_FALSE(Got.has_value());
}

// ---------------------------------------------------------------------------------------------
// Source locations
// ---------------------------------------------------------------------------------------------

namespace {
std::vector<Token> lexOrFail(const std::string &Source) {
  auto Got = lexAll(Source, /*KeepWhitespace=*/false);
  EXPECT_TRUE(Got.has_value()) << (Got ? "" : Got.error());
  return Got ? *Got : std::vector<Token>{};
}
} // namespace

TEST(TokenizerLocations, FirstTokenIsAtOrigin) {
  auto Toks = lexOrFail("x");
  ASSERT_EQ(Toks.size(), 1u);
  EXPECT_EQ(Toks[0].Location.Line, 0u);
  EXPECT_EQ(Toks[0].Location.Column, 0u);
}

TEST(TokenizerLocations, ColumnAdvancesWithTokenLength) {
  auto Toks = lexOrFail("abc+de");
  ASSERT_EQ(Toks.size(), 3u);
  EXPECT_EQ(Toks[0].Location.Column, 0u);
  EXPECT_EQ(Toks[1].Location.Column, 3u);
  EXPECT_EQ(Toks[2].Location.Column, 4u);
}

TEST(TokenizerLocations, ColumnAdvancesWithMultiCharacterOperators) {
  auto Toks = lexOrFail("a**=b");
  ASSERT_EQ(Toks.size(), 3u);
  EXPECT_EQ(Toks[0].Location.Column, 0u);
  EXPECT_EQ(Toks[1].Location.Column, 1u);
  EXPECT_EQ(Toks[2].Location.Column, 4u);
}

TEST(TokenizerLocations, ColumnAdvancesWithNumbers) {
  auto Toks = lexOrFail("123,4.5,x");
  ASSERT_EQ(Toks.size(), 5u);
  EXPECT_EQ(Toks[0].Location.Column, 0u);
  EXPECT_EQ(Toks[1].Location.Column, 3u);
  EXPECT_EQ(Toks[2].Location.Column, 4u);
  EXPECT_EQ(Toks[3].Location.Column, 7u);
  EXPECT_EQ(Toks[4].Location.Column, 8u);
}

TEST(TokenizerLocations, SpacesAdvanceColumn) {
  auto Toks = lexOrFail("a b");
  ASSERT_EQ(Toks.size(), 2u);
  EXPECT_EQ(Toks[0].Location.Column, 0u);
  EXPECT_EQ(Toks[1].Location.Column, 2u);
}

TEST(TokenizerLocations, NewlineIncrementsLineAndResetsColumn) {
  auto Toks = lexOrFail("ab\ncd");
  ASSERT_EQ(Toks.size(), 3u);
  EXPECT_EQ(Toks[0].Location.Line, 0u);
  EXPECT_EQ(Toks[1].Type, TokenType::NewLine);
  EXPECT_EQ(Toks[1].Location.Line, 0u);
  EXPECT_EQ(Toks[2].Location.Line, 1u);
  EXPECT_EQ(Toks[2].Location.Column, 0u);
}

TEST(TokenizerLocations, LineCountAcrossManyLines) {
  auto Toks = lexOrFail("a\nb\nc\nd");
  ASSERT_EQ(Toks.size(), 7u);
  EXPECT_EQ(Toks[0].Location.Line, 0u);
  EXPECT_EQ(Toks[2].Location.Line, 1u);
  EXPECT_EQ(Toks[4].Location.Line, 2u);
  EXPECT_EQ(Toks[6].Location.Line, 3u);
}

TEST(TokenizerLocations, EofLocationIsAfterLastLine) {
  Tokenizer T("a\nb");
  Token Last{};
  for (std::size_t I = 0; I < 16; ++I) {
    auto Tok = T.getNextToken();
    ASSERT_TRUE(Tok.has_value()) << Tok.error();
    Last = *Tok;
    if (Tok->Type == TokenType::Eof) {
      break;
    }
  }
  EXPECT_EQ(Last.Type, TokenType::Eof);
  EXPECT_EQ(Last.Location.Line, 1u);
  EXPECT_EQ(Last.Location.Column, 1u);
}

// ---------------------------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------------------------

TEST(TokenizerErrors, UnsupportedCharacterIsAnError) {
  expectError("$");
  expectError("?");
  expectError("`");
  expectError("\\");
  expectError("#");
}

TEST(TokenizerErrors, UnsupportedCharacterInsideStatementIsAnError) {
  expectError("a = $b");
  expectError("x = 1 ? 2");
}

TEST(TokenizerErrors, ErrorMessageIsNotEmpty) {
  auto Got = lexAll("$", false);
  ASSERT_FALSE(Got.has_value());
  EXPECT_FALSE(Got.error().empty());
}

// ---------------------------------------------------------------------------------------------
// Boolean literals
// ---------------------------------------------------------------------------------------------

struct BooleanCase {
  const char *Text;
  bool Value;
};

class BooleanTest : public ::testing::TestWithParam<BooleanCase> {};

TEST_P(BooleanTest, IsRecognisedAlone) {
  const auto &[Text, Value] = GetParam();
  expectTokens(Text, {Bool(Value)});
}

TEST_P(BooleanTest, IsRecognisedBeforeIdentifier) {
  const auto &[Text, Value] = GetParam();
  expectTokens(std::string(Text) + " x", {Bool(Value), Id("x")});
}

TEST_P(BooleanTest, IsRecognisedAfterIdentifier) {
  const auto &[Text, Value] = GetParam();
  expectTokens("x " + std::string(Text), {Id("x"), Bool(Value)});
}

TEST_P(BooleanTest, IsRecognisedInsideDelimiters) {
  const auto &[Text, Value] = GetParam();
  expectTokens(std::string("(") + Text + ")", {Delim(DelimiterType::LeftParenthesis), Bool(Value),
                                               Delim(DelimiterType::RightParenthesis)});
}

TEST_P(BooleanTest, IsNotRecognisedWhenSuffixed) {
  const auto &[Text, Value] = GetParam();
  expectTokens(std::string(Text) + "_x", {Id(std::string(Text) + "_x")});
  expectTokens(std::string(Text) + "1", {Id(std::string(Text) + "1")});
}

TEST_P(BooleanTest, IsNotRecognisedWhenPrefixed) {
  const auto &[Text, Value] = GetParam();
  expectTokens("x_" + std::string(Text), {Id("x_" + std::string(Text))});
}

TEST_P(BooleanTest, IsCaseSensitive) {
  const auto &[Text, Value] = GetParam();
  auto Lower = std::string(Text);
  for (auto &C : Lower) {
    C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
  }
  auto Upper = std::string(Text);
  for (auto &C : Upper) {
    C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
  }
  expectTokens(Lower, {Id(Lower)});
  expectTokens(Upper, {Id(Upper)});
}

INSTANTIATE_TEST_SUITE_P(AllBooleans, BooleanTest,
                         ::testing::Values(BooleanCase{"True", true}, BooleanCase{"False", false}),
                         [](const ::testing::TestParamInfo<BooleanCase> &Info) {
                           return std::string(Info.param.Text);
                         });

TEST(TokenizerBooleans, AreNotKeywords) {
  auto Got = lexAll("True False", false);
  ASSERT_TRUE(Got.has_value());
  for (const auto &Tok : *Got) {
    EXPECT_EQ((*Got)[0].Type, TokenType::Value);
  }
}

TEST(TokenizerBooleans, ComparedWithEquality) {
  expectTokens("a == True", {Id("a"), Op(OperatorType::Equal), Bool(true)});
}

TEST(TokenizerBooleans, NoneIsStillAKeyword) { expectTokens("None", {Kw(KeywordType::None)}); }

// ---------------------------------------------------------------------------------------------
// Logical operator words
// ---------------------------------------------------------------------------------------------

struct LogicalCase {
  const char *Text;
  OperatorType Type;
};

class LogicalOperatorTest : public ::testing::TestWithParam<LogicalCase> {};

TEST_P(LogicalOperatorTest, IsRecognisedAlone) {
  const auto &[Text, Type] = GetParam();
  expectTokens(Text, {Op(Type)});
}

TEST_P(LogicalOperatorTest, IsRecognisedBetweenIdentifiers) {
  const auto &[Text, Type] = GetParam();
  expectTokens("a " + std::string(Text) + " b", {Id("a"), Op(Type), Id("b")});
}

TEST_P(LogicalOperatorTest, IsRecognisedBetweenBooleans) {
  const auto &[Text, Type] = GetParam();
  expectTokens("True " + std::string(Text) + " False", {Bool(true), Op(Type), Bool(false)});
}

TEST_P(LogicalOperatorTest, IsRecognisedBeforeParenthesis) {
  const auto &[Text, Type] = GetParam();
  expectTokens(std::string(Text) + "(x)", {Op(Type), Delim(DelimiterType::LeftParenthesis), Id("x"),
                                           Delim(DelimiterType::RightParenthesis)});
}

TEST_P(LogicalOperatorTest, IsNotRecognisedWhenSuffixed) {
  const auto &[Text, Type] = GetParam();
  expectTokens(std::string(Text) + "_x", {Id(std::string(Text) + "_x")});
  expectTokens(std::string(Text) + "x", {Id(std::string(Text) + "x")});
}

TEST_P(LogicalOperatorTest, IsNotRecognisedWhenPrefixed) {
  const auto &[Text, Type] = GetParam();
  expectTokens("x_" + std::string(Text), {Id("x_" + std::string(Text))});
  expectTokens("x" + std::string(Text), {Id("x" + std::string(Text))});
}

TEST_P(LogicalOperatorTest, IsCaseSensitive) {
  const auto &[Text, Type] = GetParam();
  auto Upper = std::string(Text);
  for (auto &C : Upper) {
    C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
  }
  expectTokens(Upper, {Id(Upper)});
}

INSTANTIATE_TEST_SUITE_P(AllLogicalOperators, LogicalOperatorTest,
                         ::testing::Values(LogicalCase{"and", OperatorType::LogicalAnd},
                                           LogicalCase{"or", OperatorType::LogicalOr},
                                           LogicalCase{"not", OperatorType::LogicalNot}),
                         [](const ::testing::TestParamInfo<LogicalCase> &Info) {
                           return std::string(Info.param.Text);
                         });

TEST(TokenizerLogical, IdentifiersContainingLogicalWords) {
  expectTokens("android order notify", {Id("android"), Id("order"), Id("notify")});
}

TEST(TokenizerLogical, ChainedLogicalOperators) {
  expectTokens("not not a", {Op(OperatorType::LogicalNot), Op(OperatorType::LogicalNot), Id("a")});
  expectTokens("a and b and c", {Id("a"), Op(OperatorType::LogicalAnd), Id("b"),
                                 Op(OperatorType::LogicalAnd), Id("c")});
}

TEST(TokenizerLogical, AreNotKeywords) {
  auto Got = lexAll("and or not", false);
  ASSERT_TRUE(Got.has_value());
  for (const auto &Tok : *Got) {
    EXPECT_EQ(Tok.Type, TokenType::Operator);
  }
}

// ---------------------------------------------------------------------------------------------
// Numeric literal edge cases
// ---------------------------------------------------------------------------------------------

TEST(TokenizerNumbers, FloatWithTrailingDotThenOperator) {
  expectTokens("1.+2", {Flt(1.0), Op(OperatorType::Add), Int(2)});
}
TEST(TokenizerNumbers, FloatThenIdentifier) { expectTokens("1.5 x", {Flt(1.5), Id("x")}); }
TEST(TokenizerNumbers, FloatWithExponentThenOperator) {
  expectTokens("1e3+1", {Flt(1000.0), Op(OperatorType::Add), Int(1)});
}
TEST(TokenizerNumbers, DotFollowedByNonDigitIsNotAFloat) { expectError("."); }
TEST(TokenizerNumbers, FloatUnderscoreSeparatedDigits) { expectTokens("1_0.5", {Flt(10.5)}); }
TEST(TokenizerNumbers, FloatUnderscoreInFraction) { expectTokens("1.0_5", {Flt(1.05)}); }
TEST(TokenizerNumbers, FloatUnderscoreInExponent) { expectTokens("1e1_0", {Flt(1e10)}); }
TEST(TokenizerNumbers, ZeroExponent) { expectTokens("5e0", {Flt(5.0)}); }
TEST(TokenizerNumbers, LargeExponent) { expectTokens("1e300", {Flt(1e300)}); }
TEST(TokenizerNumbers, IntegerFollowedByDelimiter) {
  expectTokens("1,2", {Int(1), Delim(DelimiterType::Comma), Int(2)});
}
TEST(TokenizerNumbers, IntegerThenNewline) { expectTokens("1\n", {Int(1), NL()}); }
TEST(TokenizerNumbers, FloatThenNewline) { expectTokens("1.5\n", {Flt(1.5), NL()}); }
TEST(TokenizerNumbers, IntegerAdjacentToOperators) {
  expectTokens("1+2-3*4", {Int(1), Op(OperatorType::Add), Int(2), Op(OperatorType::Subtract),
                           Int(3), Op(OperatorType::Multiply), Int(4)});
}
TEST(TokenizerNumbers, IntegerAboveInt64MaxIsNotSilentlyWrapped) {
  auto Got = lexAll("9223372036854775808", false);
  if (Got.has_value()) {
    ASSERT_EQ(Got->size(), 1u);
    EXPECT_NE(Got->front().Type, TokenType::Value)
        << "value above INT64_MAX must not be reported as an Integer";
  }
}
TEST(TokenizerNumbers, DISABLED_UnderscoreSeparatedInteger) { expectTokens("1_000", {Int(1000)}); }
TEST(TokenizerNumbers, DoubleUnderscoreIsNotANumber) {
  auto Got = lexAll("1__0", false);
  if (Got.has_value()) {
    ASSERT_NE(Got->size(), 1);
    ASSERT_NE((*Got)[0].Type, TokenType::Value);
    EXPECT_FALSE(std::holds_alternative<IntegerType>((*Got)[0].Value));
  }
}

// ---------------------------------------------------------------------------------------------
// Whitespace corner cases
// ---------------------------------------------------------------------------------------------

TEST(TokenizerWhitespaceEdge, TrailingSpacesBeforeNewline) {
  expectTokensWithWhitespace("a \n", {Id("a"), WS(), NL()});
}
TEST(TokenizerWhitespaceEdge, TwoSpacesBetweenTokensIsPartialIndentation) { expectError("a  b"); }
TEST(TokenizerWhitespaceEdge, FourSpacesBetweenTokensIsAnIndentation) {
  expectTokensWithWhitespace("a    b", {Id("a"), Indent(), Id("b")});
}
TEST(TokenizerWhitespaceEdge, WhitespaceOnlySource) {
  expectTokensWithWhitespace("    ", {Indent()});
}
TEST(TokenizerWhitespaceEdge, SourceWithoutTrailingNewline) {
  expectTokens("a\nb", {Id("a"), NL(), Id("b")});
}
TEST(TokenizerWhitespaceEdge, TabIsRejected) { expectError("\tx"); }
TEST(TokenizerWhitespaceEdge, CarriageReturnIsRejected) { expectError("a\r\nb"); }

// ---------------------------------------------------------------------------------------------
// Location tracking for new token kinds
// ---------------------------------------------------------------------------------------------

TEST(TokenizerLocationsExtra, BooleanLocationsAndColumns) {
  Tokenizer T{"x = True"};
  std::vector<Token> Toks;
  while (true) {
    auto N = T.getNextToken();
    ASSERT_TRUE(N.has_value());
    if (N->Type == TokenType::Eof)
      break;
    if (N->Type != TokenType::Whitespace)
      Toks.push_back(*N);
  }
  ASSERT_EQ(Toks.size(), 3u);
  EXPECT_EQ(Toks[2].Type, TokenType::Value);
  EXPECT_EQ(Toks[2].Location.Column, 4u);
  EXPECT_EQ(Toks[2].Location.Line, 0u);
}

TEST(TokenizerLocationsExtra, LogicalOperatorAdvancesColumnByWordLength) {
  Tokenizer T{"and x"};
  auto A = T.getNextToken();
  ASSERT_TRUE(A.has_value());
  EXPECT_EQ(A->Type, TokenType::Operator);
  auto WSTok = T.getNextToken();
  ASSERT_TRUE(WSTok.has_value());
  EXPECT_EQ(WSTok->Location.Column, 3u);
}

// ---------------------------------------------------------------------------------------------
// Error messages
// ---------------------------------------------------------------------------------------------

TEST(TokenizerErrorMessages, UnsupportedCharacterIsNamedInMessage) {
  auto Got = lexAll("a = $b", false);
  ASSERT_FALSE(Got.has_value());
  EXPECT_NE(Got.error().find('$'), std::string::npos) << Got.error();
}

TEST(TokenizerErrorMessages, PartialIndentationMessageMentionsCount) {
  auto Got = lexAll("  x", false);
  ASSERT_FALSE(Got.has_value());
  EXPECT_NE(Got.error().find('2'), std::string::npos) << Got.error();
}

TEST(TokenizerErrorMessages, MessageIsWellFormedForPartialIndentation) {
  auto Got = lexAll("  x", false);
  ASSERT_FALSE(Got.has_value());
  EXPECT_EQ(Got.error().find("()"), std::string::npos) << Got.error();
}

// ---------------------------------------------------------------------------------------------
// Realistic Python snippets
// ---------------------------------------------------------------------------------------------

TEST(TokenizerPython, SimpleAssignment) {
  expectTokens("x = 5", {Id("x"), Op(OperatorType::Assign), Int(5)});
}

TEST(TokenizerPython, AugmentedAssignment) {
  expectTokens("count += 1", {Id("count"), Op(OperatorType::UnaryAdd), Int(1)});
}

TEST(TokenizerPython, ChainedAssignment) {
  expectTokens("a = b = 0",
               {Id("a"), Op(OperatorType::Assign), Id("b"), Op(OperatorType::Assign), Int(0)});
}

TEST(TokenizerPython, TupleUnpacking) {
  expectTokens("a, b = b, a",
               {Id("a"), Delim(DelimiterType::Comma), Id("b"), Op(OperatorType::Assign), Id("b"),
                Delim(DelimiterType::Comma), Id("a")});
}

TEST(TokenizerPython, ArithmeticExpression) {
  expectTokens("(a + b) * c - d / e // f % g ** 2",
               {Delim(DelimiterType::LeftParenthesis), Id("a"), Op(OperatorType::Add), Id("b"),
                Delim(DelimiterType::RightParenthesis), Op(OperatorType::Multiply), Id("c"),
                Op(OperatorType::Subtract), Id("d"), Op(OperatorType::FloatDivide), Id("e"),
                Op(OperatorType::IntegerDivide), Id("f"), Op(OperatorType::Modulo), Id("g"),
                Op(OperatorType::Exponentiate), Int(2)});
}

TEST(TokenizerPython, BitwiseExpression) {
  expectTokens("~a & b | c ^ d << 2 >> 1",
               {Op(OperatorType::BitwiseInvert), Id("a"), Op(OperatorType::BitwiseAnd), Id("b"),
                Op(OperatorType::BitwiseOr), Id("c"), Op(OperatorType::BitwiseXor), Id("d"),
                Op(OperatorType::LeftShift), Int(2), Op(OperatorType::RightShift), Int(1)});
}

TEST(TokenizerPython, ComparisonChain) {
  expectTokens("0 <= x < 10 != y >= 3 > z == 1",
               {Int(0), Op(OperatorType::LessOrEqual), Id("x"), Op(OperatorType::Less), Int(10),
                Op(OperatorType::NotEqual), Id("y"), Op(OperatorType::GreaterOrEqual), Int(3),
                Op(OperatorType::Greater), Id("z"), Op(OperatorType::Equal), Int(1)});
}

TEST(TokenizerPython, BooleanExpression) {
  expectTokens("not a and b or c",
               {Op(OperatorType::LogicalNot), Id("a"), Op(OperatorType::LogicalAnd), Id("b"),
                Op(OperatorType::LogicalOr), Id("c")});
}

TEST(TokenizerPython, BooleanAndNoneLiterals) {
  expectTokens("x = True", {Id("x"), Op(OperatorType::Assign), Bool(true)});
  expectTokens("x = False", {Id("x"), Op(OperatorType::Assign), Bool(false)});
  expectTokens("x = None", {Id("x"), Op(OperatorType::Assign), Kw(KeywordType::None)});
}

TEST(TokenizerPython, FunctionCall) {
  expectTokens("print(a, 1, 2.5)",
               {Id("print"), Delim(DelimiterType::LeftParenthesis), Id("a"),
                Delim(DelimiterType::Comma), Int(1), Delim(DelimiterType::Comma), Flt(2.5),
                Delim(DelimiterType::RightParenthesis)});
}

TEST(TokenizerPython, NestedFunctionCalls) {
  expectTokens("f(g(x), h(y))",
               {Id("f"), Delim(DelimiterType::LeftParenthesis), Id("g"),
                Delim(DelimiterType::LeftParenthesis), Id("x"),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::Comma), Id("h"),
                Delim(DelimiterType::LeftParenthesis), Id("y"),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::RightParenthesis)});
}

TEST(TokenizerPython, ListLiteralAndIndexing) {
  expectTokens("xs = [1, 2, 3]\nxs[0]",
               {Id("xs"), Op(OperatorType::Assign), Delim(DelimiterType::LeftBracket), Int(1),
                Delim(DelimiterType::Comma), Int(2), Delim(DelimiterType::Comma), Int(3),
                Delim(DelimiterType::RightBracket), NL(), Id("xs"),
                Delim(DelimiterType::LeftBracket), Int(0), Delim(DelimiterType::RightBracket)});
}

TEST(TokenizerPython, DictLiteral) {
  expectTokens("d = {a: 1, b: 2}",
               {Id("d"), Op(OperatorType::Assign), Delim(DelimiterType::LeftBraces), Id("a"),
                Delim(DelimiterType::Colon), Int(1), Delim(DelimiterType::Comma), Id("b"),
                Delim(DelimiterType::Colon), Int(2), Delim(DelimiterType::RightBraces)});
}

TEST(TokenizerPython, SliceSyntax) {
  expectTokens("xs[1:2]",
               {Id("xs"), Delim(DelimiterType::LeftBracket), Int(1), Delim(DelimiterType::Colon),
                Int(2), Delim(DelimiterType::RightBracket)});
}

TEST(TokenizerPython, LambdaExpression) {
  expectTokens("lambda x, y: x + y",
               {Kw(KeywordType::Lambda), Id("x"), Delim(DelimiterType::Comma), Id("y"),
                Delim(DelimiterType::Colon), Id("x"), Op(OperatorType::Add), Id("y")});
}

TEST(TokenizerPython, ImportStatements) {
  expectTokens("import os", {Kw(KeywordType::Import), Id("os")});
  expectTokens("import os as o", {Kw(KeywordType::Import), Id("os"), Kw(KeywordType::As), Id("o")});
  expectTokens("from os import path",
               {Kw(KeywordType::From), Id("os"), Kw(KeywordType::Import), Id("path")});
}

TEST(TokenizerPython, SimpleFunctionDefinition) {
  expectTokensWithWhitespace("def add(a, b):\n    return a + b\n",
                             {Kw(KeywordType::Def),
                              WS(),
                              Id("add"),
                              Delim(DelimiterType::LeftParenthesis),
                              Id("a"),
                              Delim(DelimiterType::Comma),
                              WS(),
                              Id("b"),
                              Delim(DelimiterType::RightParenthesis),
                              Delim(DelimiterType::Colon),
                              NL(),
                              Indent(),
                              Kw(KeywordType::Return),
                              WS(),
                              Id("a"),
                              WS(),
                              Op(OperatorType::Add),
                              WS(),
                              Id("b"),
                              NL()});
}

TEST(TokenizerPython, FunctionWithReturnAnnotation) {
  expectTokens("def f(x) -> int:",
               {Kw(KeywordType::Def), Id("f"), Delim(DelimiterType::LeftParenthesis), Id("x"),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::Arrow), Id("int"),
                Delim(DelimiterType::Colon)});
}

TEST(TokenizerPython, FunctionWithParameterAnnotations) {
  expectTokens("def f(x: int, y: float):",
               {Kw(KeywordType::Def), Id("f"), Delim(DelimiterType::LeftParenthesis), Id("x"),
                Delim(DelimiterType::Colon), Id("int"), Delim(DelimiterType::Comma), Id("y"),
                Delim(DelimiterType::Colon), Id("float"), Delim(DelimiterType::RightParenthesis),
                Delim(DelimiterType::Colon)});
}

TEST(TokenizerPython, IfElifElse) {
  expectTokensWithWhitespace("if a:\n    x\nelif b:\n    y\nelse:\n    z\n",
                             {Kw(KeywordType::If),
                              WS(),
                              Id("a"),
                              Delim(DelimiterType::Colon),
                              NL(),
                              Indent(),
                              Id("x"),
                              NL(),
                              Kw(KeywordType::Elif),
                              WS(),
                              Id("b"),
                              Delim(DelimiterType::Colon),
                              NL(),
                              Indent(),
                              Id("y"),
                              NL(),
                              Kw(KeywordType::Else),
                              Delim(DelimiterType::Colon),
                              NL(),
                              Indent(),
                              Id("z"),
                              NL()});
}

TEST(TokenizerPython, WhileLoopWithBreakAndContinue) {
  expectTokensWithWhitespace("while True:\n    break\n    continue\n",
                             {Kw(KeywordType::While), WS(), Bool(true), Delim(DelimiterType::Colon),
                              NL(), Indent(), Kw(KeywordType::Break), NL(), Indent(),
                              Kw(KeywordType::Continue), NL()});
}

TEST(TokenizerPython, ForLoop) {
  expectTokens("for i in range(10):",
               {Kw(KeywordType::For), Id("i"), Kw(KeywordType::In), Id("range"),
                Delim(DelimiterType::LeftParenthesis), Int(10),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::Colon)});
}

TEST(TokenizerPython, ClassDefinition) {
  expectTokensWithWhitespace("class A:\n    pass\n",
                             {Kw(KeywordType::Class), WS(), Id("A"), Delim(DelimiterType::Colon),
                              NL(), Indent(), Kw(KeywordType::Pass), NL()});
}

TEST(TokenizerPython, ClassWithBase) {
  expectTokens("class B(A):",
               {Kw(KeywordType::Class), Id("B"), Delim(DelimiterType::LeftParenthesis), Id("A"),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::Colon)});
}

TEST(TokenizerPython, TryExceptFinally) {
  expectTokens("try:\nexcept E as e:\nfinally:\n",
               {Kw(KeywordType::Try), Delim(DelimiterType::Colon), NL(), Kw(KeywordType::Except),
                Id("E"), Kw(KeywordType::As), Id("e"), Delim(DelimiterType::Colon), NL(),
                Kw(KeywordType::Finally), Delim(DelimiterType::Colon), NL()});
}

TEST(TokenizerPython, RaiseAssertDel) {
  expectTokens("raise E", {Kw(KeywordType::Raise), Id("E")});
  expectTokens("assert x, y",
               {Kw(KeywordType::Assert), Id("x"), Delim(DelimiterType::Comma), Id("y")});
  expectTokens("del x", {Kw(KeywordType::Del), Id("x")});
}

TEST(TokenizerPython, GlobalAndNonlocal) {
  expectTokens("global g", {Kw(KeywordType::Global), Id("g")});
  expectTokens("nonlocal n", {Kw(KeywordType::Nonlocal), Id("n")});
}

TEST(TokenizerPython, WithStatement) {
  expectTokens("with f() as g:",
               {Kw(KeywordType::With), Id("f"), Delim(DelimiterType::LeftParenthesis),
                Delim(DelimiterType::RightParenthesis), Kw(KeywordType::As), Id("g"),
                Delim(DelimiterType::Colon)});
}

TEST(TokenizerPython, AsyncAwaitYield) {
  expectTokens("async def f():",
               {Kw(KeywordType::Async), Kw(KeywordType::Def), Id("f"),
                Delim(DelimiterType::LeftParenthesis), Delim(DelimiterType::RightParenthesis),
                Delim(DelimiterType::Colon)});
  expectTokens("await x", {Kw(KeywordType::Await), Id("x")});
  expectTokens("yield x", {Kw(KeywordType::Yield), Id("x")});
}

TEST(TokenizerPython, ConditionalExpression) {
  expectTokens("a if c else b",
               {Id("a"), Kw(KeywordType::If), Id("c"), Kw(KeywordType::Else), Id("b")});
}

TEST(TokenizerPython, MultipleStatementsOnOneLine) {
  expectTokens("a = 1; b = 2; c = 3",
               {Id("a"), Op(OperatorType::Assign), Int(1), Delim(DelimiterType::Semicolon), Id("b"),
                Op(OperatorType::Assign), Int(2), Delim(DelimiterType::Semicolon), Id("c"),
                Op(OperatorType::Assign), Int(3)});
}

TEST(TokenizerPython, MultiLineProgram) {
  const std::string Source = "def fib(n):\n"
                             "    if n < 2:\n"
                             "        return n\n"
                             "    return fib(n - 1) + fib(n - 2)\n";
  auto Got = lexAll(Source, /*KeepWhitespace=*/false);
  ASSERT_TRUE(Got.has_value()) << Got.error();

  std::size_t NewLines = 0;
  std::size_t Indents = 0;
  std::size_t Keywords = 0;
  for (const auto &Tok : *Got) {
    NewLines += Tok.Type == TokenType::NewLine;
    Indents += Tok.Type == TokenType::Indent;
    Keywords += Tok.Type == TokenType::Keyword;
  }
  EXPECT_EQ(NewLines, 4u);
  EXPECT_EQ(Indents, 4u);  // 1 + 2 + 1
  EXPECT_EQ(Keywords, 4u); // def, if, return, return
}

// ---------------------------------------------------------------------------------------------
// detail:: helpers
// ---------------------------------------------------------------------------------------------

TEST(DetailIntegerLiteral, ParsesLeadingDigits) {
  auto R = detail::getIntegerLiteral("123abc");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(R->first, 123);
  EXPECT_EQ(R->second, 3u);
}

TEST(DetailIntegerLiteral, ParsesWholeString) {
  auto R = detail::getIntegerLiteral("42");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(R->first, 42);
  EXPECT_EQ(R->second, 2u);
}

TEST(DetailIntegerLiteral, RejectsNonDigits) {
  EXPECT_FALSE(detail::getIntegerLiteral("abc").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral("+").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral("").has_value());
}

TEST(DetailIntegerLiteral, StopsAtDecimalPoint) {
  auto R = detail::getIntegerLiteral("12.5");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(R->first, 12);
  EXPECT_EQ(R->second, 2u);
}

TEST(DetailFloatLiteral, ParsesSimpleFloat) {
  auto R = detail::getFloatLiteral("3.14 rest");
  ASSERT_TRUE(R.has_value());
  EXPECT_DOUBLE_EQ(R->first, 3.14);
  EXPECT_EQ(R->second, 4u);
}

TEST(DetailFloatLiteral, ParsesExponent) {
  auto R = detail::getFloatLiteral("1.5e2x");
  ASSERT_TRUE(R.has_value());
  EXPECT_DOUBLE_EQ(R->first, 150.0);
  EXPECT_EQ(R->second, 5u);
}

TEST(DetailFloatLiteral, RejectsNonNumbers) {
  EXPECT_FALSE(detail::getFloatLiteral("abc").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("(").has_value());
}

TEST(DetailOperatorOrDelimiter, ReturnsOperatorWithLength) {
  auto R = detail::getOperatorOrDelimiterType("**= 2");
  ASSERT_TRUE(R.has_value());
  ASSERT_TRUE(std::holds_alternative<OperatorType>(R->first));
  EXPECT_EQ(std::get<OperatorType>(R->first), OperatorType::UnaryExponentiate);
  EXPECT_EQ(R->second, 3u);
}

TEST(DetailOperatorOrDelimiter, ReturnsDelimiterWithLength) {
  auto R = detail::getOperatorOrDelimiterType("->x");
  ASSERT_TRUE(R.has_value());
  ASSERT_TRUE(std::holds_alternative<DelimiterType>(R->first));
  EXPECT_EQ(std::get<DelimiterType>(R->first), DelimiterType::Arrow);
  EXPECT_EQ(R->second, 2u);
}

TEST(DetailOperatorOrDelimiter, SingleCharacterOperatorLength) {
  auto R = detail::getOperatorOrDelimiterType("+1");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(std::get<OperatorType>(R->first), OperatorType::Add);
  EXPECT_EQ(R->second, 1u);
}

TEST(DetailOperatorOrDelimiter, RejectsNonOperators) {
  EXPECT_FALSE(detail::getOperatorOrDelimiterType("a").has_value());
  EXPECT_FALSE(detail::getOperatorOrDelimiterType("1").has_value());
  EXPECT_FALSE(detail::getOperatorOrDelimiterType("$").has_value());
}

TEST(DetailName, ParsesIdentifierPrefix) {
  auto R = detail::getName("foo_bar1 baz");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(*R, "foo_bar1");
}

TEST(DetailName, ParsesUnderscoreStart) {
  auto R = detail::getName("_x");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(*R, "_x");
}

TEST(DetailName, RejectsDigitStart) { EXPECT_FALSE(detail::getName("1abc").has_value()); }
TEST(DetailName, RejectsOperatorStart) { EXPECT_FALSE(detail::getName("+abc").has_value()); }
TEST(DetailName, StopsAtNonNameCharacter) {
  auto R = detail::getName("abc(def)");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(*R, "abc");
}
