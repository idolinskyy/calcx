// Форматування print у стилі std::format / std::print.
#pragma once

#include "calc/error.hpp"
#include "calc/value.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace calc {

/// Результат аналізу рядка формату на етапі компіляції.
struct FormatInfo {
  std::size_t arg_count{0}; ///< Скільки аргументів потрібно.
};

/// Аналізує формат (плейсхолдери {}, {0}, {:d}, {1:.2f}, {{ }}).
/// Параметр: fmt — рядок формату.
/// Повертає FormatInfo або FormatMismatch при некоректному синтаксисі /
/// змішуванні автоматичної й ручної індексації.
[[nodiscard]] std::expected<FormatInfo, ErrorCode>
analyze_format(std::string_view fmt);

/// Форматує рядок як std::format з аргументами Value.
/// Параметри: fmt — формат; args — значення.
/// Повертає готовий рядок або код помилки (некоректний spec / індекс).
[[nodiscard]] std::expected<std::string, ErrorCode>
format_print(std::string_view fmt, std::span<const Value> args);

} // namespace calc
