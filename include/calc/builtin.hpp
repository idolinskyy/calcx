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
};

[[nodiscard]] std::optional<BuiltinId> find_builtin(std::string_view name);

[[nodiscard]] std::uint8_t builtin_arity(BuiltinId id);

[[nodiscard]] std::string_view builtin_name(BuiltinId id);

/// Виклик builtin з уже знятими зі стека аргументами (порядок як у виклику).
[[nodiscard]] ArithResult call_builtin(BuiltinId id,
                                       std::span<const Value> args);

}  // namespace calc
