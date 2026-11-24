#include "frontend/Tokenizer.h"
#include "frontend/Error.h"
#include "frontend/Types.h"

#include "TestUtils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace compyled;
using namespace compyled::lexer;
using namespace compyled::test;

namespace {

// -------------------------------------------------------------------------------------------------
// Test helpers
// -------------------------------------------------------------------------------------------------

// Asserts that `Source` tokenizes into exactly `Want`, which must include the final Eof.
void expectTokens(const std::string &Source, const std::vector<Expected> &Want) {
  SCOPED_TRACE("source: \"" + Source + "\"");
  std::vector<Token> Got;
  try {
    Got = tokenize(Source);
  } catch (const std::exception &E) {
    FAIL() << "tokenizer threw: " << E.what();
  }

  const auto Common = std::min(Got.size(), Want.size());
  for (std::size_t I = 0; I < Common; ++I) {
    EXPECT_TRUE(matches(Got[I], Want[I]))
        << "token " << I << ": expected " << toString(Want[I]) << ", got " << toString(Got[I]);
  }
  for (std::size_t I = Common; I < Got.size(); ++I) {
    ADD_FAILURE() << "unexpected extra token " << I << ": " << toString(Got[I]);
  }
  for (std::size_t I = Common; I < Want.size(); ++I) {
    ADD_FAILURE() << "missing token " << I << ": " << toString(Want[I]);
  }
}

// Asserts that tokenizing `Source` fails with a compiler error.
void expectError(const std::string &Source) {
  SCOPED_TRACE("source: \"" + Source + "\"");
  EXPECT_THROW(tokenize(Source), error::CompyledException);
}

// Tokenizes `Source` and returns every token except Eof, failing the test if the tokenizer throws.
std::vector<Token> tokensOf(const std::string &Source) {
  try {
    auto Tokens = tokenize(Source);
    Tokens.pop_back();
    return Tokens;
  } catch (const std::exception &E) {
    ADD_FAILURE() << "tokenizer threw on \"" << Source << "\": " << E.what();
    return {};
  }
}

void expectLocation(const Token &T, std::size_t Line, std::size_t Column) {
  EXPECT_EQ(T.Location.Line, Line) << toString(T);
  EXPECT_EQ(T.Location.Column, Column) << toString(T);
}

} // namespace

// -------------------------------------------------------------------------------------------------
// Construction
// -------------------------------------------------------------------------------------------------

TEST(TokenizerConstruction, AcceptsEmptySource) { EXPECT_NO_THROW(Tokenizer{""}); }

TEST(TokenizerConstruction, ThrowsWhenFirstTokenIsInvalid) {
  EXPECT_THROW(Tokenizer{"$"}, error::CompyledException);
  EXPECT_THROW(Tokenizer{"?x"}, error::CompyledException);
}

// -------------------------------------------------------------------------------------------------
// Peeking and consuming
// -------------------------------------------------------------------------------------------------

TEST(TokenizerPeekConsume, PeekReturnsFirstTokenAfterConstruction) {
  Tokenizer T{"x = 1"};
  EXPECT_TRUE(matches(T.peekNextToken(), Id("x"))) << toString(T.peekNextToken());
}

TEST(TokenizerPeekConsume, PeekDoesNotAdvance) {
  Tokenizer T{"x = 1"};
  T.peekNextToken();
  T.peekNextToken();
  EXPECT_TRUE(matches(T.peekNextToken(), Id("x"))) << toString(T.peekNextToken());
}

TEST(TokenizerPeekConsume, ConsumeReturnsPeekedToken) {
  Tokenizer T{"x = 1"};
  const auto Peeked = T.peekNextToken();
  const auto Consumed = T.consumeNextToken();
  EXPECT_EQ(Peeked.Type, Consumed.Type);
  EXPECT_EQ(Peeked.Value, Consumed.Value);
  EXPECT_EQ(Peeked.Location.Line, Consumed.Location.Line);
  EXPECT_EQ(Peeked.Location.Column, Consumed.Location.Column);
}

TEST(TokenizerPeekConsume, PeekAfterConsumeReturnsFollowingToken) {
  Tokenizer T{"x = 1"};
  T.consumeNextToken();
  EXPECT_TRUE(matches(T.peekNextToken(), Op(OperatorType::Assign))) << toString(T.peekNextToken());
  T.consumeNextToken();
  EXPECT_TRUE(matches(T.peekNextToken(), Int(1))) << toString(T.peekNextToken());
}

TEST(TokenizerPeekConsume, ConsumeWalksThroughAllTokensInOrder) {
  Tokenizer T{"x = 1"};
  EXPECT_TRUE(matches(T.consumeNextToken(), Id("x")));
  EXPECT_TRUE(matches(T.consumeNextToken(), Op(OperatorType::Assign)));
  EXPECT_TRUE(matches(T.consumeNextToken(), Int(1)));
  EXPECT_EQ(T.consumeNextToken().Type, TokenType::Eof);
}

TEST(TokenizerPeekConsume, PeekAndConsumeInterleaved) {
  Tokenizer T{"a + b"};
  for (const auto &Want : {Id("a"), Op(OperatorType::Add), Id("b")}) {
    EXPECT_TRUE(matches(T.peekNextToken(), Want)) << toString(T.peekNextToken());
    EXPECT_TRUE(matches(T.consumeNextToken(), Want));
  }
  EXPECT_EQ(T.peekNextToken().Type, TokenType::Eof);
}

// -------------------------------------------------------------------------------------------------
// End of input
// -------------------------------------------------------------------------------------------------

TEST(TokenizerEof, EmptySourceYieldsOnlyEof) { expectTokens("", {Eof()}); }

TEST(TokenizerEof, PeekOnEmptySourceIsEof) {
  Tokenizer T{""};
  EXPECT_EQ(T.peekNextToken().Type, TokenType::Eof);
}

TEST(TokenizerEof, PeekAtEofIsStable) {
  Tokenizer T{""};
  for (int I = 0; I < 3; ++I) {
    EXPECT_EQ(T.peekNextToken().Type, TokenType::Eof);
  }
}

TEST(TokenizerEof, ConsumingEofReturnsEof) {
  Tokenizer T{""};
  EXPECT_EQ(T.consumeNextToken().Type, TokenType::Eof);
}

TEST(TokenizerEof, EofFollowsLastToken) {
  Tokenizer T{"x"};
  EXPECT_TRUE(matches(T.consumeNextToken(), Id("x")));
  EXPECT_EQ(T.consumeNextToken().Type, TokenType::Eof);
}

TEST(TokenizerEof, EofFollowsFinalNewLine) {
  Tokenizer T{"x\n"};
  EXPECT_TRUE(matches(T.consumeNextToken(), Id("x")));
  EXPECT_TRUE(matches(T.consumeNextToken(), NL()));
  EXPECT_EQ(T.consumeNextToken().Type, TokenType::Eof);
}

TEST(TokenizerEof, EofFollowsClosingDedents) {
  Tokenizer T{"if a:\n    b\n"};
  std::vector<Token> Tokens;
  for (auto Tok = T.consumeNextToken(); Tok.Type != TokenType::Eof && Tokens.size() < MAX_TOKENS;
       Tok = T.consumeNextToken()) {
    Tokens.push_back(Tok);
  }
  ASSERT_FALSE(Tokens.empty());
  EXPECT_EQ(Tokens.back().Type, TokenType::Dedent) << toString(Tokens.back());
}

// -------------------------------------------------------------------------------------------------
// Integer literals
// -------------------------------------------------------------------------------------------------

TEST(TokenizerIntegers, Zero) { expectTokens("0", {Int(0), Eof()}); }
TEST(TokenizerIntegers, SingleDigit) { expectTokens("7", {Int(7), Eof()}); }
TEST(TokenizerIntegers, MultipleDigits) { expectTokens("1234567890", {Int(1234567890), Eof()}); }
TEST(TokenizerIntegers, LargerThan32Bits) { expectTokens("4294967296", {Int(4294967296), Eof()}); }

TEST(TokenizerIntegers, MaxInt64) {
  expectTokens("9223372036854775807", {Int(9223372036854775807), Eof()});
}

TEST(TokenizerIntegers, OverflowingInt64IsAnError) {
  expectError("9223372036854775808");
  expectError("99999999999999999999999");
}

TEST(TokenizerIntegers, DISABLED_UnderscoreSeparators) {
  expectTokens("1_000", {Int(1000), Eof()});
  expectTokens("1_000_000", {Int(1000000), Eof()});
  expectTokens("1_2_3", {Int(123), Eof()});
}

