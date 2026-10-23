// Helpers для багаторядкового REPL / IDE (без залежності від replxx).
#include "calc/repl.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace calc {
namespace {

[[nodiscard]] std::string trim(std::string_view s) {
  while (!s.empty() &&
         (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
    s.remove_prefix(1);
  }
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
    s.remove_suffix(1);
  }
  return std::string(s);
}

/// Блок-оператор (fun/if/while/for/{...}), після якого `;` не потрібна.
[[nodiscard]] bool looks_like_block_stmt(std::string_view t) {
  if (t.empty()) {
    return false;
  }
  if (t.front() == '{') {
    return true;
  }
  std::size_t i = 0;
  while (i < t.size() &&
         (std::isalnum(static_cast<unsigned char>(t[i])) || t[i] == '_')) {
    ++i;
  }
  const auto kw = t.substr(0, i);
  return kw == "fun" || kw == "if" || kw == "while" || kw == "for";
}

} // namespace

bool repl_source_complete(std::string_view src) {
  int brace = 0;
  int paren = 0;
  int bracket = 0;
  bool in_string = false;
  bool escape = false;
  for (char c : src) {
    if (in_string) {
      if (escape) {
        escape = false;
        continue;
      }
      if (c == '\\') {
        escape = true;
        continue;
      }
      if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
      continue;
    }
    switch (c) {
    case '{':
      ++brace;
      break;
    case '}':
      --brace;
      break;
    case '(':
      ++paren;
      break;
    case ')':
      --paren;
      break;
    case '[':
      ++bracket;
      break;
    case ']':
      --bracket;
      break;
    default:
      break;
    }
    if (brace < 0 || paren < 0 || bracket < 0) {
      return true; // зайва закриваюча — хай парсер повідомить
    }
  }
  return brace == 0 && paren == 0 && bracket == 0 && !in_string;
}

std::string repl_ensure_stmt(std::string_view line) {
  const auto t = trim(line);
  if (t.empty()) {
    return {};
  }
  const char last = t.back();
  if (last == ';') {
    return std::string(t) + "\n";
  }
  // fun/if/while/for/{...} закінчуються на } без `;`.
  // Але `let p = {x: 1}` — теж `}`, і `;` потрібна.
  if (last == '}' && looks_like_block_stmt(t)) {
    return std::string(t) + "\n";
  }
  return std::string(t) + ";\n";
}

} // namespace calc
