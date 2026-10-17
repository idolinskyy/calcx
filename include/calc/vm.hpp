// Стекова віртуальна машина для виконання Chunk.
#pragma once

#include "calc/chunk.hpp"
#include "calc/error.hpp"
#include "calc/value.hpp"

#include <expected>
#include <ostream>
#include <vector>

namespace calc {

using ExecResult = std::expected<void, Diagnostic>;

/// Успішне виконання з фінальними значеннями локальних змінних.
struct ExecSuccess {
  std::vector<Value> locals; ///< Слоти після Halt (розмір = local_count).
};

using ExecStateResult = std::expected<ExecSuccess, Diagnostic>;

/// Виконує байткод.
/// Параметри: chunk — код; out — потік виводу (print).
/// Повертає успіх або діагностику виконання.
[[nodiscard]] ExecResult execute(const Chunk &chunk, std::ostream &out);

/// Виконує байткод і повертає фінальні значення змінних.
/// Параметри: chunk — код; out — потік виводу (print).
/// Повертає locals після Halt або діагностику.
[[nodiscard]] ExecStateResult execute_with_locals(const Chunk &chunk,
                                                  std::ostream &out);

} // namespace calc
