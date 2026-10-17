// Коди помилок і діагностика з позицією в джерелі.
#pragma once

#include <cstdint>
#include <string>

namespace calc {

/// Позиція в тексті програми (рядок і колонка — з 1).
struct SourcePos {
  std::size_t offset{}; ///< Байтовий зсув від початку джерела.
  std::uint32_t line{1};
  std::uint32_t column{1};
};

/// Коди помилок; тести звіряють саме код і позицію, не повний текст.
enum class ErrorCode : int {
  Ok = 0,
  UnexpectedChar,
  UnterminatedString,
  InvalidEscape,
  InvalidNumber,
  UnexpectedToken,
  ExpectedToken,
  MissingInitializer,
  StringInExpression,
  UndeclaredVariable,
  RedeclaredVariable,
  FormatMismatch,
  IntegerOverflow,
  DivisionByZero,
  NonFiniteResult,
  EmptyInput,
  InvalidOperation,  ///< Недопустима операція для типу (напр. true + 1).
  ArityMismatch,     ///< Невірна кількість аргументів виклику.
  UnknownFunction,   ///< Невідома функція / builtin.
};

/// Опис однієї діагностичної помилки.
struct Diagnostic {
  ErrorCode code{ErrorCode::Ok};
  SourcePos pos{};
  std::string message;
};

}  // namespace calc
