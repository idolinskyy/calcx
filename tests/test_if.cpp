// Строгі тести if / else, вкладеності та областей блоків.
#include "calc/chunk.hpp"
#include "calc/disasm.hpp"
#include "calc/engine.hpp"

#include <sstream>
#include <string>

#include <doctest/doctest.h>

using calc::compile_source;
using calc::ErrorCode;
using calc::OpCode;
using calc::run;

namespace {

[[nodiscard]] std::string run_ok(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE(r);
  return out.str();
}

[[nodiscard]] ErrorCode run_code(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE_FALSE(r);
  return r.error().code;
}

} // namespace

TEST_CASE("if then branch runs when condition true") {
  CHECK(run_ok("if (true) print(1);") == "1\n");
  CHECK(run_ok("if (1 < 2) print(\"yes\");") == "yes\n");
}

TEST_CASE("if skips then when condition false; else optional") {
  CHECK(run_ok("if (false) print(1); print(2);") == "2\n");
  CHECK(run_ok("if (false) print(1); else print(2);") == "2\n");
  CHECK(run_ok("if (true) print(1); else print(2);") == "1\n");
}

TEST_CASE("if condition must be Boolean") {
  CHECK(run_code("if (1) print(1);") == ErrorCode::InvalidOperation);
  CHECK(run_code("if (1.0) print(1);") == ErrorCode::InvalidOperation);
}

TEST_CASE("block body and nested blocks") {
  CHECK(run_ok(R"(
if (true) {
  print(1);
  print(2);
}
)") == "1\n2\n");

  CHECK(run_ok(R"(
if (false) {
  print(0);
} else {
  print(1);
  if (true) { print(2); }
}
)") == "1\n2\n");
}

TEST_CASE("else if chain") {
  const char *src = R"(
let x = 0;
if (x < 0) {
  print("neg");
} else if (x == 0) {
  print("zero");
} else {
  print("pos");
}
)";
  CHECK(run_ok(src) == "zero\n");

  CHECK(run_ok(R"(
let x = 5;
if (x < 0) print("neg");
else if (x == 0) print("zero");
else print("pos");
)") == "pos\n");
}

TEST_CASE("dangling else binds to nearest if") {
  // if (true) if (false) print(1); else print(2);  → else до внутрішнього
  CHECK(run_ok("if (true) if (false) print(1); else print(2);") == "2\n");
  CHECK(run_ok("if (false) if (true) print(1); else print(2);") == "");
}

TEST_CASE("deeply nested if / else") {
  CHECK(run_ok(R"(
let a = 3;
if (a > 0) {
  if (a > 1) {
    if (a > 2) {
      print("deep");
    } else {
      print("mid");
    }
  } else {
    print("shallow");
  }
} else {
  print("neg");
}
)") == "deep\n");
}

TEST_CASE("block introduces scope; outer shadowing") {
  CHECK(run_ok(R"(
let x = 1;
if (true) {
  let x = 2;
  print(x);
}
print(x);
)") == "2\n1\n");
}

TEST_CASE("inner let not visible outside block") {
  CHECK(run_code(R"(
if (true) {
  let y = 1;
}
print(y);
)") == ErrorCode::UndeclaredVariable);
}

TEST_CASE("same name in sibling branch scopes") {
  CHECK(run_ok(R"(
if (1 < 2) {
  let a = 10;
  print(a);
} else {
  let a = 20;
  print(a);
}
)") == "10\n");

  CHECK(run_ok(R"(
if (false) {
  let a = 10;
  print(a);
} else {
  let a = 20;
  print(a);
}
)") == "20\n");
}

TEST_CASE("if compiles to JumpIfFalse / Jump") {
  auto chunk = compile_source("if (true) print(1); else print(2);");
  REQUIRE(chunk);
  std::ostringstream dump;
  dump_chunk(*chunk, dump);
  const auto text = dump.str();
  CHECK(text.find("JumpIfFalse") != std::string::npos);
  CHECK(text.find("Jump") != std::string::npos);
  // Обидві гілки присутні в константах / Print
  CHECK(text.find("Print") != std::string::npos);
}
