// Вбудовані математичні функції.
#include "calc/builtin.hpp"

#include "calc/value.hpp"

#include <cmath>
#include <complex>
#include <limits>
#include <numbers>

namespace calc {
namespace {

struct BuiltinEntry {
  std::string_view name;
  BuiltinId id;
  std::uint8_t min_arity;
  std::uint8_t max_arity;
};

/// Канонічні імена + alias `log` → Ln.
constexpr BuiltinEntry kBuiltins[] = {
    {"sin", BuiltinId::Sin, 1, 1},       {"cos", BuiltinId::Cos, 1, 1},
    {"tan", BuiltinId::Tan, 1, 1},       {"asin", BuiltinId::Asin, 1, 1},
    {"acos", BuiltinId::Acos, 1, 1},     {"atan", BuiltinId::Atan, 1, 1},
    {"atan2", BuiltinId::Atan2, 2, 2},   {"sinh", BuiltinId::Sinh, 1, 1},
    {"cosh", BuiltinId::Cosh, 1, 1},     {"tanh", BuiltinId::Tanh, 1, 1},
    {"sqrt", BuiltinId::Sqrt, 1, 1},     {"exp", BuiltinId::Exp, 1, 1},
    {"ln", BuiltinId::Ln, 1, 1},         {"log", BuiltinId::Ln, 1, 1},
    {"log10", BuiltinId::Log10, 1, 1},   {"log2", BuiltinId::Log2, 1, 1},
    {"pow", BuiltinId::Pow, 2, 2},       {"hypot", BuiltinId::Hypot, 2, 2},
    {"abs", BuiltinId::Abs, 1, 1},       {"sign", BuiltinId::Sign, 1, 1},
    {"re", BuiltinId::Re, 1, 1},         {"im", BuiltinId::Im, 1, 1},
    {"arg", BuiltinId::Arg, 1, 1},       {"conj", BuiltinId::Conj, 1, 1},
    {"floor", BuiltinId::Floor, 1, 1},   {"ceil", BuiltinId::Ceil, 1, 1},
    {"round", BuiltinId::Round, 1, 1},   {"trunc", BuiltinId::Trunc, 1, 1},
    {"min", BuiltinId::Min, 2, 2},       {"max", BuiltinId::Max, 2, 2},
    {"deg", BuiltinId::Deg, 1, 1},       {"rad", BuiltinId::Rad, 1, 1},
    {"len", BuiltinId::Len, 1, 1},       {"push", BuiltinId::Push, 2, 2},
    {"pop", BuiltinId::Pop, 1, 1},
    {"clone", BuiltinId::Clone, 1, 1},
    {"same", BuiltinId::Same, 2, 2},
    {"int", BuiltinId::Int, 1, 1},
    {"real", BuiltinId::RealCast, 1, 1},
    {"bool", BuiltinId::BoolCast, 1, 1},
    {"complex", BuiltinId::ComplexCast, 1, 2},
    {"rational", BuiltinId::RationalCast, 2, 2},
    {"is_bool", BuiltinId::IsBool, 1, 1},
    {"is_int", BuiltinId::IsInt, 1, 1},
    {"is_rational", BuiltinId::IsRational, 1, 1},
    {"is_real", BuiltinId::IsReal, 1, 1},
    {"is_complex", BuiltinId::IsComplex, 1, 1},
    {"is_number", BuiltinId::IsNumber, 1, 1},
    {"is_array", BuiltinId::IsArray, 1, 1},
    {"is_record", BuiltinId::IsRecord, 1, 1},
    {"is_string", BuiltinId::IsString, 1, 1},
};

[[nodiscard]] bool is_number(const Value& v) {
  return is_integer(v) || is_rational(v) || is_real(v) || is_complex(v);
}

[[nodiscard]] Real as_real(const Value& v) {
  if (is_real(v)) {
    return std::get<Real>(v);
  }
  if (is_integer(v)) {
    return static_cast<Real>(std::get<Integer>(v));
  }
  if (is_rational(v)) {
    const auto& r = std::get<Rational>(v);
    return static_cast<Real>(r.num) / static_cast<Real>(r.den);
  }
  return std::get<Complex>(v).real();
}

[[nodiscard]] Complex as_complex(const Value& v) {
  if (is_complex(v)) {
    return std::get<Complex>(v);
  }
  if (is_real(v)) {
    return Complex{std::get<Real>(v), Real{0}};
  }
  if (is_rational(v)) {
    const auto& r = std::get<Rational>(v);
    return Complex{static_cast<Real>(r.num) / static_cast<Real>(r.den),
                   Real{0}};
  }
  return Complex{static_cast<Real>(std::get<Integer>(v)), Real{0}};
}

[[nodiscard]] ArithResult check_finite_real(Real x) {
  if (!std::isfinite(x)) {
    return std::unexpected(ErrorCode::NonFiniteResult);
  }
  return Value{x};
}

[[nodiscard]] ArithResult check_finite_complex(Complex z) {
  if (!std::isfinite(z.real()) || !std::isfinite(z.imag())) {
    return std::unexpected(ErrorCode::NonFiniteResult);
  }
  return Value{z};
}

[[nodiscard]] bool ok_number(const Value& v) { return is_number(v); }

[[nodiscard]] ArithResult unary_real_or_complex(const Value& v, auto real_fn,
                                                auto complex_fn) {
  if (!ok_number(v)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_complex(v)) {
    return check_finite_complex(complex_fn(std::get<Complex>(v)));
  }
  return check_finite_real(real_fn(as_real(v)));
}

/// Лише дійсні (Integer/Rational/Real); Complex — помилка.
[[nodiscard]] ArithResult unary_real_only(const Value& v, auto real_fn) {
  if (!ok_number(v) || is_complex(v)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return check_finite_real(real_fn(as_real(v)));
}

[[nodiscard]] ArithResult abs_impl(const Value& a) {
  if (!ok_number(a)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_integer(a)) {
    const Integer n = std::get<Integer>(a);
    if (n == std::numeric_limits<Integer>::min()) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    return Value{n < 0 ? -n : n};
  }
  if (is_rational(a)) {
    auto r = std::get<Rational>(a);
    if (r.num == std::numeric_limits<Integer>::min()) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    if (r.num < 0) {
      r.num = -r.num;
    }
    return Value{r};
  }
  if (is_real(a)) {
    return check_finite_real(std::fabs(std::get<Real>(a)));
  }
  return check_finite_real(std::abs(std::get<Complex>(a)));
}

[[nodiscard]] ArithResult sign_impl(const Value& a) {
  if (!ok_number(a) || is_complex(a)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_integer(a)) {
    const Integer n = std::get<Integer>(a);
    if (n > 0) {
      return Value{Integer{1}};
    }
    if (n < 0) {
      return Value{Integer{-1}};
    }
    return Value{Integer{0}};
  }
  const Real x = as_real(a);
  if (x > Real{0}) {
    return Value{Integer{1}};
  }
  if (x < Real{0}) {
    return Value{Integer{-1}};
  }
  return Value{Integer{0}};
}

[[nodiscard]] ArithResult min_max_impl(const Value& a, const Value& b,
                                       bool want_min) {
  if (!ok_number(a) || !ok_number(b) || is_complex(a) || is_complex(b)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_integer(a) && is_integer(b)) {
    const Integer x = std::get<Integer>(a);
    const Integer y = std::get<Integer>(b);
    return Value{want_min ? (x < y ? x : y) : (x > y ? x : y)};
  }
  const Real x = as_real(a);
  const Real y = as_real(b);
  return check_finite_real(want_min ? std::fmin(x, y) : std::fmax(x, y));
}

[[nodiscard]] bool is_real_numeric(const Value& v) {
  return is_integer(v) || is_rational(v) || is_real(v);
}

[[nodiscard]] ArithResult cast_int(const Value& v) {
  if (is_integer(v)) {
    return v;
  }
  if (is_rational(v)) {
    const auto& r = std::get<Rational>(v);
    if (r.den != Integer{1}) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return Value{r.num};
  }
  if (is_real(v)) {
    const Real x = std::get<Real>(v);
    if (!std::isfinite(x)) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    const Real t = std::trunc(x);
    constexpr Real kMin = static_cast<Real>(std::numeric_limits<Integer>::min());
    constexpr Real kMax = static_cast<Real>(std::numeric_limits<Integer>::max());
    if (t < kMin || t > kMax) {
      return std::unexpected(ErrorCode::IntegerOverflow);
    }
    return Value{static_cast<Integer>(t)};
  }
  return std::unexpected(ErrorCode::InvalidOperation);
}

[[nodiscard]] ArithResult cast_real(const Value& v) {
  if (!is_real_numeric(v) && !is_complex(v)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  if (is_complex(v)) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
  return Value{as_real(v)};
}

[[nodiscard]] ArithResult cast_bool(const Value& v) {
  if (is_boolean(v)) {
    return v;
  }
  if (is_integer(v)) {
    return Value{std::get<Integer>(v) != Integer{0}};
  }
  if (is_rational(v)) {
    return Value{std::get<Rational>(v).num != Integer{0}};
  }
  if (is_real(v)) {
    const Real x = std::get<Real>(v);
    if (std::isnan(x)) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return Value{x != Real{0}};
  }
  return std::unexpected(ErrorCode::InvalidOperation);
}

[[nodiscard]] ArithResult cast_complex(std::span<const Value> args) {
  for (const Value& a : args) {
    if (!is_real_numeric(a)) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
  }
  const Real re = as_real(args[0]);
  const Real im = args.size() == 2 ? as_real(args[1]) : Real{0};
  return check_finite_complex(Complex{re, im});
}

}  // namespace

std::optional<BuiltinId> find_builtin(std::string_view name) {
  for (const auto& e : kBuiltins) {
    if (e.name == name) {
      return e.id;
    }
  }
  return std::nullopt;
}

BuiltinArity builtin_arity_range(BuiltinId id) {
  for (const auto& e : kBuiltins) {
    if (e.id == id) {
      return BuiltinArity{e.min_arity, e.max_arity};
    }
  }
  return {};
}

std::uint8_t builtin_arity(BuiltinId id) {
  return builtin_arity_range(id).max;
}

bool builtin_arity_ok(BuiltinId id, std::size_t argc) {
  const BuiltinArity a = builtin_arity_range(id);
  return argc >= a.min && argc <= a.max;
}

std::string_view builtin_name(BuiltinId id) {
  for (const auto& e : kBuiltins) {
    if (e.id == id) {
      return e.name;
    }
  }
  return "?";
}

ArithResult call_builtin(BuiltinId id, std::span<const Value> args) {
  if (!builtin_arity_ok(id, args.size())) {
    return std::unexpected(ErrorCode::ArityMismatch);
  }

  switch (id) {
  case BuiltinId::Sin:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::sin(x); },
        [](Complex z) { return std::sin(z); });
  case BuiltinId::Cos:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::cos(x); },
        [](Complex z) { return std::cos(z); });
  case BuiltinId::Tan:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::tan(x); },
        [](Complex z) { return std::tan(z); });
  case BuiltinId::Asin:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::asin(x); },
        [](Complex z) { return std::asin(z); });
  case BuiltinId::Acos:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::acos(x); },
        [](Complex z) { return std::acos(z); });
  case BuiltinId::Atan:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::atan(x); },
        [](Complex z) { return std::atan(z); });
  case BuiltinId::Atan2:
    if (!ok_number(args[0]) || !ok_number(args[1]) || is_complex(args[0]) ||
        is_complex(args[1])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return check_finite_real(std::atan2(as_real(args[0]), as_real(args[1])));
  case BuiltinId::Sinh:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::sinh(x); },
        [](Complex z) { return std::sinh(z); });
  case BuiltinId::Cosh:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::cosh(x); },
        [](Complex z) { return std::cosh(z); });
  case BuiltinId::Tanh:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::tanh(x); },
        [](Complex z) { return std::tanh(z); });
  case BuiltinId::Sqrt:
    if (is_complex(args[0])) {
      return check_finite_complex(std::sqrt(std::get<Complex>(args[0])));
    }
    if (!ok_number(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    {
      const Real x = as_real(args[0]);
      if (x < Real{0}) {
        return check_finite_complex(std::sqrt(Complex{x, Real{0}}));
      }
      return check_finite_real(std::sqrt(x));
    }
  case BuiltinId::Exp:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::exp(x); },
        [](Complex z) { return std::exp(z); });
  case BuiltinId::Ln:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::log(x); },
        [](Complex z) { return std::log(z); });
  case BuiltinId::Log10:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::log10(x); },
        [](Complex z) {
          return std::log(z) / std::log(Complex{Real{10}, Real{0}});
        });
  case BuiltinId::Log2:
    return unary_real_or_complex(
        args[0], [](Real x) { return std::log2(x); },
        [](Complex z) {
          return std::log(z) / std::log(Complex{Real{2}, Real{0}});
        });
  case BuiltinId::Pow:
    return pow_op(args[0], args[1]);
  case BuiltinId::Hypot:
    if (!ok_number(args[0]) || !ok_number(args[1]) || is_complex(args[0]) ||
        is_complex(args[1])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return check_finite_real(std::hypot(as_real(args[0]), as_real(args[1])));
  case BuiltinId::Abs:
    return abs_impl(args[0]);
  case BuiltinId::Sign:
    return sign_impl(args[0]);
  case BuiltinId::Re:
    if (!ok_number(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return Value{as_complex(args[0]).real()};
  case BuiltinId::Im:
    if (!ok_number(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return Value{as_complex(args[0]).imag()};
  case BuiltinId::Arg:
    if (!ok_number(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return check_finite_real(std::arg(as_complex(args[0])));
  case BuiltinId::Conj:
    if (!ok_number(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    if (is_complex(args[0])) {
      return Value{std::conj(std::get<Complex>(args[0]))};
    }
    // дійсне число — без змін (зберегти тип)
    return args[0];
  case BuiltinId::Floor:
    return unary_real_only(args[0], [](Real x) { return std::floor(x); });
  case BuiltinId::Ceil:
    return unary_real_only(args[0], [](Real x) { return std::ceil(x); });
  case BuiltinId::Round:
    return unary_real_only(args[0], [](Real x) { return std::round(x); });
  case BuiltinId::Trunc:
    return unary_real_only(args[0], [](Real x) { return std::trunc(x); });
  case BuiltinId::Min:
    return min_max_impl(args[0], args[1], true);
  case BuiltinId::Max:
    return min_max_impl(args[0], args[1], false);
  case BuiltinId::Deg:
    return unary_real_only(
        args[0], [](Real x) { return x * 180 / std::numbers::pi_v<Real>; });
  case BuiltinId::Rad:
    return unary_real_only(
        args[0], [](Real x) { return x * std::numbers::pi_v<Real> / 180; });
  case BuiltinId::Len:
    if (is_array(args[0])) {
      return Value{static_cast<Integer>(
          array_items(std::get<Array>(args[0])).size())};
    }
    if (is_record(args[0])) {
      return Value{static_cast<Integer>(
          record_fields(std::get<Record>(args[0])).size())};
    }
    return std::unexpected(ErrorCode::InvalidOperation);
  case BuiltinId::Push: {
    if (!is_array(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    Value arr = args[0];
    array_items(std::get<Array>(arr)).push_back(args[1]);
    return arr;
  }
  case BuiltinId::Pop: {
    if (!is_array(args[0])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    Value arr = args[0];
    auto &items = array_items(std::get<Array>(arr));
    if (items.empty()) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    Value last = std::move(items.back());
    items.pop_back();
    return last;
  }
  case BuiltinId::Clone:
    return clone_value(args[0]);
  case BuiltinId::Same:
    return Value{Boolean{same_object(args[0], args[1])}};
  case BuiltinId::Int:
    return cast_int(args[0]);
  case BuiltinId::RealCast:
    return cast_real(args[0]);
  case BuiltinId::BoolCast:
    return cast_bool(args[0]);
  case BuiltinId::ComplexCast:
    return cast_complex(args);
  case BuiltinId::RationalCast:
    if (!is_integer(args[0]) || !is_integer(args[1])) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    return make_rational(std::get<Integer>(args[0]),
                         std::get<Integer>(args[1]));
  case BuiltinId::IsBool:
    return Value{Boolean{is_boolean(args[0])}};
  case BuiltinId::IsInt:
    return Value{Boolean{is_integer(args[0])}};
  case BuiltinId::IsRational:
    return Value{Boolean{is_rational(args[0])}};
  case BuiltinId::IsReal:
    return Value{Boolean{is_real(args[0])}};
  case BuiltinId::IsComplex:
    return Value{Boolean{is_complex(args[0])}};
  case BuiltinId::IsNumber:
    return Value{Boolean{is_number(args[0])}};
  case BuiltinId::IsArray:
    return Value{Boolean{is_array(args[0])}};
  case BuiltinId::IsRecord:
    return Value{Boolean{is_record(args[0])}};
  case BuiltinId::IsString:
    return Value{Boolean{is_string(args[0])}};
  }
  return std::unexpected(ErrorCode::InvalidOperation);
}

}  // namespace calc
