// Багаторядковий ввід REPL: completeness + ensure_stmt.
#include "calc/repl.hpp"

#include <doctest/doctest.h>

using calc::repl_ensure_stmt;
using calc::repl_source_complete;
using calc::scan_user_functions;

TEST_CASE("repl_source_complete balances braces and parens") {
  CHECK(repl_source_complete("let x = 1"));
  CHECK(repl_source_complete("print(1);"));
  CHECK_FALSE(repl_source_complete("fun f() {"));
  CHECK(repl_source_complete("fun f() {\n  return 1;\n}"));
  CHECK_FALSE(repl_source_complete("print("));
  CHECK(repl_source_complete("print(1)"));
  CHECK_FALSE(repl_source_complete("let a = [1,"));
  CHECK(repl_source_complete("let a = [1, 2]"));
  CHECK_FALSE(repl_source_complete("print(\"hi"));
  CHECK(repl_source_complete("print(\"hi\")"));
  CHECK(repl_source_complete("print(\"{\")")); // brace inside string
}

TEST_CASE("repl_ensure_stmt adds semicolon for expressions") {
  CHECK(repl_ensure_stmt("let x = 1") == "let x = 1;\n");
  CHECK(repl_ensure_stmt("print(x);") == "print(x);\n");
  CHECK(repl_ensure_stmt("fun f() {\n  return 1;\n}") ==
        "fun f() {\n  return 1;\n}\n");
  // Запис у кінці рядка — потрібна `;` (не плутати з блоком).
  CHECK(repl_ensure_stmt("let p = {x: 1}") == "let p = {x: 1};\n");
  CHECK(repl_ensure_stmt("let p = {\n  x: 1\n}") == "let p = {\n  x: 1\n};\n");
  CHECK(repl_ensure_stmt("if (true) { print(1); }") ==
        "if (true) { print(1); }\n");
}

TEST_CASE("scan_user_functions sees broken bodies and skips comments") {
  const auto funs = scan_user_functions(
      "fun sqrt_newton(x, eps) {\n"
      "  y = 1.0\n"
      "}\n"
      "// fun hidden(a) {}\n"
      "print \"fun not_real(a)\";\n"
      "fun later(n) { return n; }\n"
      "fun sqrt_newton(z) { return z; }\n"
      "fun typing");
  REQUIRE(funs.size() == 3);
  CHECK(funs[0].name == "sqrt_newton");
  CHECK(funs[0].has_signature);
  REQUIRE(funs[0].params.size() == 1);
  CHECK(funs[0].params[0] == "z");
  CHECK(funs[1].name == "later");
  CHECK(funs[1].params.size() == 1);
  CHECK(funs[2].name == "typing");
  CHECK_FALSE(funs[2].has_signature);
}
