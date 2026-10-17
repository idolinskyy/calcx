// Строгі тести масивів і записів (етапи G/H).
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

TEST_CASE("array literal index and len") {
  CHECK(run_ok(R"(
let a = [10, 20, 30];
print(a);
print(a[0]);
print(a[2]);
print(len(a));
)") == "[10, 20, 30]\n10\n30\n3\n");
}

TEST_CASE("array store and reference semantics") {
  CHECK(run_ok(R"(
let a = [1, 2, 3];
let b = a;
b[0] = 9;
print(a[0]);
print(b[0]);
)") == "9\n9\n");
}

TEST_CASE("array push pop") {
  CHECK(run_ok(R"(
let a = [1];
push(a, 2);
push(a, 3);
print(a);
print(pop(a));
print(a);
print(len(a));
)") == "[1, 2, 3]\n3\n[1, 2]\n2\n");
}

TEST_CASE("array index errors") {
  CHECK(run_code("let a = [1]; print(a[1]);") == ErrorCode::InvalidOperation);
  CHECK(run_code("let a = [1]; print(a[-1]);") == ErrorCode::InvalidOperation);
  CHECK(run_code("print(pop([]));") == ErrorCode::InvalidOperation);
}

TEST_CASE("array equality is deep by value") {
  CHECK(run_ok("print([1, 2] == [1, 2]);") == "true\n");
  CHECK(run_ok("print([1, 2] == [1, 3]);") == "false\n");
}

TEST_CASE("record literal fields and len") {
  CHECK(run_ok(R"(
let p = { x: 1, y: 2 };
print(p);
print(p.x);
print(p.y);
print(len(p));
)") == "{x: 1, y: 2}\n1\n2\n2\n");
}

TEST_CASE("record field assign and reference semantics") {
  CHECK(run_ok(R"(
let p = { x: 1 };
let q = p;
q.x = 5;
print(p.x);
p.z = 7;
print(q.z);
print(len(p));
)") == "5\n7\n2\n");
}

TEST_CASE("nested array and record") {
  CHECK(run_ok(R"(
let r = { pts: [{x: 1}, {x: 2}] };
print(r.pts[1].x);
r.pts[0].x = 9;
print(r.pts[0].x);
)") == "2\n9\n");
}

TEST_CASE("empty array and record") {
  CHECK(run_ok("let arr = []; let obj = {}; print(arr); print(obj);") ==
        "[]\n{}\n");
  CHECK(run_ok("print(len([]));") == "0\n");
  CHECK(run_ok("print(len({}));") == "0\n");
}

TEST_CASE("for-in over array elements") {
  CHECK(run_ok(R"(
let s = 0;
for (x in [1, 2, 3]) {
  s = s + x;
}
print(s);
)") == "6\n");
}

TEST_CASE("for-in over array index and value") {
  CHECK(run_ok(R"(
for (i, x in [10, 20]) {
  print("{}:{}", i, x);
}
)") == "0:10\n1:20\n");
}

TEST_CASE("for-in over record keys and values") {
  CHECK(run_ok(R"(
let o = { a: 1, b: 2 };
let ks = [];
for (k in o) {
  push(ks, k);
}
print(len(ks));
let sum = 0;
for (k, v in o) {
  sum = sum + v;
}
print(sum);
)") == "2\n3\n");
}

TEST_CASE("for-in empty and break") {
  CHECK(run_ok(R"(
let n = 0;
for (x in []) { n = n + 1; }
print(n);
for (x in [1, 2, 3]) {
  if (x == 2) break;
  print(x);
}
)") == "0\n1\n");
}

TEST_CASE("for-in type error") {
  CHECK(run_code("for (x in 1) { print(x); }") == ErrorCode::InvalidOperation);
}

TEST_CASE("block vs record in statement vs expression") {
  CHECK(run_ok(R"(
{
  let a = { v: 1 };
  print(a.v);
}
)") == "1\n");
}