TEST(TokenizerIntegers, DISABLED_ConsecutiveUnderscoresAreAnError) { expectError("1__000"); }
TEST(TokenizerIntegers, DISABLED_TrailingUnderscoreIsAnError) { expectError("1000_"); }

TEST(TokenizerIntegers, LeadingUnderscoreMakesAnIdentifier) {
  expectTokens("_1000", {Id("_1000"), Eof()});
}

TEST(TokenizerIntegers, DISABLED_RepeatedZerosAreAllowed) {
  expectTokens("00", {Int(0), Eof()});
  expectTokens("0_0", {Int(0), Eof()});
}

TEST(TokenizerIntegers, DISABLED_LeadingZeroInNonZeroDecimalIsAnError) {
  expectError("01");
  expectError("007");
}

TEST(TokenizerIntegers, DISABLED_HexadecimalLiterals) {
  expectTokens("0xff", {Int(255), Eof()});
  expectTokens("0XFF", {Int(255), Eof()});
  expectTokens("0x_dead_beef", {Int(0xdeadbeef), Eof()});
}

TEST(TokenizerIntegers, DISABLED_OctalLiterals) {
  expectTokens("0o17", {Int(15), Eof()});
  expectTokens("0O17", {Int(15), Eof()});
  expectTokens("0o_7_7", {Int(63), Eof()});
}

TEST(TokenizerIntegers, DISABLED_BinaryLiterals) {
  expectTokens("0b101", {Int(5), Eof()});
  expectTokens("0B101", {Int(5), Eof()});
  expectTokens("0b_1111_0000", {Int(240), Eof()});
}

TEST(TokenizerIntegers, DISABLED_PrefixWithoutDigitsIsAnError) {
  expectError("0x");
  expectError("0o");
  expectError("0b");
}

TEST(TokenizerIntegers, DISABLED_InvalidDigitForBaseIsAnError) {
  expectError("0b2");
  expectError("0o8");
  expectError("0xg");
}

TEST(TokenizerIntegers, DISABLED_DigitsFollowedByLettersAreAnError) {
  expectError("1abc");
  expectError("123x");
}

TEST(TokenizerIntegers, SignsAreSeparateOperators) {
  expectTokens("-5", {Op(OperatorType::Subtract), Int(5), Eof()});
  expectTokens("+5", {Op(OperatorType::Add), Int(5), Eof()});
  expectTokens("~5", {Op(OperatorType::BitwiseInvert), Int(5), Eof()});
}

TEST(TokenizerIntegers, SeveralIntegers) {
  expectTokens("1 22 333", {Int(1), Int(22), Int(333), Eof()});
  expectTokens("1,2,3", {Int(1), Delim(DelimiterType::Comma), Int(2), Delim(DelimiterType::Comma),
                         Int(3), Eof()});
}

// -------------------------------------------------------------------------------------------------
// Float literals
// -------------------------------------------------------------------------------------------------

TEST(TokenizerFloats, IntegerAndFraction) { expectTokens("3.14", {Flt(3.14), Eof()}); }
TEST(TokenizerFloats, ZeroPointZero) { expectTokens("0.0", {Flt(0.0), Eof()}); }
TEST(TokenizerFloats, LeadingZeroFraction) { expectTokens("0.5", {Flt(0.5), Eof()}); }
TEST(TokenizerFloats, NoIntegerPart) { expectTokens(".5", {Flt(0.5), Eof()}); }
TEST(TokenizerFloats, NoFractionPart) { expectTokens("5.", {Flt(5.0), Eof()}); }
TEST(TokenizerFloats, Exponent) { expectTokens("1e3", {Flt(1000.0), Eof()}); }
TEST(TokenizerFloats, UpperCaseExponent) { expectTokens("1E3", {Flt(1000.0), Eof()}); }
TEST(TokenizerFloats, NegativeExponent) { expectTokens("1e-3", {Flt(0.001), Eof()}); }
TEST(TokenizerFloats, PositiveExponent) { expectTokens("2.5e+2", {Flt(250.0), Eof()}); }
TEST(TokenizerFloats, FractionAndExponent) { expectTokens("1.5e3", {Flt(1500.0), Eof()}); }
TEST(TokenizerFloats, NoFractionPartWithExponent) { expectTokens("1.e2", {Flt(100.0), Eof()}); }
TEST(TokenizerFloats, NoIntegerPartWithExponent) { expectTokens(".5e1", {Flt(5.0), Eof()}); }
TEST(TokenizerFloats, ZeroExponent) { expectTokens("0e0", {Flt(0.0), Eof()}); }

TEST(TokenizerFloats, LeadingZerosAreAllowed) {
  expectTokens("007.5", {Flt(7.5), Eof()});
  expectTokens("00e1", {Flt(0.0), Eof()});
}

TEST(TokenizerFloats, UnderscoreSeparators) {
  expectTokens("1_000.5", {Flt(1000.5), Eof()});
  expectTokens("1.000_1", {Flt(1.0001), Eof()});
  expectTokens("1e1_0", {Flt(1e10), Eof()});
}

TEST(TokenizerFloats, MisplacedUnderscoresAreAnError) {
  expectError("1__0.5");
  expectError("1_.5");
  expectError("1.5_");
  expectError("1e_5");
}

TEST(TokenizerFloats, ExponentWithoutDigitsIsAnError) {
  expectError("1e");
  expectError("1e+");
  expectError("1.5e-");
}

TEST(TokenizerFloats, SecondDotStartsAnotherFloat) {
  expectTokens("1.5.2", {Flt(1.5), Flt(0.2), Eof()});
}

TEST(TokenizerFloats, FloatIsNotAnInteger) {
  const auto Tokens = tokensOf("2.0");
  ASSERT_FALSE(Tokens.empty());
  EXPECT_TRUE(std::holds_alternative<types::FloatType>(Tokens[0].Value)) << toString(Tokens[0]);
}

TEST(TokenizerFloats, IntegerIsNotAFloat) {
  const auto Tokens = tokensOf("2");
  ASSERT_FALSE(Tokens.empty());
  EXPECT_TRUE(std::holds_alternative<types::IntegerType>(Tokens[0].Value)) << toString(Tokens[0]);
}

TEST(TokenizerFloats, SignsAreSeparateOperators) {
  expectTokens("-2.5", {Op(OperatorType::Subtract), Flt(2.5), Eof()});
  expectTokens("+.5", {Op(OperatorType::Add), Flt(0.5), Eof()});
}

TEST(TokenizerFloats, FloatsInExpression) {
  expectTokens("1.5 + 2.25 * .5", {Flt(1.5), Op(OperatorType::Add), Flt(2.25),
                                   Op(OperatorType::Multiply), Flt(0.5), Eof()});
}

// -------------------------------------------------------------------------------------------------
// Identifiers
// -------------------------------------------------------------------------------------------------

TEST(TokenizerIdentifiers, SingleLetter) { expectTokens("x", {Id("x"), Eof()}); }
TEST(TokenizerIdentifiers, SnakeCase) {
  expectTokens("some_long_name", {Id("some_long_name"), Eof()});
}
TEST(TokenizerIdentifiers, WithDigits) { expectTokens("var123", {Id("var123"), Eof()}); }
TEST(TokenizerIdentifiers, LeadingUnderscore) { expectTokens("_private", {Id("_private"), Eof()}); }
TEST(TokenizerIdentifiers, Dunder) { expectTokens("__init__", {Id("__init__"), Eof()}); }
TEST(TokenizerIdentifiers, OnlyUnderscore) { expectTokens("_", {Id("_"), Eof()}); }
TEST(TokenizerIdentifiers, UpperCase) { expectTokens("CONSTANT", {Id("CONSTANT"), Eof()}); }
TEST(TokenizerIdentifiers, CamelCase) { expectTokens("camelCase", {Id("camelCase"), Eof()}); }
TEST(TokenizerIdentifiers, PascalCase) { expectTokens("MyClass", {Id("MyClass"), Eof()}); }

TEST(TokenizerIdentifiers, ReservedWordsAreCaseSensitive) {
  expectTokens("If", {Id("If"), Eof()});
  expectTokens("WHILE", {Id("WHILE"), Eof()});
  expectTokens("Def", {Id("Def"), Eof()});
  expectTokens("true", {Id("true"), Eof()});
  expectTokens("false", {Id("false"), Eof()});
  expectTokens("none", {Id("none"), Eof()});
  expectTokens("AND", {Id("AND"), Eof()});
  expectTokens("Int", {Id("Int"), Eof()});
  expectTokens("Float", {Id("Float"), Eof()});
  expectTokens("BOOL", {Id("BOOL"), Eof()});
  expectTokens("Range", {Id("Range"), Eof()});
}

