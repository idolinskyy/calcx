// Токени лексера з позицією в джерелі.
#pragma once

#include "calc/error.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace calc {

/// Вид токена.
enum class TokenKind {
  End,
  Identifier,
  IntegerLit,
  RationalLit,
  RealLit,
  ImaginaryLit,
  String,
  Let,
  Print,
  If,
  Else,
  While,
  For,
  In,  ///< for (x in xs)
  Break,
  Continue,
  Fun,
  Return,
  True,
  False,
  And,
  Or,
  Not,
  Plus,
  Minus,
  Star,
  StarStar, ///< **
  Slash,
  Percent, ///< %
  Equal,   ///< = (let / присвоєння)
  EqEq,    ///< ==
  NotEq,   ///< !=
  Less,
  LessEq,
  Greater,
  GreaterEq,
  Comma,
  Semicolon,
  LParen,
  RParen,
  LBrace,
  RBrace,
  LBracket,  ///< [
  RBracket,  ///< ]
  Dot,       ///< .
  Colon,     ///< :
};

struct Token {
  TokenKind kind{TokenKind::End};
  std::string_view text;
  SourcePos pos{};
  long double number{};
  std::int64_t rat_num{0};
  std::int64_t rat_den{1};
  std::string string_value;
};

struct LexResult {
  std::vector<Token> tokens;
  Diagnostic error;
};

} // namespace calc
