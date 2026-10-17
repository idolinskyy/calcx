// Строгі тести лексера: види токенів, позиції, літерали, помилки.
#include "calc/lexer.hpp"

#include <doctest/doctest.h>

using calc::ErrorCode;
using calc::tokenize;
using calc::TokenKind;

TEST_CASE("keywords vs identifiers") {
  auto r = tokenize("let letter print printer if else while for in break "
                    "continue fun return");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 14);
  CHECK(r.tokens[0].kind == TokenKind::Let);
  CHECK(r.tokens[1].kind == TokenKind::Identifier);
  CHECK(r.tokens[1].text == "letter");
  CHECK(r.tokens[2].kind == TokenKind::Print);
  CHECK(r.tokens[3].kind == TokenKind::Identifier);
  CHECK(r.tokens[3].text == "printer");
  CHECK(r.tokens[4].kind == TokenKind::If);
  CHECK(r.tokens[5].kind == TokenKind::Else);
  CHECK(r.tokens[6].kind == TokenKind::While);
  CHECK(r.tokens[7].kind == TokenKind::For);
  CHECK(r.tokens[8].kind == TokenKind::In);
  CHECK(r.tokens[9].kind == TokenKind::Break);
  CHECK(r.tokens[10].kind == TokenKind::Continue);
  CHECK(r.tokens[11].kind == TokenKind::Fun);
  CHECK(r.tokens[12].kind == TokenKind::Return);
  CHECK(r.tokens[13].kind == TokenKind::End);
}

TEST_CASE("braces are tokens") {
  auto r = tokenize("{ }");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 3);
  CHECK(r.tokens[0].kind == TokenKind::LBrace);
  CHECK(r.tokens[1].kind == TokenKind::RBrace);
  CHECK(r.tokens[2].kind == TokenKind::End);
}

TEST_CASE("equals followed by minus are separate tokens") {
  auto r = tokenize("neg =-1.23");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 5);
  CHECK(r.tokens[0].kind == TokenKind::Identifier);
  CHECK(r.tokens[1].kind == TokenKind::Equal);
  CHECK(r.tokens[2].kind == TokenKind::Minus);
  CHECK(r.tokens[3].kind == TokenKind::RealLit);
  CHECK(r.tokens[3].number == doctest::Approx(1.23).epsilon(1e-12));
  CHECK(r.tokens[4].kind == TokenKind::End);
}

TEST_CASE("imaginary suffix attaches to number") {
  auto r = tokenize("2i 2.5i 2e1i");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 4);
  CHECK(r.tokens[0].kind == TokenKind::ImaginaryLit);
  CHECK(r.tokens[0].number == doctest::Approx(2.0).epsilon(1e-12));
  CHECK(r.tokens[1].kind == TokenKind::ImaginaryLit);
  CHECK(r.tokens[1].number == doctest::Approx(2.5).epsilon(1e-12));
  CHECK(r.tokens[2].kind == TokenKind::ImaginaryLit);
  CHECK(r.tokens[2].number == doctest::Approx(20.0).epsilon(1e-12));
}

TEST_CASE("bare i is identifier; spaced 2 i are two tokens") {
  auto a = tokenize("i");
  REQUIRE(a.error.code == ErrorCode::Ok);
  CHECK(a.tokens[0].kind == TokenKind::Identifier);

  auto b = tokenize("2 i");
  REQUIRE(b.error.code == ErrorCode::Ok);
  CHECK(b.tokens[0].kind == TokenKind::IntegerLit);
  CHECK(b.tokens[1].kind == TokenKind::Identifier);
}

TEST_CASE("real literals with dot or exponent") {
  auto r = tokenize("2.0 .5 2e1");
  REQUIRE(r.error.code == ErrorCode::Ok);
  CHECK(r.tokens[0].kind == TokenKind::RealLit);
  CHECK(r.tokens[1].kind == TokenKind::RealLit);
  CHECK(r.tokens[1].number == doctest::Approx(0.5).epsilon(1e-12));
  CHECK(r.tokens[2].kind == TokenKind::RealLit);
  CHECK(r.tokens[2].number == doctest::Approx(20.0).epsilon(1e-12));
}

