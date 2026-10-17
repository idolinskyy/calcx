// Строгі тести парсера: пріоритет, let, print, помилки.
#include "calc/ast.hpp"
#include "calc/lexer.hpp"
#include "calc/parser.hpp"

#include <doctest/doctest.h>

using calc::BinaryExpr;
using calc::ErrorCode;
using calc::LetStmt;
using calc::LiteralExpr;
using calc::parse;
using calc::PrintStmt;
using calc::tokenize;
using calc::TokenKind;

namespace {

[[nodiscard]] calc::ParseResult parse_source(std::string_view src) {
  auto lexed = tokenize(src);
  REQUIRE(lexed.error.code == ErrorCode::Ok);
  return parse(lexed.tokens);
}

} // namespace

TEST_CASE("precedence 1+2*3 is add(1, mul(2,3))") {
  auto prog = parse_source("print(1+2*3);");
  REQUIRE(prog);
  REQUIRE(prog->statements.size() == 1);
  const auto &print = std::get<PrintStmt>(prog->statements[0]->node);
  REQUIRE(print.args.size() == 1);
  const auto &root = std::get<BinaryExpr>(print.args[0]->node);
  CHECK(root.op == TokenKind::Plus);
  CHECK(std::holds_alternative<LiteralExpr>(root.left->node));
  const auto &right = std::get<BinaryExpr>(root.right->node);
  CHECK(right.op == TokenKind::Star);
}

TEST_CASE("let with three bindings in one statement") {
  auto prog = parse_source("let a = 1, b = 2, c = 3;");
  REQUIRE(prog);
  REQUIRE(prog->statements.size() == 1);
  const auto &let = std::get<LetStmt>(prog->statements[0]->node);
  REQUIRE(let.bindings.size() == 3);
  CHECK(let.bindings[0].name == "a");
  CHECK(let.bindings[1].name == "b");
  CHECK(let.bindings[2].name == "c");
}

TEST_CASE("let without initializer is error") {
  auto a = parse_source("let a;");
  REQUIRE_FALSE(a);
  CHECK(a.error().code == ErrorCode::MissingInitializer);

  auto b = parse_source("let a = 1, b;");
  REQUIRE_FALSE(b);
  CHECK(b.error().code == ErrorCode::MissingInitializer);
}

TEST_CASE("print with and without format") {
  auto plain = parse_source("print(1);");
  REQUIRE(plain);
  const auto &p0 = std::get<PrintStmt>(plain->statements[0]->node);
  CHECK_FALSE(p0.has_format);
  CHECK(p0.args.size() == 1);

  auto fmt = parse_source("print(\"sum = {}\", sum);");
  REQUIRE(fmt);
  const auto &p1 = std::get<PrintStmt>(fmt->statements[0]->node);
  CHECK(p1.has_format);
  CHECK(p1.format == "sum = {}");
  CHECK(p1.args.size() == 1);
}

TEST_CASE("string in let expression is error") {
  auto r = parse_source("let a = \"hi\";");
  REQUIRE_FALSE(r);
  CHECK(r.error().code == ErrorCode::StringInExpression);
}

TEST_CASE("bare expression statement is ExprStmt") {
  auto prog = parse_source("1 + 2;");
  REQUIRE(prog);
  REQUIRE(prog->statements.size() == 1);
  CHECK(std::holds_alternative<calc::ExprStmt>(prog->statements[0]->node));
}

TEST_CASE("unary minus binds tighter than multiply") {
  auto prog = parse_source("print(-1.23 * 2);");
  REQUIRE(prog);
  const auto &print = std::get<PrintStmt>(prog->statements[0]->node);
  const auto &mul = std::get<BinaryExpr>(print.args[0]->node);
  CHECK(mul.op == TokenKind::Star);
  CHECK(std::holds_alternative<calc::UnaryExpr>(mul.left->node));
}

TEST_CASE("if / else / nested else-if parse tree") {
  auto prog = parse_source(
      "if (a < 0) { print(1); } else if (a == 0) print(2); else print(3);");
  REQUIRE(prog);
  REQUIRE(prog->statements.size() == 1);
  const auto &outer = std::get<calc::IfStmt>(prog->statements[0]->node);
  CHECK(outer.else_branch);
  const auto &then_block = std::get<calc::BlockStmt>(outer.then_branch->node);
  CHECK(then_block.statements.size() == 1);
  const auto &mid = std::get<calc::IfStmt>(outer.else_branch->node);
  CHECK(mid.else_branch);
  CHECK(std::holds_alternative<PrintStmt>(mid.then_branch->node));
  CHECK(std::holds_alternative<PrintStmt>(mid.else_branch->node));
}

TEST_CASE("if without else and nested if in then") {
  auto prog = parse_source("if (true) if (false) print(1); else print(2);");
  REQUIRE(prog);
  const auto &outer = std::get<calc::IfStmt>(prog->statements[0]->node);
  CHECK_FALSE(outer.else_branch); // else чіпляється до внутрішнього if
  const auto &inner = std::get<calc::IfStmt>(outer.then_branch->node);
  CHECK(inner.else_branch);
}
