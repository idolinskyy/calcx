// Крокова ВМ для IDE: breakpoints і покрокове виконання.
#pragma once

#include "calc/chunk.hpp"
#include "calc/error.hpp"
#include "calc/value.hpp"
#include "calc/vm.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace calc {

enum class DebugState {
  Ready,    ///< chunk завантажено, ще не стартували / після reset
  Running,  ///< continue / довгий крок у процесі
  Paused,   ///< зупинка (breakpoint / step / pause)
  Finished, ///< Halt
  Error,    ///< runtime помилка
};

/// Сесія дебагу поверх Chunk.
class DebugSession {
public:
  void load(Chunk chunk);

  void clear_breakpoints();
  void set_breakpoint(std::uint32_t line, bool enabled);
  [[nodiscard]] bool has_breakpoint(std::uint32_t line) const;

  /// Кооперативна пауза під час continue_run / step_*.
  void request_pause();
  void clear_pause_request();

  [[nodiscard]] DebugState state() const { return state_; }
  [[nodiscard]] std::size_t ip() const { return vm_.ip; }
  [[nodiscard]] std::uint32_t current_line() const;
  [[nodiscard]] std::size_t frame_count() const { return vm_.frames.size(); }
  [[nodiscard]] const std::vector<Value> &locals() const;
  [[nodiscard]] const std::vector<std::string> &local_names() const;
  [[nodiscard]] const std::vector<Value> &operand_stack() const {
    return vm_.stack;
  }
  [[nodiscard]] const Chunk &chunk() const { return chunk_; }
  [[nodiscard]] const Diagnostic &last_error() const { return last_error_; }

  [[nodiscard]] std::string output() const { return out_.str(); }
  void clear_output();

  [[nodiscard]] std::expected<void, Diagnostic> step_instruction();
  [[nodiscard]] std::expected<void, Diagnostic> step_line();   ///< Step Into
  [[nodiscard]] std::expected<void, Diagnostic> step_over();
  [[nodiscard]] std::expected<void, Diagnostic> step_out();
  [[nodiscard]] std::expected<void, Diagnostic> continue_run();

private:
  Chunk chunk_;
  VmState vm_;
  DebugState state_{DebugState::Ready};
  Diagnostic last_error_{};
  std::ostringstream out_;
  std::unordered_set<std::uint32_t> breakpoints_;
  /// Не зупинятись знову на цьому рядку, доки IP його не полишить.
  std::uint32_t suppress_break_line_{0};
  std::atomic<bool> pause_requested_{false};

  [[nodiscard]] std::expected<void, Diagnostic> step_once();
  [[nodiscard]] bool should_break_here() const;
  [[nodiscard]] bool pause_or_break();
};

} // namespace calc