TEST(TokenizerIdentifiers, SeveralIdentifiers) {
  expectTokens("a b c", {Id("a"), Id("b"), Id("c"), Eof()});
}

TEST(TokenizerIdentifiers, IdentifierFollowedBySymbol) {
  expectTokens("f(", {Id("f"), Delim(DelimiterType::LeftParenthesis), Eof()});
  expectTokens("x+", {Id("x"), Op(OperatorType::Add), Eof()});
}

// -------------------------------------------------------------------------------------------------
// Reserved words: keywords, word operators, boolean literals and base types
// -------------------------------------------------------------------------------------------------

struct ReservedWordCase {
  const char *Text;
  Expected Want;
};

class ReservedWordTest : public ::testing::TestWithParam<ReservedWordCase> {};

TEST_P(ReservedWordTest, IsRecognisedAlone) {
  const auto &[Text, Want] = GetParam();
  expectTokens(Text, {Want, Eof()});
}

TEST_P(ReservedWordTest, IsRecognisedBeforeIdentifier) {
  const auto &[Text, Want] = GetParam();
  expectTokens(std::string(Text) + " x", {Want, Id("x"), Eof()});
}

TEST_P(ReservedWordTest, IsRecognisedAfterIdentifier) {
  const auto &[Text, Want] = GetParam();
  expectTokens("x " + std::string(Text), {Id("x"), Want, Eof()});
}

TEST_P(ReservedWordTest, IsRecognisedNextToSymbols) {
  const auto &[Text, Want] = GetParam();
  expectTokens("(" + std::string(Text) + ")", {Delim(DelimiterType::LeftParenthesis), Want,
                                               Delim(DelimiterType::RightParenthesis), Eof()});
}

TEST_P(ReservedWordTest, IsAnIdentifierWithSuffix) {
  const auto &[Text, Want] = GetParam();
  expectTokens(std::string(Text) + "_x", {Id(std::string(Text) + "_x"), Eof()});
  expectTokens(std::string(Text) + "1", {Id(std::string(Text) + "1"), Eof()});
}

TEST_P(ReservedWordTest, IsAnIdentifierWithPrefix) {
  const auto &[Text, Want] = GetParam();
  expectTokens("x_" + std::string(Text), {Id("x_" + std::string(Text)), Eof()});
  expectTokens("_" + std::string(Text), {Id("_" + std::string(Text)), Eof()});
}

INSTANTIATE_TEST_SUITE_P(AllReservedWords, ReservedWordTest,
                         ::testing::Values(
                             // Keywords
                             ReservedWordCase{"await", Kw(KeywordType::Await)},
                             ReservedWordCase{"else", Kw(KeywordType::Else)},
                             ReservedWordCase{"import", Kw(KeywordType::Import)},
                             ReservedWordCase{"pass", Kw(KeywordType::Pass)},
                             ReservedWordCase{"None", Kw(KeywordType::None)},
                             ReservedWordCase{"break", Kw(KeywordType::Break)},
                             ReservedWordCase{"except", Kw(KeywordType::Except)},
                             ReservedWordCase{"in", Kw(KeywordType::In)},
                             ReservedWordCase{"raise", Kw(KeywordType::Raise)},
                             ReservedWordCase{"class", Kw(KeywordType::Class)},
                             ReservedWordCase{"finally", Kw(KeywordType::Finally)},
                             ReservedWordCase{"is", Kw(KeywordType::Is)},
                             ReservedWordCase{"return", Kw(KeywordType::Return)},
                             ReservedWordCase{"continue", Kw(KeywordType::Continue)},
                             ReservedWordCase{"for", Kw(KeywordType::For)},
                             ReservedWordCase{"lambda", Kw(KeywordType::Lambda)},
                             ReservedWordCase{"try", Kw(KeywordType::Try)},
                             ReservedWordCase{"as", Kw(KeywordType::As)},
                             ReservedWordCase{"def", Kw(KeywordType::Def)},
                             ReservedWordCase{"from", Kw(KeywordType::From)},
                             ReservedWordCase{"nonlocal", Kw(KeywordType::Nonlocal)},
                             ReservedWordCase{"while", Kw(KeywordType::While)},
                             ReservedWordCase{"assert", Kw(KeywordType::Assert)},
                             ReservedWordCase{"del", Kw(KeywordType::Del)},
                             ReservedWordCase{"global", Kw(KeywordType::Global)},
                             ReservedWordCase{"with", Kw(KeywordType::With)},
                             ReservedWordCase{"async", Kw(KeywordType::Async)},
                             ReservedWordCase{"elif", Kw(KeywordType::Elif)},
                             ReservedWordCase{"if", Kw(KeywordType::If)},
                             ReservedWordCase{"yield", Kw(KeywordType::Yield)},
                             ReservedWordCase{"range", Kw(KeywordType::Range)},
                             // Word operators
                             ReservedWordCase{"and", Op(OperatorType::LogicalAnd)},
                             ReservedWordCase{"or", Op(OperatorType::LogicalOr)},
                             ReservedWordCase{"not", Op(OperatorType::LogicalNot)},
                             // Boolean literals
                             ReservedWordCase{"True", Bool(true)},
                             ReservedWordCase{"False", Bool(false)},
                             // Base types
                             ReservedWordCase{"int", Base(BaseType::Integer)},
                             ReservedWordCase{"float", Base(BaseType::Float)},
                             ReservedWordCase{"bool", Base(BaseType::Boolean)}),
                         [](const ::testing::TestParamInfo<ReservedWordCase> &Info) {
                           return std::string(Info.param.Text);
                         });

// -------------------------------------------------------------------------------------------------
// Operators and delimiters
// -------------------------------------------------------------------------------------------------

struct SymbolCase {
  const char *Text;
  Expected Want;
  const char *Name;
};

class SymbolTest : public ::testing::TestWithParam<SymbolCase> {};

TEST_P(SymbolTest, IsRecognisedAlone) {
  const auto &[Text, Want, Name] = GetParam();
  expectTokens(Text, {Want, Eof()});
}

TEST_P(SymbolTest, IsRecognisedBetweenIdentifiersWithSpaces) {
  const auto &[Text, Want, Name] = GetParam();
  expectTokens(std::string("a ") + Text + " b", {Id("a"), Want, Id("b"), Eof()});
}

TEST_P(SymbolTest, IsRecognisedBetweenIdentifiersWithoutSpaces) {
  const auto &[Text, Want, Name] = GetParam();
  expectTokens(std::string("a") + Text + "b", {Id("a"), Want, Id("b"), Eof()});
}

TEST_P(SymbolTest, IsRecognisedBeforeNumber) {
  const auto &[Text, Want, Name] = GetParam();
  expectTokens(std::string(Text) + "1", {Want, Int(1), Eof()});
}

TEST_P(SymbolTest, IsRecognisedAfterNumber) {
  const auto &[Text, Want, Name] = GetParam();
  expectTokens(std::string("1") + Text, {Int(1), Want, Eof()});
}

TEST_P(SymbolTest, IsRecognisedTwiceInARow) {
  const auto &[Text, Want, Name] = GetParam();
  expectTokens(std::string(Text) + " " + Text, {Want, Want, Eof()});
}

