// Інтерактивний режим (REPL) та helpers без залежності від replxx.
#pragma once

#include <string>
#include <string_view>

namespace calc {

/// Чи збалансовані дужки/дужки масиву (поза рядками) — для багаторядкового вводу.
/// Реалізовано в libcalc (без replxx) — для CLI і IDE.
[[nodiscard]] bool repl_source_complete(std::string_view src);

/// Додає ';' якщо фрагмент схожий на вираз без крапки з комою.
[[nodiscard]] std::string repl_ensure_stmt(std::string_view line);

/// Запускає інтерактивний цикл у терміналі (потрібен replxx).
/// Повертає код виходу процесу (0 — нормальний вихід).
[[nodiscard]] int run_repl();

} // namespace calc
