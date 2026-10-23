// Строгі тести арифметики Integer/Real/Complex і форматованого друку.
#include "calc/value.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <doctest/doctest.h>

using calc::add;
using calc::Complex;
using calc::div;
using calc::ErrorCode;
using calc::format_value;
using calc::Integer;
using calc::is_complex;
using calc::is_integer;
using calc::is_rational;
using calc::is_real;
using calc::make_array;
using calc::make_record;
using calc::mod;
using calc::mul;
using calc::negate;
using calc::posit;
using calc::pow_op;
using calc::Rational;
using calc::Real;
using calc::sub;
using calc::Value;

namespace {

[[nodiscard]] bool near_real(const Value &v, Real expected, Real rel = 1e-12L) {
  REQUIRE(is_real(v));
  const Real got = std::get<Real>(v);
  if (expected == Real{0}) {
    return std::fabsl(got) <= rel;
  }
  return std::fabsl(got - expected) <= rel * std::fabsl(expected);
}

[[nodiscard]] bool near_complex(const Value &v, Complex expected,
                                Real rel = 1e-12L) {
  REQUIRE(is_complex(v));
  const auto &got = std::get<Complex>(v);
  const Real dre = std::fabsl(got.real() - expected.real());
  const Real dim = std::fabsl(got.imag() - expected.imag());
  const Real scale = std::max(
      Real{1}, std::max(std::abs(expected.real()), std::abs(expected.imag())));
  return dre <= rel * scale && dim <= rel * scale;
}

} // namespace

TEST_CASE("integer add/sub/mul stay integer") {
  auto s = add(Value{Integer{12}}, Value{Integer{3}});
  REQUIRE(s);
  CHECK(is_integer(*s));
  CHECK(std::get<Integer>(*s) == 15);

  auto d = sub(Value{Integer{12}}, Value{Integer{3}});
  REQUIRE(d);
  CHECK(std::get<Integer>(*d) == 9);

  auto p = mul(Value{Integer{12}}, Value{Integer{5}});
  REQUIRE(p);
  CHECK(is_integer(*p));
  CHECK(std::get<Integer>(*p) == 60);
}

TEST_CASE("unary minus on integer") {
  auto n = negate(Value{Integer{5}});
  REQUIRE(n);
  CHECK(std::get<Integer>(*n) == -5);

  auto overflow = negate(Value{std::numeric_limits<Integer>::min()});
  REQUIRE_FALSE(overflow);
  CHECK(overflow.error() == ErrorCode::IntegerOverflow);
}

TEST_CASE("unary plus rejects non-numeric") {
  auto ok = posit(Value{Integer{5}});
  REQUIRE(ok);
  CHECK(std::get<Integer>(*ok) == 5);

  auto bad = posit(Value{calc::Boolean{true}});
  REQUIRE_FALSE(bad);
  CHECK(bad.error() == ErrorCode::InvalidOperation);
}

TEST_CASE("integer overflow is error") {
  const Integer max = std::numeric_limits<Integer>::max();
  auto r = add(Value{max}, Value{Integer{1}});
  REQUIRE_FALSE(r);
  CHECK(r.error() == ErrorCode::IntegerOverflow);

  auto m = mul(Value{Integer{1} << 62}, Value{Integer{4}});
  REQUIRE_FALSE(m);
  CHECK(m.error() == ErrorCode::IntegerOverflow);
}

TEST_CASE("integer mod by plus or minus one is zero without UB") {
  const Integer min = std::numeric_limits<Integer>::min();
  auto a = mod(Value{min}, Value{Integer{-1}});
  REQUIRE(a);
  CHECK(std::get<Integer>(*a) == 0);

  auto b = mod(Value{min}, Value{Integer{1}});
  REQUIRE(b);
  CHECK(std::get<Integer>(*b) == 0);

  auto c = mod(Value{Integer{7}}, Value{Integer{-1}});
  REQUIRE(c);
  CHECK(std::get<Integer>(*c) == 0);
}

TEST_CASE("division always promotes to Real without Complex") {
  auto a = div(Value{Integer{5}}, Value{Integer{2}});
  REQUIRE(a);
  CHECK(is_real(*a));
  CHECK(near_real(*a, 2.5L));

  auto b = div(Value{Integer{4}}, Value{Integer{2}});
  REQUIRE(b);
  CHECK(is_real(*b));
  CHECK_FALSE(is_integer(*b));
  CHECK(near_real(*b, 2.0L));
}

TEST_CASE("integer with real promotes to real") {
  auto r = add(Value{Integer{60}}, Value{Real{-1.23L}});
  REQUIRE(r);
  CHECK(is_real(*r));
  CHECK(near_real(*r, 58.77L));

  auto p = mul(Value{Integer{60}}, Value{Real{-1.23L}});
  REQUIRE(p);
  CHECK(is_real(*p));
  CHECK(near_real(*p, -73.8L));
}

TEST_CASE("real * real stays real, never narrows to integer") {
  auto r = mul(Value{Real{2.0L}}, Value{Real{3.0L}});
  REQUIRE(r);
  CHECK(is_real(*r));
  CHECK_FALSE(is_integer(*r));
  CHECK(near_real(*r, 6.0L));
}

