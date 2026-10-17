// Лексер: текст програми → послідовність токенів.
#pragma once

#include "calc/token.hpp"

#include <string_view>

namespace calc {

/// Лексичний аналіз джерела.
/// Параметр: source — текст програми.
/// Повертає токени або діагностику в LexResult::error.
[[nodiscard]] LexResult tokenize(std::string_view source);

}  // namespace calc
