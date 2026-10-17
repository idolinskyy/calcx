// Строгі тести: %, **, порівняння, логіка, Boolean.
#include "calc/engine.hpp"
#include "calc/value.hpp"

#include <sstream>
#include <string>

#include <doctest/doctest.h>

using calc::add;
using calc::Boolean;
using calc::eq;
using calc::ErrorCode;
using calc::gt;
using calc::Integer;
using calc::is_boolean;
using calc::is_integer;
using calc::is_rational;
using calc::is_real;
using calc::logical_and;
using calc::logical_not;
using calc::logical_or;
using calc::lt;
using calc::mod;
using calc::pow_op;
using calc::Rational;
using calc::Real;
using calc::run;
using calc::Value;

namespace {

[[nodiscard]] std::string run_ok(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE(r);
  return out.str();
}

[[nodiscard]] calc::ErrorCode run_code(std::string_view src) {
  std::ostringstream out;
  auto r = run(src, out);
  REQUIRE_FALSE(r);
  return r.error().code;
}

} // namespace

TEST_CASE("modulo integer and real") {
  auto m = mod(Value{Integer{7}}, Value{Integer{3}});
  REQUIRE(m);
  CHECK(is_integer(*m));
  CHECK(std::get<Integer>(*m) == 1);

  auto r = mod(Value{Real{7.5L}}, Value{Real{2.0L}});
  REQUIRE(r);
  CHECK(is_real(*r));

  CHECK_FALSE(mod(Value{Integer{1}}, Value{Integer{0}}));
}

TEST_CASE("power integer and negative exponent to rational") {
  auto p = pow_op(Value{Integer{2}}, Value{Integer{10}});
  REQUIRE(p);
  CHECK(std::get<Integer>(*p) == 1024);

  auto q = pow_op(Value{Integer{2}}, Value{Integer{-3}});
  REQUIRE(q);
  REQUIRE(is_rational(*q));
  CHECK(std::get<Rational>(*q) == Rational{1, 8});

  auto one = pow_op(Value{Integer{5}}, Value{Integer{0}});
  REQUIRE(one);
  CHECK(std::get<Integer>(*one) == 1);
}

TEST_CASE("comparisons and equality") {
  auto t = lt(Value{Integer{1}}, Value{Integer{2}});
  REQUIRE(t);
  CHECK(std::get<Boolean>(*t) == true);

  auto f = gt(Value{Integer{1}}, Value{Integer{2}});
  REQUIRE(f);
  CHECK(std::get<Boolean>(*f) == false);

  auto e = eq(Value{Rational{1, 2}}, Value{Real{0.5L}});
  REQUIRE(e);
  CHECK(std::get<Boolean>(*e) == true);

  CHECK_FALSE(lt(Value{Boolean{true}}, Value{Integer{1}}));
  CHECK(lt(Value{Boolean{true}}, Value{Integer{1}}).error() ==
        ErrorCode::InvalidOperation);
}

TEST_CASE("logic requires boolean") {
  auto a = logical_and(Value{Boolean{true}}, Value{Boolean{false}});
  REQUIRE(a);
  CHECK(std::get<Boolean>(*a) == false);

  auto o = logical_or(Value{Boolean{true}}, Value{Boolean{false}});
  REQUIRE(o);
  CHECK(std::get<Boolean>(*o) == true);

  auto n = logical_not(Value{Boolean{true}});
  REQUIRE(n);
  CHECK(std::get<Boolean>(*n) == false);

  CHECK_FALSE(logical_and(Value{Integer{1}}, Value{Boolean{true}}));
}

TEST_CASE("e2e mod pow compare logic") {
  CHECK(run_ok("print(7 % 3);") == "1\n");
  CHECK(run_ok("print(2 ** 10);") == "1024\n");
  CHECK(run_ok("print(2 ** -3);") == "1/8\n");
  CHECK(run_ok("print(2 ** 3 ** 2);") == "512\n"); // 2**(3**2)
  CHECK(run_ok("print(1 < 2 + 3);") == "true\n");
  CHECK(run_ok("print(1 + 2 < 3);") == "false\n");
  CHECK(run_ok("print(1/2 == 0.5);") == "true\n");
  CHECK(run_ok("print(true and false);") == "false\n");
  CHECK(run_ok("print(true or false);") == "true\n");
  CHECK(run_ok("print(not false);") == "true\n");
  CHECK(run_ok("print(1 < 2 and 3 < 4);") == "true\n");
  CHECK(run_ok("let a = 5 > 3; print(a);") == "true\n");
}

TEST_CASE("e2e type errors") {
  CHECK(run_code("print(true + 1);") == ErrorCode::InvalidOperation);
  CHECK(run_code("print(1 and true);") == ErrorCode::InvalidOperation);
  CHECK(run_code("print(2i < 3i);") == ErrorCode::InvalidOperation);
}

TEST_CASE("boolean keywords vs identifiers") {
  CHECK(run_ok("print(true);") == "true\n");
  CHECK(run_ok("print(false);") == "false\n");
  // true не можна перевизначити як імʼя через let — це ключове слово,
  // тож let true = 1 — очікується ідентифікатор → помилка ExpectedToken
  CHECK(run_code("let true = 1;") == ErrorCode::ExpectedToken);
}
