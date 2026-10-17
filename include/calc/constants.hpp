// Вбудовані іменовані константи (математика, фізика, хімія).
#pragma once

#include "calc/value.hpp"

#include <optional>
#include <string_view>

namespace calc {

/// Знайти константу за імʼям (exact match). Повертає Value або nullopt.
[[nodiscard]] std::optional<Value> find_constant(std::string_view name);

/// Чи зарезервоване імʼя як builtin-константа (для заборони fun name).
[[nodiscard]] bool is_constant_name(std::string_view name);

} // namespace calc
