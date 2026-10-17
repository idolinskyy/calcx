// Наскрізні строгі тести: джерело → байткод → ВМ → вивід / діагностика.
#include "calc/engine.hpp"

#include <sstream>
#include <string>

#include <doctest/doctest.h>

using calc::ErrorCode;
using calc::run;

namespace {

[[nodiscard]] std::string run_ok(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE(r);
  return out.str();
}

[[nodiscard]] calc::Diagnostic run_err(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE_FALSE(r);
  CHECK(out.str().empty()); // ВМ не запускалась або нічого не надрукувала
  return r.error();
}

} // namespace

TEST_CASE("sample program output") {
  const char *src = R"(
let a = 12 * (3 + 2); let compl = 2 + 2i, neg =-1.23;
let sum = a + neg;
print("sum = {}", sum);
print(a * neg);
)";
  CHECK(run_ok(src) == "sum = 58.77\n-73.8\n");
}

TEST_CASE("spaced division is Real; glued 5/2 is Rational") {
  CHECK(run_ok("print(5 / 2);") == "2.5\n");
  CHECK(run_ok("print(4 / 2);") == "2.0\n");
  CHECK(run_ok("print(5/2);") == "5/2\n");
  CHECK(run_ok("print(4/2);") == "2\n"); // 4/2 скорочується до Integer
}

TEST_CASE("rational arithmetic stays exact") {
  CHECK(run_ok("print(1/2 + 1/3);") == "5/6\n");
  CHECK(run_ok("print(1/2 * 2/3);") == "1/3\n");
  CHECK(run_ok("print(-1/2);") == "-1/2\n");
  CHECK(run_ok("let a = 1/2; print(a + a);") == "1\n");
  CHECK(run_ok("print(1/2 + 0.5);") == "1.0\n"); // Rational + Real → Real
}

TEST_CASE("sequential let uses previous binding") {
  CHECK(run_ok("let a = 2, b = a * 3; print(b);") == "6\n");
}

TEST_CASE("undeclared variable reports code and position") {
  auto d = run_err("print(x);");
  CHECK(d.code == ErrorCode::UndeclaredVariable);
  CHECK(d.pos.line == 1);
  CHECK(d.pos.column == 7);
}

TEST_CASE("self-reference in initializer is undeclared") {
  auto d = run_err("let a = a + 1;");
  CHECK(d.code == ErrorCode::UndeclaredVariable);
}

TEST_CASE("redeclaration is error") {
  auto d = run_err("let a = 1; let a = 2;");
  CHECK(d.code == ErrorCode::RedeclaredVariable);
}

TEST_CASE("format mismatch does not run VM") {
  auto d = run_err("let x = 1; print(\"{} {}\", x);");
  CHECK(d.code == ErrorCode::FormatMismatch);
}

TEST_CASE("division by zero does not print inf") {
  std::ostringstream out;
  auto r = run("print(1 / 0);", out); // пробільний оператор ділення
  REQUIRE_FALSE(r);
  CHECK(r.error().code == ErrorCode::DivisionByZero);
  const auto text = out.str();
  CHECK(text.find("inf") == std::string::npos);
  CHECK(text.find("Inf") == std::string::npos);
  CHECK(text.find("nan") == std::string::npos);
}

TEST_CASE("glued 1/0 is invalid rational literal") {
  auto d = run_err("print(1/0);");
  CHECK(d.code == ErrorCode::InvalidNumber);
}

TEST_CASE("complex print in program") {
  CHECK(run_ok("let c = 2 + 2i; print(c);") == "2 + 2i\n");
  CHECK(run_ok("print(2i);") == "2i\n");
}

TEST_CASE("escaped braces in format") {
  CHECK(run_ok("print(\"{{}}\");") == "{}\n");
  CHECK(run_ok("let x = 1; print(\"a={{}} b={}\", x);") == "a={} b=1\n");
}