INSTANTIATE_TEST_SUITE_P(
    AllSymbols, SymbolTest,
    ::testing::Values(
        // Arithmetic operators
        SymbolCase{"+", Op(OperatorType::Add), "Add"},
        SymbolCase{"-", Op(OperatorType::Subtract), "Subtract"},
        SymbolCase{"*", Op(OperatorType::Multiply), "Multiply"},
        SymbolCase{"**", Op(OperatorType::Exponentiate), "Exponentiate"},
        SymbolCase{"/", Op(OperatorType::FloatDivide), "FloatDivide"},
        SymbolCase{"//", Op(OperatorType::IntegerDivide), "IntegerDivide"},
        SymbolCase{"%", Op(OperatorType::Modulo), "Modulo"},
        // Bitwise operators
        SymbolCase{"&", Op(OperatorType::BitwiseAnd), "BitwiseAnd"},
        SymbolCase{"|", Op(OperatorType::BitwiseOr), "BitwiseOr"},
        SymbolCase{"^", Op(OperatorType::BitwiseXor), "BitwiseXor"},
        SymbolCase{"~", Op(OperatorType::BitwiseInvert), "BitwiseInvert"},
        SymbolCase{"<<", Op(OperatorType::LeftShift), "LeftShift"},
        SymbolCase{">>", Op(OperatorType::RightShift), "RightShift"},
        // Comparison operators
        SymbolCase{"<", Op(OperatorType::Less), "Less"},
        SymbolCase{">", Op(OperatorType::Greater), "Greater"},
        SymbolCase{"<=", Op(OperatorType::LessOrEqual), "LessOrEqual"},
        SymbolCase{">=", Op(OperatorType::GreaterOrEqual), "GreaterOrEqual"},
        SymbolCase{"==", Op(OperatorType::Equal), "Equal"},
        SymbolCase{"!=", Op(OperatorType::NotEqual), "NotEqual"},
        // Assignment operators
        SymbolCase{"=", Op(OperatorType::Assign), "Assign"},
        SymbolCase{"+=", Op(OperatorType::UnaryAdd), "AddAssign"},
        SymbolCase{"-=", Op(OperatorType::UnarySubtract), "SubtractAssign"},
        SymbolCase{"*=", Op(OperatorType::UnaryMultiply), "MultiplyAssign"},
        SymbolCase{"**=", Op(OperatorType::UnaryExponentiate), "ExponentiateAssign"},
        SymbolCase{"/=", Op(OperatorType::UnaryFloatDivide), "FloatDivideAssign"},
        SymbolCase{"//=", Op(OperatorType::UnaryIntegerDivide), "IntegerDivideAssign"},
        SymbolCase{"%=", Op(OperatorType::UnaryModulo), "ModuloAssign"},
        SymbolCase{"&=", Op(OperatorType::UnaryBitwiseAnd), "BitwiseAndAssign"},
        SymbolCase{"|=", Op(OperatorType::UnaryBitwiseOr), "BitwiseOrAssign"},
        SymbolCase{"^=", Op(OperatorType::UnaryBitwiseXor), "BitwiseXorAssign"},
        SymbolCase{"<<=", Op(OperatorType::UnaryLeftShift), "LeftShiftAssign"},
        SymbolCase{">>=", Op(OperatorType::UnaryRightShift), "RightShiftAssign"},
        // Delimiters
        SymbolCase{"(", Delim(DelimiterType::LeftParenthesis), "LeftParenthesis"},
        SymbolCase{")", Delim(DelimiterType::RightParenthesis), "RightParenthesis"},
        SymbolCase{"[", Delim(DelimiterType::LeftBracket), "LeftBracket"},
        SymbolCase{"]", Delim(DelimiterType::RightBracket), "RightBracket"},
        SymbolCase{"{", Delim(DelimiterType::LeftBraces), "LeftBraces"},
        SymbolCase{"}", Delim(DelimiterType::RightBraces), "RightBraces"},
        SymbolCase{",", Delim(DelimiterType::Comma), "Comma"},
        SymbolCase{":", Delim(DelimiterType::Colon), "Colon"},
        SymbolCase{";", Delim(DelimiterType::Semicolon), "Semicolon"},
        SymbolCase{"!", Delim(DelimiterType::Exclamation), "Exclamation"},
        SymbolCase{"->", Delim(DelimiterType::Arrow), "Arrow"}),
    [](const ::testing::TestParamInfo<SymbolCase> &Info) { return std::string(Info.param.Name); });

TEST(TokenizerSymbols, LongestSymbolWins) {
  expectTokens("a**b", {Id("a"), Op(OperatorType::Exponentiate), Id("b"), Eof()});
  expectTokens("a//b", {Id("a"), Op(OperatorType::IntegerDivide), Id("b"), Eof()});
  expectTokens("a**=b", {Id("a"), Op(OperatorType::UnaryExponentiate), Id("b"), Eof()});
  expectTokens("a//=b", {Id("a"), Op(OperatorType::UnaryIntegerDivide), Id("b"), Eof()});
  expectTokens("a<<=b", {Id("a"), Op(OperatorType::UnaryLeftShift), Id("b"), Eof()});
  expectTokens("a>>=b", {Id("a"), Op(OperatorType::UnaryRightShift), Id("b"), Eof()});
  expectTokens("a->b", {Id("a"), Delim(DelimiterType::Arrow), Id("b"), Eof()});
}

TEST(TokenizerSymbols, LongestSymbolWinsThenContinues) {
  expectTokens("a***b", {Id("a"), Op(OperatorType::Exponentiate), Op(OperatorType::Multiply),
                         Id("b"), Eof()});
  expectTokens("a///b", {Id("a"), Op(OperatorType::IntegerDivide), Op(OperatorType::FloatDivide),
                         Id("b"), Eof()});
  expectTokens("a<<<b",
               {Id("a"), Op(OperatorType::LeftShift), Op(OperatorType::Less), Id("b"), Eof()});
  expectTokens("a===b",
               {Id("a"), Op(OperatorType::Equal), Op(OperatorType::Assign), Id("b"), Eof()});
}

TEST(TokenizerSymbols, AdjacentSymbolsAreSplit) {
  expectTokens("a+-b",
               {Id("a"), Op(OperatorType::Add), Op(OperatorType::Subtract), Id("b"), Eof()});
  expectTokens("a*-b",
               {Id("a"), Op(OperatorType::Multiply), Op(OperatorType::Subtract), Id("b"), Eof()});
  expectTokens("a==-b",
               {Id("a"), Op(OperatorType::Equal), Op(OperatorType::Subtract), Id("b"), Eof()});
  expectTokens(
      "a=~b", {Id("a"), Op(OperatorType::Assign), Op(OperatorType::BitwiseInvert), Id("b"), Eof()});
  expectTokens("a<-b",
               {Id("a"), Op(OperatorType::Less), Op(OperatorType::Subtract), Id("b"), Eof()});
  expectTokens("a>-b",
               {Id("a"), Op(OperatorType::Greater), Op(OperatorType::Subtract), Id("b"), Eof()});
  expectTokens("a--b",
               {Id("a"), Op(OperatorType::Subtract), Op(OperatorType::Subtract), Id("b"), Eof()});
}

TEST(TokenizerSymbols, SeparatedCharactersAreNotCombined) {
  expectTokens("* *", {Op(OperatorType::Multiply), Op(OperatorType::Multiply), Eof()});
  expectTokens("/ /", {Op(OperatorType::FloatDivide), Op(OperatorType::FloatDivide), Eof()});
  expectTokens("< =", {Op(OperatorType::Less), Op(OperatorType::Assign), Eof()});
  expectTokens("= =", {Op(OperatorType::Assign), Op(OperatorType::Assign), Eof()});
  expectTokens("- >", {Op(OperatorType::Subtract), Op(OperatorType::Greater), Eof()});
  expectTokens("! =", {Delim(DelimiterType::Exclamation), Op(OperatorType::Assign), Eof()});
}

TEST(TokenizerSymbols, NestedBrackets) {
  expectTokens("([{}])",
               {Delim(DelimiterType::LeftParenthesis), Delim(DelimiterType::LeftBracket),
                Delim(DelimiterType::LeftBraces), Delim(DelimiterType::RightBraces),
                Delim(DelimiterType::RightBracket), Delim(DelimiterType::RightParenthesis), Eof()});
}

// -------------------------------------------------------------------------------------------------
// Whitespace and comments
// -------------------------------------------------------------------------------------------------

TEST(TokenizerWhitespace, SpacesProduceNoTokens) {
  expectTokens("a b", {Id("a"), Id("b"), Eof()});
  expectTokens("a + b", {Id("a"), Op(OperatorType::Add), Id("b"), Eof()});
}

TEST(TokenizerWhitespace, AnyNumberOfSpacesInsideALine) {
  expectTokens("a  b", {Id("a"), Id("b"), Eof()});
  expectTokens("a   b", {Id("a"), Id("b"), Eof()});
  expectTokens("a    b", {Id("a"), Id("b"), Eof()});
  expectTokens("a         =      1", {Id("a"), Op(OperatorType::Assign), Int(1), Eof()});
}

TEST(TokenizerWhitespace, TrailingSpacesAreIgnored) {
  expectTokens("a   ", {Id("a"), Eof()});
  expectTokens("a   \nb", {Id("a"), NL(), Id("b"), Eof()});
}

TEST(TokenizerWhitespace, SpacesOnlySourceProducesNoTokens) {
  expectTokens(" ", {Eof()});
  expectTokens("    ", {Eof()});
  expectTokens("   \n  \n", {Eof()});
}

