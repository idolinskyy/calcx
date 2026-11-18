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

[[nodiscard]] bool is_ident_start(char c) {
  const auto u = static_cast<unsigned char>(c);
  return std::isalpha(u) || c == '_';
}

[[nodiscard]] bool is_ident_cont(char c) {
  const auto u = static_cast<unsigned char>(c);
  return std::isalnum(u) || c == '_';
}

void skip_ws_comments(std::string_view src, std::size_t &i) {
  while (i < src.size()) {
    const char c = src[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
      i += 2;
      while (i < src.size() && src[i] != '\n') {
        ++i;
      }
      continue;
    }
    break;
  }
}

[[nodiscard]] std::string read_ident(std::string_view src, std::size_t &i) {
  if (i >= src.size() || !is_ident_start(src[i])) {
    return {};
  }
  const std::size_t begin = i;
  ++i;
  while (i < src.size() && is_ident_cont(src[i])) {
    ++i;
  }
  return std::string(src.substr(begin, i - begin));
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

std::vector<UserFunction> scan_user_functions(std::string_view source) {
  std::vector<UserFunction> out;
  std::size_t i = 0;
  while (i < source.size()) {
    const char c = source[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
      i += 2;
      while (i < source.size() && source[i] != '\n') {
        ++i;
      }
      continue;
    }
    if (c == '"') {
      ++i;
      bool escape = false;
      while (i < source.size() && source[i] != '\n') {
        if (escape) {
          escape = false;
          ++i;
          continue;
        }
        if (source[i] == '\\') {
          escape = true;
          ++i;
          continue;
        }
        if (source[i] == '"') {
          ++i;
          break;
        }
        ++i;
      }
      continue;
    }
    if (!is_ident_start(c)) {
      ++i;
      continue;
    }
    const std::string word = read_ident(source, i);
    if (word != "fun") {
      continue;
    }
    skip_ws_comments(source, i);
    const std::string name = read_ident(source, i);
    if (name.empty()) {
      continue;
    }
    UserFunction fn;
    fn.name = name;
    skip_ws_comments(source, i);
    if (i < source.size() && source[i] == '(') {
      fn.has_signature = true;
      ++i;
      while (i < source.size() && source[i] != ')' && source[i] != '{' &&
             source[i] != ';') {
        skip_ws_comments(source, i);
        if (i >= source.size() || source[i] == ')' || source[i] == '{' ||
            source[i] == ';') {
          break;
        }
        if (source[i] == ',') {
          ++i;
          continue;
        }
        const std::string param = read_ident(source, i);
        if (param.empty()) {
          ++i;
          continue;
        }
        fn.params.push_back(param);
      }
    }
    bool replaced = false;
    for (auto &prev : out) {
      if (prev.name == fn.name) {
        prev = fn;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      out.push_back(std::move(fn));
    }
  }
  return out;
}

} // namespace calc
