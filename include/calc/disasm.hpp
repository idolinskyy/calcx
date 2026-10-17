// Текстовий дамп байткоду Chunk (опкоди, константи, змінні, формати).
#pragma once

#include "calc/chunk.hpp"
#include "calc/value.hpp"

#include <ostream>
#include <span>
#include <string_view>

namespace calc {

/// Людська назва опкоду.
/// Параметр: op — код інструкції.
/// Повертає рядок на кшталт "LoadConst".
[[nodiscard]] std::string_view opcode_name(OpCode op);

/// Друкує повний дамп Chunk у потік.
/// Параметри:
///   chunk — скомпільований код;
///   out — куди писати (зазвичай stdout);
///   local_values — опційно фінальні значення слотів після виконання
///                  (тип і значення в секції «змінні»);
///   show_temps — показувати службові слоти `__it`/`__i`/`__e` (for-in);
///                за замовчуванням їх ховаємо, лишається лише примітка.
void dump_chunk(const Chunk &chunk, std::ostream &out,
                std::span<const Value> local_values = {},
                bool show_temps = false);

} // namespace calc
