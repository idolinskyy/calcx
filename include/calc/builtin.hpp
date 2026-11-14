// Реєстр вбудованих функцій.
#pragma once

#include "calc/error.hpp"
#include "calc/value.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace calc {

/// Ідентифікатори builtins (індекс у CallBuiltin).
enum class BuiltinId : std::uint16_t {
  // тригонометрія
  Sin = 0,
  Cos,
  Tan,
  Asin,
  Acos,
  Atan,
  Atan2,  ///< atan2(y, x) — 2 аргументи
  // гіперболічні
  Sinh,
  Cosh,
  Tanh,
  // степінь / корінь / exp / log
  Sqrt,
  Exp,
  Ln,     ///< натуральний логарифм (alias: log)
  Log10,
  Log2,
  Pow,    ///< pow(base, exp) — 2 аргументи
  Hypot,  ///< hypot(x, y) — 2 аргументи
  // модуль / знак / комплексна частина
  Abs,
  Sign,
  Re,
  Im,
  Arg,
  Conj,
  // округлення (лише дійсні; Complex — помилка)
  Floor,
  Ceil,
  Round,
  Trunc,
  // порівняння
  Min,  ///< 2 аргументи
  Max,  ///< 2 аргументи
  // кути
  Deg,  ///< радіани → градуси
  Rad,  ///< градуси → радіани
  // масиви / записи
  Len,   ///< len(a|r) → Integer
  Push,  ///< push(arr, x) → arr (мутація)
  Pop,   ///< pop(arr) → знятий елемент
  Clone, ///< clone(x) → глибока копія масиву/запису
  Same,  ///< same(a, b) → той самий обʼєкт у памʼяті
  // явне приведення типів
  Int,       ///< int(x) → Integer
  RealCast,  ///< real(x) → Real
  BoolCast,  ///< bool(x) → Boolean
  ComplexCast, ///< complex(re) | complex(re, im)
  RationalCast, ///< rational(n, d)
  // перевірка типу значення → Boolean
  IsBool,     ///< is_bool(x)
  IsInt,      ///< is_int(x)
  IsRational, ///< is_rational(x)
  IsReal,     ///< is_real(x)
  IsComplex,  ///< is_complex(x)
  IsNumber,   ///< is_number(x) — Integer|Rational|Real|Complex
  IsArray,    ///< is_array(x)
  IsRecord,   ///< is_record(x)
  IsString,   ///< is_string(x) — ключі for-in по запису
};

struct BuiltinArity {
  std::uint8_t min{0};
  std::uint8_t max{0};
};

[[nodiscard]] std::optional<BuiltinId> find_builtin(std::string_view name);

[[nodiscard]] BuiltinArity builtin_arity_range(BuiltinId id);

/// Максимальна арність (для фіксованих builtins збігається з єдиною).
[[nodiscard]] std::uint8_t builtin_arity(BuiltinId id);

[[nodiscard]] bool builtin_arity_ok(BuiltinId id, std::size_t argc);

[[nodiscard]] std::string_view builtin_name(BuiltinId id);

/// Виклик builtin з уже знятими зі стека аргументами (порядок як у виклику).
[[nodiscard]] ArithResult call_builtin(BuiltinId id,
                                       std::span<const Value> args);

}  // namespace calc
