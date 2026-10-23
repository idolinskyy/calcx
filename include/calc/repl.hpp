// Інтерактивний режим (REPL) з редагуванням рядка та історією.
#pragma once

#include <string>
#include <string_view>

namespace calc {

/// Запускає інтерактивний цикл у терміналі.
/// Повертає код виходу процесу (0 — нормальний вихід).
[[nodiscard]] int run_repl();

/// Чи збалансовані дужки/дужки масиву (поза рядками) — для багаторядкового вводу.
[[nodiscard]] bool repl_source_complete(std::string_view src);

/// Додає ';' якщо фрагмент схожий на вираз без крапки з комою.
[[nodiscard]] std::string repl_ensure_stmt(std::string_view line);

} // namespace calc
