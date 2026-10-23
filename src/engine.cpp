// Фасад: джерело → лексер → парсер → компілятор → ВМ.
#include "calc/engine.hpp"

#include "calc/compiler.hpp"
#include "calc/lexer.hpp"
#include "calc/parser.hpp"
#include "calc/vm.hpp"

namespace calc {

std::string error_message(ErrorCode code) {
  switch (code) {
  case ErrorCode::Ok:
    return "Ок";
  case ErrorCode::UnexpectedChar:
    return "Неочікуваний символ";
  case ErrorCode::UnterminatedString:
    return "Незакритий рядковий літерал";
  case ErrorCode::InvalidEscape:
    return "Невідома escape-послідовність";
  case ErrorCode::InvalidNumber:
    return "Некоректний числовий літерал";
  case ErrorCode::UnexpectedToken:
    return "Неочікуваний токен";
  case ErrorCode::ExpectedToken:
    return "Очікується інший токен";
  case ErrorCode::MissingInitializer:
    return "Змінна має бути ініціалізована";
  case ErrorCode::StringInExpression:
    return "Рядок дозволений лише у print";
  case ErrorCode::UndeclaredVariable:
    return "Неоголошена змінна";
  case ErrorCode::RedeclaredVariable:
    return "Повторне оголошення змінної";
  case ErrorCode::FormatMismatch:
    return "Некоректний формат print";
  case ErrorCode::IntegerOverflow:
    return "Цілочисельне переповнення";
  case ErrorCode::DivisionByZero:
    return "Ділення на нуль";
  case ErrorCode::NonFiniteResult:
    return "Нескінченний або NaN результат";
  case ErrorCode::EmptyInput:
    return "Порожній вхід";
  case ErrorCode::InvalidOperation:
    return "Недопустима операція для цього типу";
  case ErrorCode::ArityMismatch:
    return "Невірна кількість аргументів";
  case ErrorCode::UnknownFunction:
    return "Невідома функція";
  case ErrorCode::BytecodeLimit:
    return "Перевищено ліміт байткоду";
  case ErrorCode::StackOverflow:
    return "Перевищено ліміт стека";
  }
  return "Невідома помилка";
}

CompileSourceResult compile_source(std::string_view source) {
  auto lexed = tokenize(source);
  if (lexed.error.code != ErrorCode::Ok) {
    return std::unexpected(lexed.error);
  }
  auto parsed = parse(lexed.tokens);
  if (!parsed) {
    return std::unexpected(parsed.error());
  }
  auto compiled = compile(*parsed);
  if (!compiled) {
    return std::unexpected(compiled.error());
  }
  return std::move(*compiled);
}

RunResult run(std::string_view source, std::ostream &out) {
  auto compiled = compile_source(source);
  if (!compiled) {
    return std::unexpected(compiled.error());
  }
  return execute(*compiled, out);
}

} // namespace calc
