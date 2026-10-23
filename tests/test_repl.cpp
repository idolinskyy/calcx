// Багаторядковий ввід REPL: completeness + ensure_stmt.
#include "calc/repl.hpp"

#include <doctest/doctest.h>

using calc::repl_ensure_stmt;
using calc::repl_source_complete;

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
