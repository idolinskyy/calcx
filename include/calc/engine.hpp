// Фасад: компіляція джерела і виконання (або лише компіляція).
#pragma once

#include "calc/chunk.hpp"
#include "calc/error.hpp"

#include <expected>
#include <ostream>
#include <string>
#include <string_view>

namespace calc {

using CompileSourceResult = std::expected<Chunk, Diagnostic>;
using RunResult = std::expected<void, Diagnostic>;

/// Лексика + парсинг + компіляція в байткод.
/// Параметр: source — текст програми.
/// Повертає Chunk або діагностику.
[[nodiscard]] CompileSourceResult compile_source(std::string_view source);

/// Компілює і виконує програму.
/// Параметри: source — текст; out — потік виводу.
/// Повертає успіх або діагностику.
[[nodiscard]] RunResult run(std::string_view source, std::ostream& out);

/// Текст українського повідомлення для коду помилки (без позиції).
[[nodiscard]] std::string error_message(ErrorCode code);

}  // namespace calc
