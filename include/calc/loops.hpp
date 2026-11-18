// Попередження про цикли, з яких не видно виходу.
#pragma once

#include "calc/ast.hpp"

#include <string>
#include <vector>

namespace calc {

/// Попередження, не помилка: програма запускається, але цикл може не закінчитись.
struct LoopWarning {
  SourcePos pos{};
  std::string message;
};

/// Шукає цикли без виходу і цикли, чий єдиний вихід зривається
/// через неперевірений знак змінної (наприклад від'ємний eps).
/// Параметр: program — розібрана програма.
/// Повертає попередження в порядку появи в тексті.
[[nodiscard]] std::vector<LoopWarning>
find_loop_warnings(const Program &program);

} // namespace calc
