// Реалізація арифметики, порівнянь і логіки для Value.
#include "calc/value.hpp"

#include <cmath>
#include <complex>
#include <format>
#include <limits>
#include <numeric>
#include <type_traits>
#include <utility>

namespace calc {

struct ArrayData {
  std::vector<Value> items;
};

struct RecordData {
  std::map<std::string, Value> fields;
};

Array::Array() : data(std::make_shared<ArrayData>()) {}
Array::~Array() = default;

Record::Record() : data(std::make_shared<RecordData>()) {}
Record::~Record() = default;

Array make_array(std::vector<Value> items) {
  Array a;
  a.data->items = std::move(items);
  return a;
}

std::vector<Value> &array_items(Array &a) { return a.data->items; }
const std::vector<Value> &array_items(const Array &a) { return a.data->items; }

Record make_record(std::map<std::string, Value> fields) {
  Record r;
  r.data->fields = std::move(fields);
  return r;
}

std::map<std::string, Value> &record_fields(Record &r) {
  return r.data->fields;
}
const std::map<std::string, Value> &record_fields(const Record &r) {
  return r.data->fields;
}

namespace {

[[nodiscard]] bool is_zero_real(Real x) { return x == Real{0}; }
[[nodiscard]] bool is_finite_real(Real x) { return std::isfinite(x); }

[[nodiscard]] bool is_finite_complex(const Complex &c) {
  return is_finite_real(c.real()) && is_finite_real(c.imag());
}

[[nodiscard]] bool is_zero_complex(const Complex &c) {
  return is_zero_real(c.real()) && is_zero_real(c.imag());
}

[[nodiscard]] bool is_number(const Value &v) {
  return is_integer(v) || is_rational(v) || is_real(v) || is_complex(v);
}

[[nodiscard]] Rational as_rational(const Value &v) {
  if (const auto *i = std::get_if<Integer>(&v)) {
    return Rational{*i, 1};
  }
  return std::get<Rational>(v);
}

[[nodiscard]] Real as_real(const Value &v) {
  if (const auto *i = std::get_if<Integer>(&v)) {
    return static_cast<Real>(*i);
  }
  if (const auto *q = std::get_if<Rational>(&v)) {
    return static_cast<Real>(q->num) / static_cast<Real>(q->den);
  }
  return std::get<Real>(v);
}

[[nodiscard]] Complex as_complex(const Value &v) {
  if (const auto *c = std::get_if<Complex>(&v)) {
    return *c;
  }
  if (const auto *r = std::get_if<Real>(&v)) {
    return Complex{*r, Real{0}};
  }
  if (const auto *q = std::get_if<Rational>(&v)) {
    return Complex{static_cast<Real>(q->num) / static_cast<Real>(q->den),
                   Real{0}};
  }
  return Complex{static_cast<Real>(std::get<Integer>(v)), Real{0}};
}

[[nodiscard]] bool involves_complex(const Value &a, const Value &b) {
  return is_complex(a) || is_complex(b);
}

[[nodiscard]] bool involves_real(const Value &a, const Value &b) {
  return is_real(a) || is_real(b);
}

[[nodiscard]] bool involves_rational(const Value &a, const Value &b) {
  return is_rational(a) || is_rational(b);
}

[[nodiscard]] bool involves_boolean(const Value &a, const Value &b) {
  return is_boolean(a) || is_boolean(b);
}

[[nodiscard]] ArithResult check_finite(Value v) {
  if (const auto *r = std::get_if<Real>(&v)) {
    if (!is_finite_real(*r)) {
      return std::unexpected(ErrorCode::NonFiniteResult);
    }
  } else if (const auto *c = std::get_if<Complex>(&v)) {
    if (!is_finite_complex(*c)) {
      return std::unexpected(ErrorCode::NonFiniteResult);
    }
  }
  return v;
}

[[nodiscard]] ArithResult add_int(Integer a, Integer b) {
  Integer sum{};
  if (__builtin_add_overflow(a, b, &sum)) {
    return std::unexpected(ErrorCode::IntegerOverflow);
  }
  return Value{sum};
}

[[nodiscard]] ArithResult sub_int(Integer a, Integer b) {
  Integer diff{};
  if (__builtin_sub_overflow(a, b, &diff)) {
    return std::unexpected(ErrorCode::IntegerOverflow);
  }
  return Value{diff};
}

[[nodiscard]] ArithResult mul_int(Integer a, Integer b) {
  Integer prod{};
  if (__builtin_mul_overflow(a, b, &prod)) {
    return std::unexpected(ErrorCode::IntegerOverflow);
  }
  return Value{prod};
}

[[nodiscard]] ArithResult mul_checked(Integer a, Integer b) {
  return mul_int(a, b);
}

[[nodiscard]] ArithResult add_rat(Rational a, Rational b) {
  auto ad = mul_checked(a.num, b.den);
  if (!ad) {
    return std::unexpected(ad.error());
  }
  auto cb = mul_checked(b.num, a.den);
  if (!cb) {
    return std::unexpected(cb.error());
  }
  auto num = add_int(std::get<Integer>(*ad), std::get<Integer>(*cb));
  if (!num) {
    return num;
  }
  auto den = mul_checked(a.den, b.den);
  if (!den) {
    return std::unexpected(den.error());
  }
  return make_rational(std::get<Integer>(*num), std::get<Integer>(*den));
}

[[nodiscard]] ArithResult sub_rat(Rational a, Rational b) {
  auto ad = mul_checked(a.num, b.den);
  if (!ad) {
    return std::unexpected(ad.error());
  }
  auto cb = mul_checked(b.num, a.den);
  if (!cb) {
    return std::unexpected(cb.error());
  }
  auto num = sub_int(std::get<Integer>(*ad), std::get<Integer>(*cb));
  if (!num) {
    return num;
  }
  auto den = mul_checked(a.den, b.den);
  if (!den) {
    return std::unexpected(den.error());
  }
  return make_rational(std::get<Integer>(*num), std::get<Integer>(*den));
}

[[nodiscard]] ArithResult mul_rat(Rational a, Rational b) {
  auto num = mul_checked(a.num, b.num);
  if (!num) {
    return std::unexpected(num.error());
  }
  auto den = mul_checked(a.den, b.den);
  if (!den) {
    return std::unexpected(den.error());
  }
  return make_rational(std::get<Integer>(*num), std::get<Integer>(*den));
}

[[nodiscard]] ArithResult div_rat(Rational a, Rational b) {
  if (b.num == 0) {
    return std::unexpected(ErrorCode::DivisionByZero);
  }
  auto num = mul_checked(a.num, b.den);
  if (!num) {
    return std::unexpected(num.error());
  }
  auto den = mul_checked(a.den, b.num);
  if (!den) {
    return std::unexpected(den.error());
  }
  return make_rational(std::get<Integer>(*num), std::get<Integer>(*den));
}

/// Порівняння a/b ? c/d через добутки (ширший цілий тип, ніж int64).
enum class Cmp : int { Lt = -1, Eq = 0, Gt = 1 };

[[nodiscard]] std::expected<Cmp, ErrorCode> cmp_rat(Rational a, Rational b) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
  using wide = __int128;
  const wide left = static_cast<wide>(a.num) * static_cast<wide>(b.den);
  const wide right = static_cast<wide>(b.num) * static_cast<wide>(a.den);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
  if (left < right) {
    return Cmp::Lt;
  }
  if (left > right) {
    return Cmp::Gt;
  }
  return Cmp::Eq;
}

[[nodiscard]] std::expected<Cmp, ErrorCode> cmp_ordered(const Value &a,
                                                        const Value &b) {
  if (!is_number(a) || !is_number(b) || involves_boolean(a, b) ||
      involves_complex(a, b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (!involves_real(a, b) && (is_integer(a) || is_rational(a)) &&
      (is_integer(b) || is_rational(b))) {
    return cmp_rat(as_rational(a), as_rational(b));
  }
  const Real ra = as_real(a);
  const Real rb = as_real(b);
  if (ra < rb) {
    return Cmp::Lt;
  }
  if (ra > rb) {
    return Cmp::Gt;
  }
  return Cmp::Eq;
}

[[nodiscard]] bool values_equal(const Value &a, const Value &b);

[[nodiscard]] bool arrays_equal(const Array &a, const Array &b) {
  const auto &ai = array_items(a);
  const auto &bi = array_items(b);
  if (ai.size() != bi.size()) {
    return false;
  }
  for (std::size_t i = 0; i < ai.size(); ++i) {
    if (!values_equal(ai[i], bi[i])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool records_equal(const Record &a, const Record &b) {
  const auto &af = record_fields(a);
  const auto &bf = record_fields(b);
  if (af.size() != bf.size()) {
    return false;
  }
  for (const auto &[k, v] : af) {
    const auto it = bf.find(k);
    if (it == bf.end() || !values_equal(v, it->second)) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool values_equal(const Value &a, const Value &b) {
  if (is_string(a) && is_string(b)) {
    return std::get<String>(a) == std::get<String>(b);
  }
  if (is_string(a) || is_string(b)) {
    return false;
  }
  if (is_array(a) && is_array(b)) {
    return arrays_equal(std::get<Array>(a), std::get<Array>(b));
  }
  if (is_record(a) && is_record(b)) {
    return records_equal(std::get<Record>(a), std::get<Record>(b));
  }
  if (is_array(a) || is_array(b) || is_record(a) || is_record(b)) {
    return false;
  }
  if (is_boolean(a) && is_boolean(b)) {
    return std::get<Boolean>(a) == std::get<Boolean>(b);
  }
  if (involves_boolean(a, b)) {
    return false; // різні «світи» типів — не рівні (eq поверне Invalid)
  }
  if (involves_complex(a, b)) {
    const auto ca = as_complex(a);
    const auto cb = as_complex(b);
    return ca.real() == cb.real() && ca.imag() == cb.imag();
  }
  if (!involves_real(a, b) && (is_integer(a) || is_rational(a)) &&
      (is_integer(b) || is_rational(b))) {
    auto c = cmp_rat(as_rational(a), as_rational(b));
    return c && *c == Cmp::Eq;
  }
  return as_real(a) == as_real(b);
}

[[nodiscard]] bool is_exact_integer(Real x) {
  if (!is_finite_real(x)) {
    return false;
  }
  Real intpart{};
  return std::modf(x, &intpart) == Real{0} &&
         x >= static_cast<Real>(std::numeric_limits<Integer>::min()) &&
         x <= static_cast<Real>(std::numeric_limits<Integer>::max());
}

[[nodiscard]] std::string format_real(Real x) {
  std::string s = std::format("{:.15Lg}", x);
  const bool has_dot = s.find('.') != std::string::npos;
  const bool has_exp =
      s.find('e') != std::string::npos || s.find('E') != std::string::npos;
  if (!has_dot && !has_exp) {
    s += ".0";
  }
  return s;
}

[[nodiscard]] std::string format_complex_component(Real x) {
  if (is_exact_integer(x)) {
    return std::format("{}", static_cast<Integer>(x));
  }
  return format_real(x);
}

[[nodiscard]] ArithResult ipow_nonneg(Integer base, Integer exp) {
  // exp >= 0
  Integer result = 1;
  Integer b = base;
  Integer e = exp;
  while (e > 0) {
    if (e & 1) {
      auto r = mul_int(result, b);
      if (!r) {
        return r;
      }
      result = std::get<Integer>(*r);
    }
    e >>= 1;
    if (e > 0) {
      auto r = mul_int(b, b);
      if (!r) {
        return r;
      }
      b = std::get<Integer>(*r);
    }
  }
  return Value{result};
}

} // namespace

ArithResult make_rational(Integer num, Integer den) {
  if (den == 0) {
    return std::unexpected(ErrorCode::DivisionByZero);
  }
  if (den < 0) {
    if (num == std::numeric_limits<Integer>::min() ||
        den == std::numeric_limits<Integer>::min()) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    num = -num;
    den = -den;
  }
  const auto u_num = num < 0 ? (0ull - static_cast<std::uint64_t>(num))
                             : static_cast<std::uint64_t>(num);
  const auto u_den = static_cast<std::uint64_t>(den);
  const auto g = static_cast<Integer>(std::gcd(u_num, u_den));
  num /= g;
  den /= g;
  if (den == 1) {
    return Value{num};
  }
  return Value{Rational{num, den}};
}

bool is_boolean(const Value &v) { return std::holds_alternative<Boolean>(v); }
bool is_integer(const Value &v) { return std::holds_alternative<Integer>(v); }
bool is_rational(const Value &v) { return std::holds_alternative<Rational>(v); }
bool is_real(const Value &v) { return std::holds_alternative<Real>(v); }
bool is_complex(const Value &v) { return std::holds_alternative<Complex>(v); }
bool is_array(const Value &v) { return std::holds_alternative<Array>(v); }
bool is_record(const Value &v) { return std::holds_alternative<Record>(v); }
bool is_string(const Value &v) { return std::holds_alternative<String>(v); }

std::string_view value_type_name(const Value &v) {
  if (is_boolean(v)) {
    return "Boolean";
  }
  if (is_integer(v)) {
    return "Integer";
  }
  if (is_rational(v)) {
    return "Rational";
  }
  if (is_real(v)) {
    return "Real";
  }
  if (is_complex(v)) {
    return "Complex";
  }
  if (is_array(v)) {
    return "Array";
  }
  if (is_record(v)) {
    return "Record";
  }
  return "String";
}

ArithResult negate(const Value &v) {
  if (is_boolean(v)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (const auto *i = std::get_if<Integer>(&v)) {
    if (*i == std::numeric_limits<Integer>::min()) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    return Value{-*i};
  }
  if (const auto *q = std::get_if<Rational>(&v)) {
    if (q->num == std::numeric_limits<Integer>::min()) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    return Value{Rational{-q->num, q->den}};
  }
  if (const auto *r = std::get_if<Real>(&v)) {
    return check_finite(Value{-*r});
  }
  return check_finite(Value{-std::get<Complex>(v)});
}

ArithResult add(const Value &a, const Value &b) {
  if (involves_boolean(a, b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (involves_complex(a, b)) {
    return check_finite(Value{as_complex(a) + as_complex(b)});
  }
  if (involves_real(a, b)) {
    return check_finite(Value{as_real(a) + as_real(b)});
  }
  if (involves_rational(a, b)) {
    return add_rat(as_rational(a), as_rational(b));
  }
  return add_int(std::get<Integer>(a), std::get<Integer>(b));
}

ArithResult sub(const Value &a, const Value &b) {
  if (involves_boolean(a, b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (involves_complex(a, b)) {
    return check_finite(Value{as_complex(a) - as_complex(b)});
  }
  if (involves_real(a, b)) {
    return check_finite(Value{as_real(a) - as_real(b)});
  }
  if (involves_rational(a, b)) {
    return sub_rat(as_rational(a), as_rational(b));
  }
  return sub_int(std::get<Integer>(a), std::get<Integer>(b));
}

ArithResult mul(const Value &a, const Value &b) {
  if (involves_boolean(a, b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (involves_complex(a, b)) {
    return check_finite(Value{as_complex(a) * as_complex(b)});
  }
  if (involves_real(a, b)) {
    return check_finite(Value{as_real(a) * as_real(b)});
  }
  if (involves_rational(a, b)) {
    return mul_rat(as_rational(a), as_rational(b));
  }
  return mul_int(std::get<Integer>(a), std::get<Integer>(b));
}

ArithResult div(const Value &a, const Value &b) {
  if (involves_boolean(a, b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (involves_complex(a, b)) {
    const Complex cb = as_complex(b);
    if (is_zero_complex(cb)) {
      return std::unexpected(ErrorCode::DivisionByZero);
    }
    return check_finite(Value{as_complex(a) / cb});
  }
  if (involves_rational(a, b)) {
    return div_rat(as_rational(a), as_rational(b));
  }
  const Real rb = as_real(b);
  if (is_zero_real(rb)) {
    return std::unexpected(ErrorCode::DivisionByZero);
  }
  return check_finite(Value{as_real(a) / rb});
}

ArithResult mod(const Value &a, const Value &b) {
  if (involves_boolean(a, b) || involves_complex(a, b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_integer(a) && is_integer(b)) {
    const Integer rb = std::get<Integer>(b);
    if (rb == 0) {
      return std::unexpected(ErrorCode::DivisionByZero);
    }
    return Value{std::get<Integer>(a) % rb};
  }
  // Rational/Real → fmod у Real.
  const Real rb = as_real(b);
  if (is_zero_real(rb)) {
    return std::unexpected(ErrorCode::DivisionByZero);
  }
  return check_finite(Value{std::fmod(as_real(a), rb)});
}

ArithResult pow_op(const Value &base, const Value &exp) {
  if (involves_boolean(base, exp)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (involves_complex(base, exp)) {
    return check_finite(Value{std::pow(as_complex(base), as_complex(exp))});
  }

  // Цілий показник і Integer/Rational основа — точний шлях.
  if (is_integer(exp) && (is_integer(base) || is_rational(base))) {
    const Integer e = std::get<Integer>(exp);
    if (e >= 0) {
      if (is_integer(base)) {
        return ipow_nonneg(std::get<Integer>(base), e);
      }
      const auto &q = std::get<Rational>(base);
      auto pn = ipow_nonneg(q.num, e);
      if (!pn) {
        return pn;
      }
      auto pd = ipow_nonneg(q.den, e);
      if (!pd) {
        return pd;
      }
      return make_rational(std::get<Integer>(*pn), std::get<Integer>(*pd));
    }
    // Відʼємний показник: 1 / base^|e| (точно через Rational, не Real).
    if (e == std::numeric_limits<Integer>::min()) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    auto pos = pow_op(base, Value{Integer{-e}});
    if (!pos) {
      return pos;
    }
    if (is_integer(*pos)) {
      return make_rational(Integer{1}, std::get<Integer>(*pos));
    }
    if (is_rational(*pos)) {
      const auto &q = std::get<Rational>(*pos);
      return make_rational(q.den, q.num);
    }
    return div(Value{Integer{1}}, *pos);
  }

  return check_finite(Value{std::pow(as_real(base), as_real(exp))});
}

ArithResult eq(const Value &a, const Value &b) {
  if (is_string(a) || is_string(b)) {
    if (is_string(a) && is_string(b)) {
      return Value{Boolean{std::get<String>(a) == std::get<String>(b)}};
    }
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_array(a) || is_array(b) || is_record(a) || is_record(b)) {
    if ((is_array(a) && is_array(b)) || (is_record(a) && is_record(b))) {
      return Value{Boolean{values_equal(a, b)}};
    }
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (involves_boolean(a, b)) {
    if (is_boolean(a) && is_boolean(b)) {
      return Value{Boolean{std::get<Boolean>(a) == std::get<Boolean>(b)}};
    }
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (!is_number(a) || !is_number(b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return Value{Boolean{values_equal(a, b)}};
}

ArithResult ne(const Value &a, const Value &b) {
  auto r = eq(a, b);
  if (!r) {
    return r;
  }
  return Value{Boolean{!std::get<Boolean>(*r)}};
}

ArithResult lt(const Value &a, const Value &b) {
  auto c = cmp_ordered(a, b);
  if (!c) {
    return std::unexpected(c.error());
  }
  return Value{Boolean{*c == Cmp::Lt}};
}

ArithResult le(const Value &a, const Value &b) {
  auto c = cmp_ordered(a, b);
  if (!c) {
    return std::unexpected(c.error());
  }
  return Value{Boolean{*c == Cmp::Lt || *c == Cmp::Eq}};
}

ArithResult gt(const Value &a, const Value &b) {
  auto c = cmp_ordered(a, b);
  if (!c) {
    return std::unexpected(c.error());
  }
  return Value{Boolean{*c == Cmp::Gt}};
}

ArithResult ge(const Value &a, const Value &b) {
  auto c = cmp_ordered(a, b);
  if (!c) {
    return std::unexpected(c.error());
  }
  return Value{Boolean{*c == Cmp::Gt || *c == Cmp::Eq}};
}

ArithResult logical_not(const Value &v) {
  if (!is_boolean(v)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return Value{Boolean{!std::get<Boolean>(v)}};
}

ArithResult logical_and(const Value &a, const Value &b) {
  if (!is_boolean(a) || !is_boolean(b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return Value{Boolean{std::get<Boolean>(a) && std::get<Boolean>(b)}};
}

ArithResult logical_or(const Value &a, const Value &b) {
  if (!is_boolean(a) || !is_boolean(b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return Value{Boolean{std::get<Boolean>(a) || std::get<Boolean>(b)}};
}

std::string format_value(const Value &v) {
  if (const auto *b = std::get_if<Boolean>(&v)) {
    return *b ? "true" : "false";
  }
  if (const auto *i = std::get_if<Integer>(&v)) {
    return std::format("{}", *i);
  }
  if (const auto *q = std::get_if<Rational>(&v)) {
    return std::format("{}/{}", q->num, q->den);
  }
  if (const auto *r = std::get_if<Real>(&v)) {
    return format_real(*r);
  }
  if (const auto *c = std::get_if<Complex>(&v)) {
    const Real re = c->real();
    const Real im = c->imag();
    if (is_zero_real(re)) {
      if (im < Real{0}) {
        return "-" + format_complex_component(-im) + "i";
      }
      return format_complex_component(im) + "i";
    }
    std::string result = format_complex_component(re);
    if (im < Real{0}) {
      result += " - ";
      result += format_complex_component(-im);
    } else {
      result += " + ";
      result += format_complex_component(im);
    }
    result += "i";
    return result;
  }
  if (const auto *arr = std::get_if<Array>(&v)) {
    std::string out = "[";
    const auto &items = array_items(*arr);
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      out += format_value(items[i]);
    }
    out += "]";
    return out;
  }
  if (const auto *rec = std::get_if<Record>(&v)) {
    std::string out = "{";
    bool first = true;
    for (const auto &[k, val] : record_fields(*rec)) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += k;
      out += ": ";
      out += format_value(val);
    }
    out += "}";
    return out;
  }
  return std::get<String>(v);
}

void print_value(std::ostream &os, const Value &v) { os << format_value(v); }

ArithResult array_get(const Value &arr, const Value &index) {
  if (is_record(arr) && is_string(index)) {
    return record_get(arr, std::get<String>(index));
  }
  if (!is_array(arr) || !is_integer(index)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  const Integer i = std::get<Integer>(index);
  const auto &items = array_items(std::get<Array>(arr));
  if (i < 0 || static_cast<std::size_t>(i) >= items.size()) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return items[static_cast<std::size_t>(i)];
}

ArithResult array_set(Value &arr, const Value &index, Value value) {
  if (is_record(arr) && is_string(index)) {
    return record_set(arr, std::get<String>(index), std::move(value));
  }
  if (!is_array(arr) || !is_integer(index)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  const Integer i = std::get<Integer>(index);
  auto &items = array_items(std::get<Array>(arr));
  if (i < 0 || static_cast<std::size_t>(i) >= items.size()) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  items[static_cast<std::size_t>(i)] = std::move(value);
  return arr;
}

ArithResult make_iter_items(const Value &coll, std::uint8_t mode) {
  if (mode != 1 && mode != 2) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_array(coll)) {
    const auto &items = array_items(std::get<Array>(coll));
    if (mode == 1) {
      return coll; // ті самі елементи (посилання на масив)
    }
    std::vector<Value> pairs;
    pairs.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
      pairs.push_back(
          Value{make_array({Value{static_cast<Integer>(i)}, items[i]})});
    }
    return Value{make_array(std::move(pairs))};
  }
  if (is_record(coll)) {
    const auto &fields = record_fields(std::get<Record>(coll));
    std::vector<Value> out;
    out.reserve(fields.size());
    if (mode == 1) {
      for (const auto &[k, _] : fields) {
        out.push_back(Value{String{k}});
      }
    } else {
      for (const auto &[k, v] : fields) {
        out.push_back(Value{make_array({Value{String{k}}, v})});
      }
    }
    return Value{make_array(std::move(out))};
  }
  return std::unexpected(ErrorCode::InvalidOperation);
}

ArithResult record_get(const Value &rec, std::string_view field) {
  if (!is_record(rec)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  const auto &fields = record_fields(std::get<Record>(rec));
  const auto it = fields.find(std::string(field));
  if (it == fields.end()) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return it->second;
}

ArithResult record_set(Value &rec, std::string_view field, Value value) {
  if (!is_record(rec)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  record_fields(std::get<Record>(rec))[std::string(field)] = std::move(value);
  return rec;
}

} // namespace calc
