// Строгі тести while / for, присвоєння, break / continue.
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

[[nodiscard]] ErrorCode run_code(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE_FALSE(r);
  return r.error().code;
}

} // namespace

TEST_CASE("assignment updates existing local") {
  CHECK(run_ok("let x = 1; x = x + 1; print(x);") == "2\n");
  CHECK(run_code("y = 1;") == ErrorCode::UndeclaredVariable);
}

TEST_CASE("while counts up") {
  CHECK(run_ok(R"(
let i = 0;
while (i < 3) {
  print(i);
  i = i + 1;
}
)") == "0\n1\n2\n");
}

TEST_CASE("while does not enter when false") {
  CHECK(run_ok("while (false) print(1); print(2);") == "2\n");
}

TEST_CASE("while condition must be Boolean") {
  CHECK(run_code("while (1) print(1);") == ErrorCode::InvalidOperation);
}

TEST_CASE("for with let init and step") {
  CHECK(run_ok(R"(
for (let i = 0; i < 3; i = i + 1) {
  print(i);
}
)") == "0\n1\n2\n");
}

TEST_CASE("for init let is scoped to the loop") {
  // після циклу імʼя з init недоступне (не плутати з константою i)
  CHECK(run_code(R"(
for (let n = 0; n < 1; n = n + 1) {
  print(n);
}
print(n);
)") == ErrorCode::UndeclaredVariable);
}

TEST_CASE("for empty condition is infinite until break") {
  CHECK(run_ok(R"(
let n = 0;
for (;;) {
  if (n == 2) break;
  print(n);
  n = n + 1;
}
)") == "0\n1\n");
}

TEST_CASE("for with external counter") {
  CHECK(run_ok(R"(
let i = 0;
for (; i < 2; i = i + 1) print(i);
print(i);
)") == "0\n1\n2\n");
}

TEST_CASE("break exits nearest loop") {
  CHECK(run_ok(R"(
let i = 0;
while (true) {
  if (i == 2) break;
  print(i);
  i = i + 1;
}
print(9);
)") == "0\n1\n9\n");
}

TEST_CASE("continue skips to next while iteration") {
  CHECK(run_ok(R"(
let i = 0;
while (i < 4) {
  i = i + 1;
  if (i == 2) continue;
  print(i);
}
)") == "1\n3\n4\n");
}

TEST_CASE("continue in for still runs step") {
  CHECK(run_ok(R"(
for (let i = 0; i < 5; i = i + 1) {
  if (i == 2) continue;
  print(i);
}
)") == "0\n1\n3\n4\n");
}

TEST_CASE("nested loops with break/continue") {
  CHECK(run_ok(R"(
for (let i = 0; i < 3; i = i + 1) {
  let j = 0;
  while (j < 3) {
    if (j == 1) {
      j = j + 1;
      continue;
    }
    if (i == 1 and j == 2) break;
    print(i * 10 + j);
    j = j + 1;
  }
}
)") == "0\n2\n10\n20\n22\n");
}

TEST_CASE("break/continue outside loop are errors") {
  CHECK(run_code("break;") == ErrorCode::UnexpectedToken);
  CHECK(run_code("continue;") == ErrorCode::UnexpectedToken);
  CHECK(run_code("if (true) break;") == ErrorCode::UnexpectedToken);
}

TEST_CASE("nested while") {
  CHECK(run_ok(R"(
let a = 0;
while (a < 2) {
  let b = 0;
  while (b < 2) {
    print(a * 10 + b);
    b = b + 1;
  }
  a = a + 1;
}
)") == "0\n1\n10\n11\n");
}
