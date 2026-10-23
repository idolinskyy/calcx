// Крокова ВМ для IDE.
#include "calc/debug.hpp"

namespace calc {

void DebugSession::load(Chunk chunk) {
  chunk_ = std::move(chunk);
  vm_reset(vm_, chunk_);
  state_ = DebugState::Paused;
  last_error_ = {};
  out_.str("");
  out_.clear();
  suppress_break_line_ = 0;
  pause_requested_.store(false);
}

void DebugSession::clear_breakpoints() { breakpoints_.clear(); }

void DebugSession::set_breakpoint(std::uint32_t line, bool enabled) {
  if (enabled) {
    breakpoints_.insert(line);
  } else {
    breakpoints_.erase(line);
  }
}

bool DebugSession::has_breakpoint(std::uint32_t line) const {
  return breakpoints_.contains(line);
}

void DebugSession::request_pause() { pause_requested_.store(true); }

void DebugSession::clear_pause_request() { pause_requested_.store(false); }

std::uint32_t DebugSession::current_line() const {
  return chunk_.line_at(vm_.ip);
}

const std::vector<Value> &DebugSession::locals() const {
  static const std::vector<Value> empty;
  if (vm_.frames.empty()) {
    return empty;
  }
  return vm_.frames.back().locals;
}

const std::vector<std::string> &DebugSession::local_names() const {
  if (vm_.frames.size() <= 1) {
    return chunk_.local_names;
  }
  const auto fi = vm_.frames.size() - 1;
  // Кадр 0 — script; кадри 1+ відповідають Call (індекс функції у стеку
  // викликів визначаємо за розміром: для панелі беремо імена з entry chunk
  // через local_count кадру).
  // Спрощення: імена з FunctionProto з найбільшою відповідністю local_count.
  for (const auto &fn : chunk_.functions) {
    if (fn.local_count == vm_.frames.back().locals.size() &&
        !fn.local_names.empty()) {
      return fn.local_names;
    }
  }
  (void)fi;
  return chunk_.local_names;
}

void DebugSession::clear_output() {
  out_.str("");
  out_.clear();
}

bool DebugSession::should_break_here() const {
  if (breakpoints_.empty()) {
    return false;
  }
  const auto line = chunk_.line_at(vm_.ip);
  if (line == suppress_break_line_) {
    return false;
  }
  return breakpoints_.contains(line);
}

bool DebugSession::pause_or_break() {
  if (pause_requested_.exchange(false)) {
    state_ = DebugState::Paused;
    return true;
  }
  if (should_break_here()) {
    suppress_break_line_ = current_line();
    state_ = DebugState::Paused;
    return true;
  }
  return false;
}

std::expected<void, Diagnostic> DebugSession::step_once() {
  if (state_ == DebugState::Finished || state_ == DebugState::Error) {
    return std::unexpected(
        Diagnostic{ErrorCode::UnexpectedToken, {}, "Сесія дебагу завершена"});
  }

  const auto line_before = current_line();
  auto step = vm_step(vm_, chunk_, out_);
  if (!step) {
    state_ = DebugState::Error;
    last_error_ = step.error();
    return std::unexpected(step.error());
  }

  if (current_line() != line_before) {
    suppress_break_line_ = 0;
  }

  if (*step == VmStepStatus::Halted) {
    state_ = DebugState::Finished;
  } else {
    state_ = DebugState::Paused;
  }
  return {};
}

std::expected<void, Diagnostic> DebugSession::step_instruction() {
  return step_once();
}

std::expected<void, Diagnostic> DebugSession::step_line() {
  state_ = DebugState::Running;
  const auto start_line = current_line();
  while (true) {
    auto r = step_once();
    if (!r) {
      return r;
    }
    if (state_ == DebugState::Finished) {
      return {};
    }
    if (pause_or_break()) {
      return {};
    }
    if (current_line() != start_line) {
      state_ = DebugState::Paused;
      return {};
    }
    state_ = DebugState::Running;
  }
}

std::expected<void, Diagnostic> DebugSession::step_over() {
  state_ = DebugState::Running;
  const auto start_line = current_line();
  const auto start_depth = vm_.frames.size();
  while (true) {
    auto r = step_once();
    if (!r) {
      return r;
    }
    if (state_ == DebugState::Finished) {
      return {};
    }
    if (pause_or_break()) {
      return {};
    }
    if (vm_.frames.size() <= start_depth && current_line() != start_line) {
      state_ = DebugState::Paused;
      return {};
    }
    state_ = DebugState::Running;
  }
}

std::expected<void, Diagnostic> DebugSession::step_out() {
  state_ = DebugState::Running;
  const auto start_depth = vm_.frames.size();
  if (start_depth <= 1) {
    return step_line();
  }
  while (true) {
    auto r = step_once();
    if (!r) {
      return r;
    }
    if (state_ == DebugState::Finished) {
      return {};
    }
    if (pause_or_break()) {
      return {};
    }
    if (vm_.frames.size() < start_depth) {
      state_ = DebugState::Paused;
      return {};
    }
    state_ = DebugState::Running;
  }
}

std::expected<void, Diagnostic> DebugSession::continue_run() {
  state_ = DebugState::Running;
  // Якщо вже стоїмо на breakpoint — пропустити цей рядок до виходу з нього.
  if (breakpoints_.contains(current_line())) {
    suppress_break_line_ = current_line();
  }
  while (true) {
    if (state_ == DebugState::Finished || state_ == DebugState::Error) {
      return {};
    }
    auto r = step_once();
    if (!r) {
      return r;
    }
    if (state_ == DebugState::Finished) {
      return {};
    }
    if (pause_or_break()) {
      return {};
    }
    state_ = DebugState::Running;
  }
}

} // namespace calc
