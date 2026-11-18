// Інтерактивний режим (REPL) та helpers без залежності від replxx.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace calc {

/// Оголошення `fun`, знайдене в тексті без повного розбору.
/// Тіло може бути обірваним: для доповнення й підказки вистачає імені.
struct UserFunction {
  std::string name;
  std::vector<std::string> params;
  /// true, якщо після імені вже стоїть `(`.
  bool has_signature{false};
};

/// Усі `fun ім'я` у тексті. Коментарі й рядки ігноруються.
/// Повторне ім'я лишає останній підпис.
[[nodiscard]] std::vector<UserFunction>
scan_user_functions(std::string_view source);

/// Чи збалансовані дужки/дужки масиву (поза рядками) — для багаторядкового вводу.
/// Реалізовано в libcalc (без replxx) — для CLI і IDE.
[[nodiscard]] bool repl_source_complete(std::string_view src);

/// Додає ';' якщо фрагмент схожий на вираз без крапки з комою.
[[nodiscard]] std::string repl_ensure_stmt(std::string_view line);

/// Запускає інтерактивний цикл у терміналі (потрібен replxx).
/// Повертає код виходу процесу (0 — нормальний вихід).
[[nodiscard]] int run_repl();

} // namespace calc