TEST_CASE("complex arithmetic") {
  const Value two_i = Complex{0.0L, 2.0L};
  auto c = add(Value{Integer{2}}, two_i);
  REQUIRE(c);
  CHECK(is_complex(*c));
  CHECK(near_complex(*c, Complex{2.0L, 2.0L}));

  auto m = mul(Value{Complex{1.0L, 1.0L}}, Value{Complex{1.0L, -1.0L}});
  REQUIRE(m);
  CHECK(near_complex(*m, Complex{2.0L, 0.0L}));
}

TEST_CASE("arithmetic rejects array record and string") {
  const Value arr = make_array({Value{Integer{1}}});
  const Value rec = make_record({{"x", Value{Integer{1}}}});
  const Value str = Value{std::string{"a"}};

  CHECK(add(Value{Integer{1}}, arr).error() == ErrorCode::InvalidOperation);
  CHECK(mul(arr, Value{Integer{2}}).error() == ErrorCode::InvalidOperation);
  CHECK(sub(rec, rec).error() == ErrorCode::InvalidOperation);
  CHECK(div(Value{Integer{1}}, arr).error() == ErrorCode::InvalidOperation);
  CHECK(mod(arr, Value{Integer{2}}).error() == ErrorCode::InvalidOperation);
  CHECK(pow_op(arr, Value{Integer{2}}).error() == ErrorCode::InvalidOperation);
  CHECK(negate(arr).error() == ErrorCode::InvalidOperation);
  CHECK(negate(str).error() == ErrorCode::InvalidOperation);
}

TEST_CASE("real divided by rational promotes to real") {
  auto a = div(Value{Real{1.0L}}, Value{Rational{1, 2}});
  REQUIRE(a);
  CHECK(is_real(*a));
  CHECK(near_real(*a, 2.0L));

  auto b = div(Value{Rational{1, 2}}, Value{Real{2.0L}});
  REQUIRE(b);
  CHECK(is_real(*b));
  CHECK(near_real(*b, 0.25L));
}

TEST_CASE("division by zero") {
  auto z = div(Value{Integer{1}}, Value{Integer{0}});
  REQUIRE_FALSE(z);
  CHECK(z.error() == ErrorCode::DivisionByZero);

  auto cz = div(Value{Complex{1.0L, 0.0L}}, Value{Complex{0.0L, 0.0L}});
  REQUIRE_FALSE(cz);
  CHECK(cz.error() == ErrorCode::DivisionByZero);

  auto rz = div(Value{Real{1.0L}}, Value{Real{0.0L}});
  REQUIRE_FALSE(rz);
  CHECK(rz.error() == ErrorCode::DivisionByZero);
}

TEST_CASE("format_value printing rules") {
  CHECK(format_value(Value{Integer{60}}) == "60");
  CHECK(format_value(Value{Integer{-12}}) == "-12");
  CHECK(format_value(Value{Rational{1, 2}}) == "1/2");
  CHECK(format_value(Value{Rational{-3, 4}}) == "-3/4");
  CHECK(format_value(Value{Real{2.0L}}) == "2.0");
  CHECK(format_value(Value{Real{2.5L}}) == "2.5");
  CHECK(format_value(Value{Real{-1.23L}}) == "-1.23");
  CHECK(format_value(Value{Real{-73.8L}}) == "-73.8");
  CHECK(format_value(Value{Real{58.77L}}) == "58.77");
  CHECK(format_value(Value{Complex{2.0L, 2.0L}}) == "2 + 2i");
  CHECK(format_value(Value{Complex{0.0L, 2.0L}}) == "2i");
  CHECK(format_value(Value{Complex{0.0L, -2.0L}}) == "-2i");
  CHECK(format_value(Value{Complex{2.0L, -3.5L}}) == "2 - 3.5i");
  CHECK(format_value(Value{Complex{2.0L, 0.0L}}) == "2 + 0i");
}

TEST_CASE("rational make and arithmetic") {
  using calc::make_rational;

  auto half = make_rational(1, 2);
  REQUIRE(half);
  REQUIRE(is_rational(*half));
  CHECK(std::get<Rational>(*half) == Rational{1, 2});

  auto whole = make_rational(4, 2);
  REQUIRE(whole);
  CHECK(is_integer(*whole));
  CHECK(std::get<Integer>(*whole) == 2);

  auto sum = add(Value{Rational{1, 2}}, Value{Rational{1, 3}});
  REQUIRE(sum);
  REQUIRE(is_rational(*sum));
  CHECK(std::get<Rational>(*sum) == Rational{5, 6});

  auto prod = mul(Value{Rational{1, 2}}, Value{Integer{2}});
  REQUIRE(prod);
  CHECK(is_integer(*prod));
  CHECK(std::get<Integer>(*prod) == 1);

  auto q = div(Value{Rational{1, 2}}, Value{Rational{1, 4}});
  REQUIRE(q);
  CHECK(is_integer(*q));
  CHECK(std::get<Integer>(*q) == 2);

  // Integer / Integer (не літерал) лишається Real
  auto r = div(Value{Integer{5}}, Value{Integer{2}});
  REQUIRE(r);
  CHECK(is_real(*r));
  CHECK(near_real(*r, 2.5L));

  CHECK_FALSE(make_rational(1, 0));
}
