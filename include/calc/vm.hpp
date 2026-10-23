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

/// Кадр виклику для крокової ВМ.
struct VmFrame {
  std::size_t return_ip{0};
  std::vector<Value> locals;
};

enum class VmStepStatus {
  Continue, ///< можна виконувати далі
  Halted,   ///< зустріли Halt
};

/// Стан інтерпретатора (для execute і DebugSession).
struct VmState {
  std::vector<VmFrame> frames;
  std::vector<Value> stack;
  std::size_t ip{0};
};

/// Ініціалізує кадр 0 під chunk.local_count.
void vm_reset(VmState &st, const Chunk &chunk);

/// Виконує рівно одну інструкцію.
[[nodiscard]] std::expected<VmStepStatus, Diagnostic>
vm_step(VmState &st, const Chunk &chunk, std::ostream &out);

/// Виконує байткод.
[[nodiscard]] ExecResult execute(const Chunk &chunk, std::ostream &out);

/// Виконує байткод і повертає фінальні значення змінних.
[[nodiscard]] ExecStateResult execute_with_locals(const Chunk &chunk,
                                                  std::ostream &out);

} // namespace calc
