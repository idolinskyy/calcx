// Строгі тести функцій користувача і builtins.
#include "calc/builtin.hpp"
#include "calc/engine.hpp"
#include "calc/value.hpp"

#include <array>
#include <cmath>
#include <limits>
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

TEST_CASE("simple function call and return") {
  CHECK(run_ok(R"(
fun add(a, b) {
  return a + b;
}
print(add(2, 3));
)") == "5\n");
}

TEST_CASE("function without explicit return yields 0") {
  CHECK(run_ok(R"(
fun f() { print(1); }
print(f());
)") == "1\n0\n");
}

TEST_CASE("recursive factorial") {
  CHECK(run_ok(R"(
fun fact(n) {
  if (n <= 1) return 1;
  return n * fact(n - 1);
}
print(fact(5));
)") == "120\n");
}

TEST_CASE("recursive fibonacci") {
  CHECK(run_ok(R"(
fun fib(n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}
print(fib(7));
)") == "13\n");
}

TEST_CASE("mutual recursion") {
  CHECK(run_ok(R"(
fun even(n) {
  if (n == 0) return true;
  return odd(n - 1);
}
fun odd(n) {
  if (n == 0) return false;
  return even(n - 1);
}
print(even(4));
print(odd(4));
)") == "true\nfalse\n");
}

TEST_CASE("deep recursion hits StackOverflow") {
  CHECK(run_ok(R"(
fun f(n) {
  if (n <= 0) return 0;
  return f(n - 1);
}
print(f(100));
)") == "0\n");
  CHECK(run_code(R"(
fun f() { return f(); }
print(f());
)") == ErrorCode::StackOverflow);
}

TEST_CASE("arity mismatch and unknown function") {
  CHECK(run_code("fun f(a) { return a; } print(f());") ==
        ErrorCode::ArityMismatch);
  CHECK(run_code("fun f(a) { return a; } print(f(1, 2));") ==
        ErrorCode::ArityMismatch);
  CHECK(run_code("print(nope(1));") == ErrorCode::UnknownFunction);
}

TEST_CASE("call argc over 255 is rejected before u8 truncation") {
  // Без перевірки static_cast<uint8_t>(257)==1 збігався б з arity sin/f.
  std::string args;
  for (int i = 0; i < 257; ++i) {
    if (i) {
      args += ", ";
    }
    args += "0";
  }
  CHECK(run_code("print(sin(" + args + "));") == ErrorCode::ArityMismatch);
  CHECK(run_code("fun f(a) { return a; } print(f(" + args + "));") ==
        ErrorCode::ArityMismatch);
}

TEST_CASE("fun with over 255 params is rejected") {
  std::string params;
  for (int i = 0; i < 256; ++i) {
    if (i) {
      params += ", ";
    }
    params += "p" + std::to_string(i);
  }
  CHECK(run_code("fun f(" + params + ") { return 0; }") ==
        ErrorCode::ArityMismatch);
}

TEST_CASE("return outside function is error") {
  CHECK(run_code("return 1;") == ErrorCode::UnexpectedToken);
}

TEST_CASE("function locals are independent frames") {
  CHECK(run_ok(R"(
fun f(x) {
  let y = x + 1;
  return y;
}
let y = 100;
print(f(1));
print(y);
)") == "2\n100\n");
}

TEST_CASE("builtins sin cos sqrt abs") {
  CHECK(run_ok("print(abs(-3));") == "3\n");
  CHECK(run_ok("print(abs(-1/2));") == "1/2\n");
  CHECK(run_ok("print(sqrt(9));") == "3.0\n");
  CHECK(run_ok("print(sin(0));") == "0.0\n");
  CHECK(run_ok("print(cos(0));") == "1.0\n");
}

TEST_CASE("abs Integer/Rational INT64_MIN overflows") {
  CHECK(run_code("let m = -9223372036854775807; print(abs(m - 1));") ==
        ErrorCode::IntegerOverflow);
  const auto min = std::numeric_limits<calc::Integer>::min();
  auto r = calc::call_builtin(calc::BuiltinId::Abs,
                              std::array{calc::Value{calc::Rational{min, 3}}});
  REQUIRE_FALSE(r);
  CHECK(r.error() == ErrorCode::IntegerOverflow);
}

TEST_CASE("builtins trig inverse exp log rounding") {
  CHECK(run_ok("print(tan(0));") == "0.0\n");
  CHECK(run_ok("print(asin(0));") == "0.0\n");
  CHECK(run_ok("print(acos(1));") == "0.0\n");
  CHECK(run_ok("print(atan(0));") == "0.0\n");
  CHECK(run_ok("print(exp(0));") == "1.0\n");
  CHECK(run_ok("print(ln(1));") == "0.0\n");
  CHECK(run_ok("print(log(1));") == "0.0\n"); // alias ln
  CHECK(run_ok("print(log10(100));") == "2.0\n");
  CHECK(run_ok("print(log2(8));") == "3.0\n");
  CHECK(run_ok("print(floor(3.7));") == "3.0\n");
  CHECK(run_ok("print(ceil(3.2));") == "4.0\n");
  CHECK(run_ok("print(round(3.5));") == "4.0\n");
  CHECK(run_ok("print(trunc(-3.9));") == "-3.0\n");
}

TEST_CASE("builtins binary min max hypot atan2 pow") {
  CHECK(run_ok("print(min(3, 1));") == "1\n");
  CHECK(run_ok("print(max(3, 1));") == "3\n");
  CHECK(run_ok("print(hypot(3, 4));") == "5.0\n");
  CHECK(run_ok("print(atan2(0, 1));") == "0.0\n");
  CHECK(run_ok("print(pow(2, 10));") == "1024\n");
}

TEST_CASE("builtins sign conj deg rad") {
  CHECK(run_ok("print(sign(-5));") == "-1\n");
  CHECK(run_ok("print(sign(0));") == "0\n");
  CHECK(run_ok("print(sign(2.5));") == "1\n");
  CHECK(run_ok("print(conj(3 + 4i));") == "3 - 4i\n");
  CHECK(run_ok("print(deg(0));") == "0.0\n");
  CHECK(run_ok("print(rad(0));") == "0.0\n");
}

TEST_CASE("builtins re im arg on complex") {
  CHECK(run_ok("print(re(3 + 4i));") == "3.0\n");
  CHECK(run_ok("print(im(3 + 4i));") == "4.0\n");
  CHECK(run_ok("print(abs(3 + 4i));") == "5.0\n");
  CHECK(run_ok("print(arg(1 + 0i));") == "0.0\n");
}

TEST_CASE("builtin arity and type errors") {
  CHECK(run_code("print(sin());") == ErrorCode::ArityMismatch);
  CHECK(run_code("print(sin(1, 2));") == ErrorCode::ArityMismatch);
  CHECK(run_code("print(min(1));") == ErrorCode::ArityMismatch);
  CHECK(run_code("print(sin(true));") == ErrorCode::InvalidOperation);
  CHECK(run_code("print(floor(1 + 2i));") == ErrorCode::InvalidOperation);
}

TEST_CASE("user function cannot shadow builtin name") {
  CHECK(run_code("fun sin(x) { return x; } print(sin(1));") ==
        ErrorCode::RedeclaredVariable);
}

TEST_CASE("call builtin from user function") {
  CHECK(run_ok(R"(
fun len2(a, b) {
  return sqrt(a * a + b * b);
}
print(len2(3, 4));
print(hypot(3, 4));
)") == "5.0\n5.0\n");
}

TEST_CASE("builtin constants math physics chemistry") {
  CHECK(run_ok("print(pi > 3 and pi < 4);") == "true\n");
  CHECK(run_ok("print(e > 2 and e < 3);") == "true\n");
  CHECK(run_ok("print(abs(tau - 2 * pi) < 1e-15);") == "true\n");
  CHECK(run_ok("print(c);") == "299792458.0\n");
  CHECK(run_ok("print(i);") == "1i\n");
  CHECK(run_ok("print(1 + i);") == "1 + 1i\n");
  CHECK(run_ok("print(abs(NA * qe - F) < 1e-6);") == "true\n");
  CHECK(run_ok("print(k == kB);") == "true\n");
  CHECK(run_ok("print(celsius0);") == "273.15\n");
}

TEST_CASE("builtin constant can be shadowed by let") {
  CHECK(run_ok("let pi = 3; print(pi);") == "3\n");
  CHECK(run_ok("let i = 7; print(i);") == "7\n");
}

TEST_CASE("fun cannot reuse constant name") {
  CHECK(run_code("fun pi() { return 1; } print(pi());") ==
        ErrorCode::RedeclaredVariable);
}