TEST_CASE("sample program token kinds and positions") {
  const char *src = "let a = 12 * (3 + 2); let compl = 2 + 2i, neg =-1.23;";
  auto r = tokenize(src);
  REQUIRE(r.error.code == ErrorCode::Ok);

  CHECK(r.tokens[0].kind == TokenKind::Let);
  CHECK(r.tokens[0].pos.line == 1);
  CHECK(r.tokens[0].pos.column == 1);

  // "a" at column 5
  CHECK(r.tokens[1].kind == TokenKind::Identifier);
  CHECK(r.tokens[1].text == "a");
  CHECK(r.tokens[1].pos.column == 5);

  CHECK(r.tokens[2].kind == TokenKind::Equal);
  CHECK(r.tokens[3].kind == TokenKind::IntegerLit);
  CHECK(r.tokens[3].number == 12);

  // Find Imaginary 2i
  bool found_imag = false;
  for (const auto &t : r.tokens) {
    if (t.kind == TokenKind::ImaginaryLit && t.number == 2) {
      found_imag = true;
      break;
    }
  }
  CHECK(found_imag);

  // Find Real 1.23 after Minus after Equal for neg
  bool found_neg_pattern = false;
  for (std::size_t i = 0; i + 2 < r.tokens.size(); ++i) {
    if (r.tokens[i].kind == TokenKind::Equal &&
        r.tokens[i + 1].kind == TokenKind::Minus &&
        r.tokens[i + 2].kind == TokenKind::RealLit) {
      CHECK(r.tokens[i + 2].number == doctest::Approx(1.23).epsilon(1e-12));
      found_neg_pattern = true;
    }
  }
  CHECK(found_neg_pattern);
}

TEST_CASE("unterminated string and invalid escape") {
  auto u = tokenize("print(\"hi");
  REQUIRE(u.error.code == ErrorCode::UnterminatedString);
  CHECK(u.error.pos.column == 7);

  auto e = tokenize("print(\"\\q\")");
  REQUIRE(e.error.code == ErrorCode::InvalidEscape);
}

TEST_CASE("invalid number") {
  auto r = tokenize("1e");
  REQUIRE(r.error.code == ErrorCode::InvalidNumber);

  auto m = tokenize("12abc");
  REQUIRE(m.error.code == ErrorCode::InvalidNumber);
}

TEST_CASE("glued integer slash integer is RationalLit") {
  auto r = tokenize("1/2 3/4");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 3);
  CHECK(r.tokens[0].kind == TokenKind::RationalLit);
  CHECK(r.tokens[0].rat_num == 1);
  CHECK(r.tokens[0].rat_den == 2);
  CHECK(r.tokens[0].text == "1/2");
  CHECK(r.tokens[1].kind == TokenKind::RationalLit);
  CHECK(r.tokens[1].rat_num == 3);
  CHECK(r.tokens[1].rat_den == 4);
}

TEST_CASE("spaced slash stays Integer Slash Integer") {
  auto r = tokenize("1 / 2");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 4);
  CHECK(r.tokens[0].kind == TokenKind::IntegerLit);
  CHECK(r.tokens[1].kind == TokenKind::Slash);
  CHECK(r.tokens[2].kind == TokenKind::IntegerLit);
}

TEST_CASE("1/2i is Integer Slash Imaginary not Rational") {
  auto r = tokenize("1/2i");
  REQUIRE(r.error.code == ErrorCode::Ok);
  REQUIRE(r.tokens.size() == 4);
  CHECK(r.tokens[0].kind == TokenKind::IntegerLit);
  CHECK(r.tokens[1].kind == TokenKind::Slash);
  CHECK(r.tokens[2].kind == TokenKind::ImaginaryLit);
}

TEST_CASE("line comments are skipped") {
  auto r = tokenize("let a = 1; // comment\nprint(a);");
  REQUIRE(r.error.code == ErrorCode::Ok);
  bool saw_comment_text = false;
  for (const auto &t : r.tokens) {
    if (t.text.find("comment") != std::string_view::npos) {
      saw_comment_text = true;
    }
  }
  CHECK_FALSE(saw_comment_text);
  CHECK(r.tokens[0].kind == TokenKind::Let);
}