TEST(TokenizerWhitespace, NoWhitespaceTokensAreEverEmitted) {
  for (const auto &Token : tokensOf("def f(a: int, b: int) -> int:\n    return a  +  b\n")) {
    EXPECT_NE(Token.Type, TokenType::Whitespace) << toString(Token);
  }
}

TEST(TokenizerComments, CommentOnlySourceProducesNoTokens) {
  expectTokens("# a comment", {Eof()});
  expectTokens("# a comment\n", {Eof()});
  expectTokens("#", {Eof()});
  expectTokens("#\n#\n#\n", {Eof()});
}

TEST(TokenizerComments, TrailingCommentIsIgnored) {
  expectTokens("x # comment", {Id("x"), Eof()});
  expectTokens("x = 1  # comment\n", {Id("x"), Op(OperatorType::Assign), Int(1), NL(), Eof()});
  expectTokens("x = 1# comment\n", {Id("x"), Op(OperatorType::Assign), Int(1), NL(), Eof()});
}

TEST(TokenizerComments, CommentMayContainAnyCharacter) {
  expectTokens("x # $ ? ` \\ ' \" @ . if def 1abc 0b2\n", {Id("x"), NL(), Eof()});
}

TEST(TokenizerComments, CommentEndsAtNewLine) {
  expectTokens("# first\nx\n# second\ny\n", {Id("x"), NL(), Id("y"), NL(), Eof()});
}

TEST(TokenizerComments, CommentLinesBetweenStatementsProduceNoNewLines) {
  expectTokens("a\n# c\n# d\nb\n", {Id("a"), NL(), Id("b"), NL(), Eof()});
}

TEST(TokenizerComments, IndentedCommentLineProducesNoTokens) {
  expectTokens("    # comment\nx\n", {Id("x"), NL(), Eof()});
  expectTokens("  # comment\nx\n", {Id("x"), NL(), Eof()});
}

TEST(TokenizerComments, HashInsideExpressionStartsComment) {
  expectTokens("a + # b\n", {Id("a"), Op(OperatorType::Add), NL(), Eof()});
}

// -------------------------------------------------------------------------------------------------
// New lines
// -------------------------------------------------------------------------------------------------

TEST(TokenizerNewLines, LineEndsWithNewLine) { expectTokens("x\n", {Id("x"), NL(), Eof()}); }

TEST(TokenizerNewLines, NoNewLineWithoutLineBreak) { expectTokens("x", {Id("x"), Eof()}); }

TEST(TokenizerNewLines, OneNewLinePerLogicalLine) {
  expectTokens("a\nb\nc\n", {Id("a"), NL(), Id("b"), NL(), Id("c"), NL(), Eof()});
  expectTokens("a\nb\nc", {Id("a"), NL(), Id("b"), NL(), Id("c"), Eof()});
}

TEST(TokenizerNewLines, NewLinesOnlySourceProducesNoTokens) {
  expectTokens("\n", {Eof()});
  expectTokens("\n\n\n", {Eof()});
}

TEST(TokenizerNewLines, LeadingBlankLinesProduceNoTokens) {
  expectTokens("\n\nx\n", {Id("x"), NL(), Eof()});
}

TEST(TokenizerNewLines, BlankLinesBetweenStatementsProduceNoTokens) {
  expectTokens("a\n\nb\n", {Id("a"), NL(), Id("b"), NL(), Eof()});
  expectTokens("a\n\n\n\nb\n", {Id("a"), NL(), Id("b"), NL(), Eof()});
}

TEST(TokenizerNewLines, TrailingBlankLinesProduceNoTokens) {
  expectTokens("x\n\n\n", {Id("x"), NL(), Eof()});
}

TEST(TokenizerNewLines, WhitespaceOnlyLinesProduceNoTokens) {
  expectTokens("a\n    \nb\n", {Id("a"), NL(), Id("b"), NL(), Eof()});
  expectTokens("a\n  \nb\n", {Id("a"), NL(), Id("b"), NL(), Eof()});
  expectTokens("a\n      \nb\n", {Id("a"), NL(), Id("b"), NL(), Eof()});
}

TEST(TokenizerNewLines, SemicolonsDoNotEndTheLogicalLine) {
  expectTokens("a; b\n", {Id("a"), Delim(DelimiterType::Semicolon), Id("b"), NL(), Eof()});
  expectTokens("a;\n", {Id("a"), Delim(DelimiterType::Semicolon), NL(), Eof()});
}

// -------------------------------------------------------------------------------------------------
// Indentation
// -------------------------------------------------------------------------------------------------

