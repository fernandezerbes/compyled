#pragma once

// Helpers shared by the frontend tests (tokenizer and parser).

#include "frontend/Tokenizer.h"
#include "frontend/Types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace compyled::test {

// Upper bound on the number of tokens consumed from a single source. Protects the tests against a
// tokenizer that never reaches Eof.
constexpr std::size_t MAX_TOKENS = 10'000;

// The parts of a token a test cares about: its type and value. Locations are checked separately.
struct Expected {
  lexer::TokenType Type;
  lexer::Token::ValueType Value{};
};

Expected Id(std::string Name);
Expected Kw(lexer::KeywordType K);
Expected Op(lexer::OperatorType O);
Expected Delim(lexer::DelimiterType D);
Expected Base(lexer::BaseType B);
Expected Int(types::IntegerType V);
Expected Flt(types::FloatType V);
Expected Bool(types::BooleanType V);
Expected NL();
Expected Indent(std::size_t S);
Expected Dedent(std::size_t S);
Expected Eof();

// True if `Actual` has the type and value of `Want`.
bool matches(const lexer::Token &Actual, const Expected &Want);

std::string toString(lexer::TokenType Type);
std::string toString(const lexer::Token::ValueType &Value);
std::string toString(const Expected &E);
std::string toString(const lexer::Token &T);

// Consumes every token of `Source`, including the final Eof token.
// Exceptions thrown by the tokenizer propagate to the caller.
std::vector<lexer::Token> tokenize(std::string Source);

} // namespace compyled::test