TEST(TokenizerIndentation, IndentedBlock) {
  expectTokens("if a:\n    b\n", {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(),
                                  Indent(4), Id("b"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, IndentIsEmittedOnlyWhenIndentationIncreases) {
  expectTokens("if a:\n    b\n    c\n    d\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Id("c"), NL(), Id("d"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, DedentWhenReturningToPreviousLevel) {
  expectTokens("if a:\n    b\nc\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Dedent(4), Id("c"), NL(), Eof()});
}

TEST(TokenizerIndentation, NestedBlocks) {
  expectTokens("if a:\n    if b:\n        c\n    d\ne\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4),
                Kw(KeywordType::If), Id("b"), Delim(DelimiterType::Colon), NL(), Indent(8), Id("c"),
                NL(), Dedent(8), Id("d"), NL(), Dedent(4), Id("e"), NL(), Eof()});
}

TEST(TokenizerIndentation, OneDedentPerClosedLevel) {
  expectTokens("if a:\n    if b:\n        if c:\n            d\ne\n", {Kw(KeywordType::If),
                                                                       Id("a"),
                                                                       Delim(DelimiterType::Colon),
                                                                       NL(),
                                                                       Indent(4),
                                                                       Kw(KeywordType::If),
                                                                       Id("b"),
                                                                       Delim(DelimiterType::Colon),
                                                                       NL(),
                                                                       Indent(8),
                                                                       Kw(KeywordType::If),
                                                                       Id("c"),
                                                                       Delim(DelimiterType::Colon),
                                                                       NL(),
                                                                       Indent(12),
                                                                       Id("d"),
                                                                       NL(),
                                                                       Dedent(12),
                                                                       Dedent(8),
                                                                       Dedent(4),
                                                                       Id("e"),
                                                                       NL(),
                                                                       Eof()});
}

TEST(TokenizerIndentation, PartialDedent) {
  expectTokens("if a:\n    if b:\n        c\n    d\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4),
                Kw(KeywordType::If), Id("b"), Delim(DelimiterType::Colon), NL(), Indent(8), Id("c"),
                NL(), Dedent(8), Id("d"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, OpenBlocksAreClosedAtEndOfInput) {
  expectTokens("if a:\n    if b:\n        c\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4),
                Kw(KeywordType::If), Id("b"), Delim(DelimiterType::Colon), NL(), Indent(8), Id("c"),
                NL(), Dedent(8), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, OpenBlocksAreClosedWithoutFinalNewLine) {
  expectTokens("if a:\n    b", {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(),
                                Indent(4), Id("b"), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, OpenBlocksAreClosedBeforeTrailingBlankLines) {
  expectTokens("if a:\n    b\n\n\n", {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon),
                                      NL(), Indent(4), Id("b"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, SiblingBlocks) {
  expectTokens("if a:\n    b\nelse:\n    c\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Dedent(4), Kw(KeywordType::Else), Delim(DelimiterType::Colon), NL(),
                Indent(4), Id("c"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, BlankLinesInsideBlockDoNotAffectIndentation) {
  expectTokens("if a:\n    b\n\n    c\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Id("c"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, WhitespaceOnlyLinesInsideBlockDoNotAffectIndentation) {
  expectTokens("if a:\n    b\n  \n    c\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Id("c"), NL(), Dedent(4), Eof()});
  expectTokens("if a:\n    b\n            \n    c\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Id("c"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, CommentLinesInsideBlockDoNotAffectIndentation) {
  expectTokens("if a:\n    b\n# c\n    d\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Id("d"), NL(), Dedent(4), Eof()});
  expectTokens("if a:\n    b\n        # c\n    d\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                NL(), Id("d"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, CommentAfterColonDoesNotAffectIndentation) {
  expectTokens("if a:  # c\n    b\n", {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon),
                                       NL(), Indent(4), Id("b"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, BlockOnSameLineAsColonHasNoIndent) {
  expectTokens("if a: b\nc\n", {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), Id("b"),
                                NL(), Id("c"), NL(), Eof()});
}

TEST(TokenizerIndentation, SpacesAfterFirstTokenAreNotIndentation) {
  expectTokens("a    = 1\n", {Id("a"), Op(OperatorType::Assign), Int(1), NL(), Eof()});
  expectTokens("if a:\n    b        = 1\n",
               {Kw(KeywordType::If), Id("a"), Delim(DelimiterType::Colon), NL(), Indent(4), Id("b"),
                Op(OperatorType::Assign), Int(1), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, IndentedFirstLine) {
  expectTokens("    x\n", {Indent(4), Id("x"), NL(), Dedent(4), Eof()});
}

TEST(TokenizerIndentation, IndentingMoreThanOneLevelAtOnceIsAnError) {
  expectError("if a:\n        b\n");
  expectError("if a:\n    if b:\n                c\n");
}

// -------------------------------------------------------------------------------------------------
// Source locations
// -------------------------------------------------------------------------------------------------

TEST(TokenizerLocations, FirstTokenIsAtOrigin) {
  const auto Tokens = tokensOf("x");
  ASSERT_GE(Tokens.size(), 1u);
  expectLocation(Tokens[0], 0, 0);
}

TEST(TokenizerLocations, ColumnAdvancesByTokenLength) {
  const auto Tokens = tokensOf("abc+de");
  ASSERT_GE(Tokens.size(), 3u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[1], 0, 3);
  expectLocation(Tokens[2], 0, 4);
}

TEST(TokenizerLocations, ColumnAdvancesByMultiCharacterSymbolLength) {
  const auto Tokens = tokensOf("a**=b->c");
  ASSERT_GE(Tokens.size(), 5u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[1], 0, 1);
  expectLocation(Tokens[2], 0, 4);
  expectLocation(Tokens[3], 0, 5);
  expectLocation(Tokens[4], 0, 7);
}

TEST(TokenizerLocations, ColumnAdvancesByNumberLength) {
  const auto Tokens = tokensOf("123,4.5,1e7,x");
  ASSERT_GE(Tokens.size(), 7u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[1], 0, 3);
  expectLocation(Tokens[2], 0, 4);
  expectLocation(Tokens[3], 0, 7);
  expectLocation(Tokens[4], 0, 8);
  expectLocation(Tokens[5], 0, 11);
  expectLocation(Tokens[6], 0, 12);
}

TEST(TokenizerLocations, ColumnAdvancesByReservedWordLength) {
  const auto Tokens = tokensOf("not True and int");
  ASSERT_GE(Tokens.size(), 4u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[1], 0, 4);
  expectLocation(Tokens[2], 0, 9);
  expectLocation(Tokens[3], 0, 13);
}

TEST(TokenizerLocations, SpacesAdvanceColumn) {
  const auto Tokens = tokensOf("a b   c");
  ASSERT_GE(Tokens.size(), 3u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[1], 0, 2);
  expectLocation(Tokens[2], 0, 6);
}

TEST(TokenizerLocations, NewLineIsAtEndOfLine) {
  const auto Tokens = tokensOf("ab\ncd\n");
  ASSERT_GE(Tokens.size(), 2u);
  ASSERT_EQ(Tokens[1].Type, TokenType::NewLine) << toString(Tokens[1]);
  expectLocation(Tokens[1], 0, 2);
}

TEST(TokenizerLocations, NewLineResetsColumnAndAdvancesLine) {
  const auto Tokens = tokensOf("ab\ncd");
  ASSERT_GE(Tokens.size(), 3u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[2], 1, 0);
}

TEST(TokenizerLocations, LinesAreCountedAcrossManyLines) {
  const auto Tokens = tokensOf("a\nb\nc\nd\n");
  ASSERT_GE(Tokens.size(), 7u);
  expectLocation(Tokens[0], 0, 0);
  expectLocation(Tokens[2], 1, 0);
  expectLocation(Tokens[4], 2, 0);
  expectLocation(Tokens[6], 3, 0);
}

TEST(TokenizerLocations, BlankAndCommentLinesAreCounted) {
  const auto Tokens = tokensOf("\n# comment\n\n   \nx\n");
  ASSERT_GE(Tokens.size(), 1u);
  expectLocation(Tokens[0], 4, 0);
}

TEST(TokenizerLocations, IndentationAdvancesColumn) {
  // if x : NL INDENT y NL z DEDENT
  const auto Tokens = tokensOf("if x:\n    y\n        \n    z");
  ASSERT_GE(Tokens.size(), 8u);
  ASSERT_TRUE(matches(Tokens[5], Id("y"))) << toString(Tokens[5]);
  expectLocation(Tokens[5], 1, 4);
  ASSERT_TRUE(matches(Tokens[7], Id("z"))) << toString(Tokens[7]);
  expectLocation(Tokens[7], 3, 4);
}

TEST(TokenizerLocations, TokenAfterTrailingCommentIsOnNextLine) {
  const auto Tokens = tokensOf("a # comment\nb");
  ASSERT_GE(Tokens.size(), 3u);
  expectLocation(Tokens[2], 1, 0);
}

// -------------------------------------------------------------------------------------------------
// Errors
// -------------------------------------------------------------------------------------------------

TEST(TokenizerErrors, UnsupportedCharacters) {
  expectError("$");
  expectError("?");
  expectError("`");
  expectError("\\");
  expectError("@");
}

TEST(TokenizerErrors, UnsupportedCharacterAfterValidTokens) {
  expectError("a = $b");
  expectError("x = 1 ? 2");
  expectError("a\nb\n$\n");
}

TEST(TokenizerErrors, UnsupportedCharacterInsideBlock) { expectError("if a:\n    $\n"); }

TEST(TokenizerErrors, StringsAreUnsupported) {
  expectError("\"text\"");
  expectError("'text'");
}

TEST(TokenizerErrors, AttributeAccessIsUnsupported) {
  expectError("a.b");
  expectError(".");
}

TEST(TokenizerErrors, TabsAreUnsupported) {
  expectError("\tx");
  expectError("if a:\n\tb\n");
}

TEST(TokenizerErrors, ErrorMessageIsNotEmpty) {
  try {
    tokenize("$");
    FAIL() << "expected an exception";
  } catch (const error::CompyledException &E) {
    EXPECT_STRNE(E.what(), "");
  }
}

// -------------------------------------------------------------------------------------------------
// Programs
// -------------------------------------------------------------------------------------------------

TEST(TokenizerPrograms, VariableDeclarations) {
  expectTokens("x: int = 5\n", {Id("x"), Delim(DelimiterType::Colon), Base(BaseType::Integer),
                                Op(OperatorType::Assign), Int(5), NL(), Eof()});
  expectTokens("y: float = 2.5\n", {Id("y"), Delim(DelimiterType::Colon), Base(BaseType::Float),
                                    Op(OperatorType::Assign), Flt(2.5), NL(), Eof()});
  expectTokens("z: bool = False\n", {Id("z"), Delim(DelimiterType::Colon), Base(BaseType::Boolean),
                                     Op(OperatorType::Assign), Bool(false), NL(), Eof()});
}

TEST(TokenizerPrograms, Assignments) {
  expectTokens("x = 5\n", {Id("x"), Op(OperatorType::Assign), Int(5), NL(), Eof()});
  expectTokens("a = b = 0\n", {Id("a"), Op(OperatorType::Assign), Id("b"), Op(OperatorType::Assign),
                               Int(0), NL(), Eof()});
  expectTokens("count += 1\n", {Id("count"), Op(OperatorType::UnaryAdd), Int(1), NL(), Eof()});
  expectTokens("mask <<= 2\n", {Id("mask"), Op(OperatorType::UnaryLeftShift), Int(2), NL(), Eof()});
}

TEST(TokenizerPrograms, ArithmeticExpression) {
  expectTokens("(a + b) * c - d / e // f % g ** -2\n", {Delim(DelimiterType::LeftParenthesis),
                                                        Id("a"),
                                                        Op(OperatorType::Add),
                                                        Id("b"),
                                                        Delim(DelimiterType::RightParenthesis),
                                                        Op(OperatorType::Multiply),
                                                        Id("c"),
                                                        Op(OperatorType::Subtract),
                                                        Id("d"),
                                                        Op(OperatorType::FloatDivide),
                                                        Id("e"),
                                                        Op(OperatorType::IntegerDivide),
                                                        Id("f"),
                                                        Op(OperatorType::Modulo),
                                                        Id("g"),
                                                        Op(OperatorType::Exponentiate),
                                                        Op(OperatorType::Subtract),
                                                        Int(2),
                                                        NL(),
                                                        Eof()});
}

TEST(TokenizerPrograms, BitwiseExpression) {
  expectTokens("~a & b | c ^ d << 2 >> 1\n",
               {Op(OperatorType::BitwiseInvert), Id("a"), Op(OperatorType::BitwiseAnd), Id("b"),
                Op(OperatorType::BitwiseOr), Id("c"), Op(OperatorType::BitwiseXor), Id("d"),
                Op(OperatorType::LeftShift), Int(2), Op(OperatorType::RightShift), Int(1), NL(),
                Eof()});
}

TEST(TokenizerPrograms, ComparisonChain) {
  expectTokens("0 <= x < 10 != y >= 3 > z == 1\n",
               {Int(0), Op(OperatorType::LessOrEqual), Id("x"), Op(OperatorType::Less), Int(10),
                Op(OperatorType::NotEqual), Id("y"), Op(OperatorType::GreaterOrEqual), Int(3),
                Op(OperatorType::Greater), Id("z"), Op(OperatorType::Equal), Int(1), NL(), Eof()});
}

TEST(TokenizerPrograms, LogicalExpression) {
  expectTokens("not a and b or not c\n",
               {Op(OperatorType::LogicalNot), Id("a"), Op(OperatorType::LogicalAnd), Id("b"),
                Op(OperatorType::LogicalOr), Op(OperatorType::LogicalNot), Id("c"), NL(), Eof()});
}

TEST(TokenizerPrograms, ConditionalExpression) {
  expectTokens("x = a if c else b\n",
               {Id("x"), Op(OperatorType::Assign), Id("a"), Kw(KeywordType::If), Id("c"),
                Kw(KeywordType::Else), Id("b"), NL(), Eof()});
}

TEST(TokenizerPrograms, FunctionCall) {
  expectTokens("f(a, 1, 2.5, True, g(x),)\n",
               {Id("f"), Delim(DelimiterType::LeftParenthesis), Id("a"),
                Delim(DelimiterType::Comma), Int(1), Delim(DelimiterType::Comma), Flt(2.5),
                Delim(DelimiterType::Comma), Bool(true), Delim(DelimiterType::Comma), Id("g"),
                Delim(DelimiterType::LeftParenthesis), Id("x"),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::Comma),
                Delim(DelimiterType::RightParenthesis), NL(), Eof()});
}

TEST(TokenizerPrograms, MultipleStatementsOnOneLine) {
  expectTokens("a = 1; b = 2; c = 3;\n",
               {Id("a"), Op(OperatorType::Assign), Int(1), Delim(DelimiterType::Semicolon), Id("b"),
                Op(OperatorType::Assign), Int(2), Delim(DelimiterType::Semicolon), Id("c"),
                Op(OperatorType::Assign), Int(3), Delim(DelimiterType::Semicolon), NL(), Eof()});
}

TEST(TokenizerPrograms, GlobalStatement) {
  expectTokens("global a, b\n", {Kw(KeywordType::Global), Id("a"), Delim(DelimiterType::Comma),
                                 Id("b"), NL(), Eof()});
}

TEST(TokenizerPrograms, FunctionDefinition) {
  expectTokens("def add(a: int, b: int = 1) -> int:\n"
               "    return a + b\n",
               {Kw(KeywordType::Def),
                Id("add"),
                Delim(DelimiterType::LeftParenthesis),
                Id("a"),
                Delim(DelimiterType::Colon),
                Base(BaseType::Integer),
                Delim(DelimiterType::Comma),
                Id("b"),
                Delim(DelimiterType::Colon),
                Base(BaseType::Integer),
                Op(OperatorType::Assign),
                Int(1),
                Delim(DelimiterType::RightParenthesis),
                Delim(DelimiterType::Arrow),
                Base(BaseType::Integer),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Kw(KeywordType::Return),
                Id("a"),
                Op(OperatorType::Add),
                Id("b"),
                NL(),
                Dedent(4),
                Eof()});
}

TEST(TokenizerPrograms, FunctionReturningNone) {
  expectTokens("def f() -> None:\n"
               "    pass\n",
               {Kw(KeywordType::Def), Id("f"), Delim(DelimiterType::LeftParenthesis),
                Delim(DelimiterType::RightParenthesis), Delim(DelimiterType::Arrow),
                Kw(KeywordType::None), Delim(DelimiterType::Colon), NL(), Indent(4),
                Kw(KeywordType::Pass), NL(), Dedent(4), Eof()});
}

TEST(TokenizerPrograms, IfElifElse) {
  expectTokens("if a < 0:\n"
               "    x = -1\n"
               "elif a == 0:\n"
               "    x = 0\n"
               "else:\n"
               "    x = 1\n",
               {Kw(KeywordType::If),
                Id("a"),
                Op(OperatorType::Less),
                Int(0),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Id("x"),
                Op(OperatorType::Assign),
                Op(OperatorType::Subtract),
                Int(1),
                NL(),
                Dedent(4),
                Kw(KeywordType::Elif),
                Id("a"),
                Op(OperatorType::Equal),
                Int(0),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Id("x"),
                Op(OperatorType::Assign),
                Int(0),
                NL(),
                Dedent(4),
                Kw(KeywordType::Else),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Id("x"),
                Op(OperatorType::Assign),
                Int(1),
                NL(),
                Dedent(4),
                Eof()});
}

TEST(TokenizerPrograms, WhileElseWithBreakAndContinue) {
  expectTokens("while True:\n"
               "    if a:\n"
               "        break\n"
               "    continue\n"
               "else:\n"
               "    pass\n",
               {Kw(KeywordType::While),
                Bool(true),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Kw(KeywordType::If),
                Id("a"),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(8),
                Kw(KeywordType::Break),
                NL(),
                Dedent(8),
                Kw(KeywordType::Continue),
                NL(),
                Dedent(4),
                Kw(KeywordType::Else),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Kw(KeywordType::Pass),
                NL(),
                Dedent(4),
                Eof()});
}

TEST(TokenizerPrograms, ForOverRange) {
  expectTokens("for i in range(0, 10, 2):\n"
               "    total += i\n",
               {Kw(KeywordType::For),
                Id("i"),
                Kw(KeywordType::In),
                Kw(KeywordType::Range),
                Delim(DelimiterType::LeftParenthesis),
                Int(0),
                Delim(DelimiterType::Comma),
                Int(10),
                Delim(DelimiterType::Comma),
                Int(2),
                Delim(DelimiterType::RightParenthesis),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Id("total"),
                Op(OperatorType::UnaryAdd),
                Id("i"),
                NL(),
                Dedent(4),
                Eof()});
}

TEST(TokenizerPrograms, RecursiveFunction) {
  expectTokens("def fib(n: int) -> int:\n"
               "    if n < 2:\n"
               "        return n\n"
               "    return fib(n - 1) + fib(n - 2)\n",
               {Kw(KeywordType::Def),
                Id("fib"),
                Delim(DelimiterType::LeftParenthesis),
                Id("n"),
                Delim(DelimiterType::Colon),
                Base(BaseType::Integer),
                Delim(DelimiterType::RightParenthesis),
                Delim(DelimiterType::Arrow),
                Base(BaseType::Integer),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Kw(KeywordType::If),
                Id("n"),
                Op(OperatorType::Less),
                Int(2),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(8),
                Kw(KeywordType::Return),
                Id("n"),
                NL(),
                Dedent(8),
                Kw(KeywordType::Return),
                Id("fib"),
                Delim(DelimiterType::LeftParenthesis),
                Id("n"),
                Op(OperatorType::Subtract),
                Int(1),
                Delim(DelimiterType::RightParenthesis),
                Op(OperatorType::Add),
                Id("fib"),
                Delim(DelimiterType::LeftParenthesis),
                Id("n"),
                Op(OperatorType::Subtract),
                Int(2),
                Delim(DelimiterType::RightParenthesis),
                NL(),
                Dedent(4),
                Eof()});
}

TEST(TokenizerPrograms, ModuleWithGlobalsFunctionsCommentsAndBlankLines) {
  expectTokens("# Counter module\n"
               "counter: int = 0\n"
               "\n"
               "\n"
               "def bump(step: int = 1) -> None:\n"
               "    global counter  # module-level\n"
               "\n"
               "    counter += step\n"
               "\n"
               "\n"
               "bump()\n",
               {Id("counter"),
                Delim(DelimiterType::Colon),
                Base(BaseType::Integer),
                Op(OperatorType::Assign),
                Int(0),
                NL(),
                Kw(KeywordType::Def),
                Id("bump"),
                Delim(DelimiterType::LeftParenthesis),
                Id("step"),
                Delim(DelimiterType::Colon),
                Base(BaseType::Integer),
                Op(OperatorType::Assign),
                Int(1),
                Delim(DelimiterType::RightParenthesis),
                Delim(DelimiterType::Arrow),
                Kw(KeywordType::None),
                Delim(DelimiterType::Colon),
                NL(),
                Indent(4),
                Kw(KeywordType::Global),
                Id("counter"),
                NL(),
                Id("counter"),
                Op(OperatorType::UnaryAdd),
                Id("step"),
                NL(),
                Dedent(4),
                Id("bump"),
                Delim(DelimiterType::LeftParenthesis),
                Delim(DelimiterType::RightParenthesis),
                NL(),
                Eof()});
}

// -------------------------------------------------------------------------------------------------
// detail::getIntegerLiteral
// -------------------------------------------------------------------------------------------------

TEST(DetailGetIntegerLiteral, ParsesWholeInput) {
  const auto R = detail::getIntegerLiteral("42");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(R->first, 42);
  EXPECT_EQ(R->second, 2u);
}

TEST(DetailGetIntegerLiteral, ParsesPrefixAndReportsLength) {
  const auto R = detail::getIntegerLiteral("123 rest");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(R->first, 123);
  EXPECT_EQ(R->second, 3u);
}

TEST(DetailGetIntegerLiteral, CountsUnderscoresInLength) {
  const auto R = detail::getIntegerLiteral("1_000)");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(R->first, 1000);
  EXPECT_EQ(R->second, 5u);
}

TEST(DetailGetIntegerLiteral, ParsesPrefixedBases) {
  const auto Hex = detail::getIntegerLiteral("0xff");
  ASSERT_TRUE(Hex.has_value());
  EXPECT_EQ(Hex->first, 255);
  EXPECT_EQ(Hex->second, 4u);

  const auto Oct = detail::getIntegerLiteral("0o17");
  ASSERT_TRUE(Oct.has_value());
  EXPECT_EQ(Oct->first, 15);
  EXPECT_EQ(Oct->second, 4u);

  const auto Bin = detail::getIntegerLiteral("0b101");
  ASSERT_TRUE(Bin.has_value());
  EXPECT_EQ(Bin->first, 5);
  EXPECT_EQ(Bin->second, 5u);
}

TEST(DetailGetIntegerLiteral, RejectsNonIntegers) {
  EXPECT_FALSE(detail::getIntegerLiteral("").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral("abc").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral("_1").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral("+1").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral("-1").has_value());
  EXPECT_FALSE(detail::getIntegerLiteral(".5").has_value());
}

// -------------------------------------------------------------------------------------------------
// detail::getFloatLiteral
// -------------------------------------------------------------------------------------------------

TEST(DetailGetFloatLiteral, ParsesPrefixAndReportsLength) {
  const auto R = detail::getFloatLiteral("3.14 rest");
  ASSERT_TRUE(R.has_value());
  EXPECT_DOUBLE_EQ(R->first, 3.14);
  EXPECT_EQ(R->second, 4u);
}

TEST(DetailGetFloatLiteral, ParsesExponent) {
  const auto R = detail::getFloatLiteral("1.5e2)");
  ASSERT_TRUE(R.has_value());
  EXPECT_DOUBLE_EQ(R->first, 150.0);
  EXPECT_EQ(R->second, 5u);
}

TEST(DetailGetFloatLiteral, ParsesEveryForm) {
  const auto NoIntegerPart = detail::getFloatLiteral(".5");
  ASSERT_TRUE(NoIntegerPart.has_value());
  EXPECT_DOUBLE_EQ(NoIntegerPart->first, 0.5);
  EXPECT_EQ(NoIntegerPart->second, 2u);

  const auto NoFractionPart = detail::getFloatLiteral("5.");
  ASSERT_TRUE(NoFractionPart.has_value());
  EXPECT_DOUBLE_EQ(NoFractionPart->first, 5.0);
  EXPECT_EQ(NoFractionPart->second, 2u);

  const auto ExponentOnly = detail::getFloatLiteral("2e3");
  ASSERT_TRUE(ExponentOnly.has_value());
  EXPECT_DOUBLE_EQ(ExponentOnly->first, 2000.0);
  EXPECT_EQ(ExponentOnly->second, 3u);
}

TEST(DetailGetFloatLiteral, CountsUnderscoresInLength) {
  const auto R = detail::getFloatLiteral("1_000.5_0,");
  ASSERT_TRUE(R.has_value());
  EXPECT_DOUBLE_EQ(R->first, 1000.5);
  EXPECT_EQ(R->second, 9u);
}

TEST(DetailGetFloatLiteral, RejectsIntegers) {
  EXPECT_FALSE(detail::getFloatLiteral("42").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("42 ").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("1_000").has_value());
}

TEST(DetailGetFloatLiteral, RejectsNonNumbers) {
  EXPECT_FALSE(detail::getFloatLiteral("").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("abc").has_value());
  EXPECT_FALSE(detail::getFloatLiteral(".").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("(").has_value());
  EXPECT_FALSE(detail::getFloatLiteral("e5").has_value());
}

// -------------------------------------------------------------------------------------------------
// detail::getOperatorOrDelimiterType
// -------------------------------------------------------------------------------------------------

TEST(DetailGetOperatorOrDelimiterType, ReturnsOperatorAndLength) {
  const auto R = detail::getOperatorOrDelimiterType("**= 2");
  ASSERT_TRUE(R.has_value());
  ASSERT_TRUE(std::holds_alternative<OperatorType>(R->first));
  EXPECT_EQ(std::get<OperatorType>(R->first), OperatorType::UnaryExponentiate);
  EXPECT_EQ(R->second, 3u);
}

TEST(DetailGetOperatorOrDelimiterType, ReturnsDelimiterAndLength) {
  const auto R = detail::getOperatorOrDelimiterType("->x");
  ASSERT_TRUE(R.has_value());
  ASSERT_TRUE(std::holds_alternative<DelimiterType>(R->first));
  EXPECT_EQ(std::get<DelimiterType>(R->first), DelimiterType::Arrow);
  EXPECT_EQ(R->second, 2u);
}

TEST(DetailGetOperatorOrDelimiterType, SingleCharacterAtEndOfInput) {
  const auto R = detail::getOperatorOrDelimiterType("<");
  ASSERT_TRUE(R.has_value());
  ASSERT_TRUE(std::holds_alternative<OperatorType>(R->first));
  EXPECT_EQ(std::get<OperatorType>(R->first), OperatorType::Less);
  EXPECT_EQ(R->second, 1u);
}

TEST(DetailGetOperatorOrDelimiterType, SingleCharacterBeforeOtherInput) {
  const auto R = detail::getOperatorOrDelimiterType("*x");
  ASSERT_TRUE(R.has_value());
  ASSERT_TRUE(std::holds_alternative<OperatorType>(R->first));
  EXPECT_EQ(std::get<OperatorType>(R->first), OperatorType::Multiply);
  EXPECT_EQ(R->second, 1u);
}

TEST(DetailGetOperatorOrDelimiterType, RejectsNonSymbols) {
  EXPECT_FALSE(detail::getOperatorOrDelimiterType("a").has_value());
  EXPECT_FALSE(detail::getOperatorOrDelimiterType("1").has_value());
  EXPECT_FALSE(detail::getOperatorOrDelimiterType("$").has_value());
  EXPECT_FALSE(detail::getOperatorOrDelimiterType(" +").has_value());
}

// -------------------------------------------------------------------------------------------------
// detail::getName
// -------------------------------------------------------------------------------------------------

TEST(DetailGetName, ParsesWholeInput) {
  const auto R = detail::getName("foo");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(*R, "foo");
}

TEST(DetailGetName, StopsAtNonNameCharacter) {
  const auto Space = detail::getName("foo_bar1 baz");
  ASSERT_TRUE(Space.has_value());
  EXPECT_EQ(*Space, "foo_bar1");

  const auto Paren = detail::getName("abc(def)");
  ASSERT_TRUE(Paren.has_value());
  EXPECT_EQ(*Paren, "abc");
}

TEST(DetailGetName, AcceptsLeadingUnderscore) {
  const auto R = detail::getName("_x1");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(*R, "_x1");
}

TEST(DetailGetName, ReturnsReservedWordsVerbatim) {
  const auto R = detail::getName("while x");
  ASSERT_TRUE(R.has_value());
  EXPECT_EQ(*R, "while");
}

TEST(DetailGetName, RejectsNonNames) {
  EXPECT_FALSE(detail::getName("1abc").has_value());
  EXPECT_FALSE(detail::getName("+abc").has_value());
  EXPECT_FALSE(detail::getName(" abc").has_value());
}
