// Інтерактивний калькулятор: replxx (історія, стрілки, підсвітка) + сесія.
#include "calc/repl.hpp"

#include "calc/builtin.hpp"
#include "calc/constants.hpp"
#include "calc/engine.hpp"
#include "calc/error.hpp"
#include "calc/lexer.hpp"
#include "calc/token.hpp"

#include <replxx.hxx>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace calc {
namespace {

using Replxx = replxx::Replxx;

void report(const Diagnostic &d) {
  std::cerr << "помилка:" << d.pos.line << ':' << d.pos.column << ": "
            << (d.message.empty() ? error_message(d.code) : d.message) << " ["
            << static_cast<int>(d.code) << "]\n";
}

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

[[nodiscard]] bool is_quit(std::string_view s) {
  return s == ":q" || s == ":quit" || s == ":exit" || s == "quit" ||
         s == "exit" || s == ".quit";
}

[[nodiscard]] bool is_help(std::string_view s) {
  return s == ":h" || s == ":help" || s == "help" || s == ".help";
}

[[nodiscard]] bool is_clear(std::string_view s) {
  return s == ":clear" || s == ":reset" || s == ".clear";
}

void print_repl_help() {
  std::cout << "Інтерактивний calx. Рядки накопичуються в сесію (змінні/fun "
               "лишаються).\n"
            << "Незакриті { ( [ — продовження на наступному рядку (підказка "
               "...>).\n"
            << "  ↑/↓     — історія команд\n"
            << "  Tab     — доповнення ключових слів / builtins\n"
            << "  Ctrl+A / Ctrl+E — початок / кінець рядка\n"
            << "  Ctrl+L  — очистити екран\n"
            << "  Ctrl+C  — очистити поточний рядок\n"
            << "  Ctrl+D  — вийти (на порожньому рядку)\n"
            << "Команди: :help  :clear  :quit\n"
            << "Приклади:\n"
            << "  let x = 2 + 2\n"
            << "  print(x)\n"
            << "  fun add(a, b) {\n"
            << "    return a + b;\n"
            << "  }\n"
            << "  print(add(2, 3))\n";
}

[[nodiscard]] std::string history_path() {
  if (const char *home = std::getenv("HOME"); home && home[0] != '\0') {
    return std::string(home) + "/.calc_history";
  }
  return ".calc_history";
}

/// Байтовий зсув → індекс Unicode code point (як у colors_t replxx).
[[nodiscard]] std::size_t utf8_codepoint_index(std::string_view s,
                                               std::size_t byte_off) {
  std::size_t cp = 0;
  std::size_t i = 0;
  const auto lim = std::min(byte_off, s.size());
  while (i < lim) {
    const auto c = static_cast<unsigned char>(s[i]);
    if ((c & 0x80U) == 0) {
      i += 1;
    } else if ((c & 0xE0U) == 0xC0U) {
      i += 2;
    } else if ((c & 0xF0U) == 0xE0U) {
      i += 3;
    } else if ((c & 0xF8U) == 0xF0U) {
      i += 4;
    } else {
      i += 1;
    }
    ++cp;
  }
  return cp;
}

void paint(Replxx::colors_t &colors, std::size_t from_cp, std::size_t to_cp,
           Replxx::Color color) {
  const auto n = colors.size();
  if (from_cp >= n) {
    return;
  }
  to_cp = std::min(to_cp, n);
  for (std::size_t i = from_cp; i < to_cp; ++i) {
    colors[i] = color;
  }
}

[[nodiscard]] Replxx::Color color_for_token(const Token &tok) {
  using C = Replxx::Color;
  switch (tok.kind) {
  case TokenKind::Let:
  case TokenKind::Print:
  case TokenKind::If:
  case TokenKind::Else:
  case TokenKind::While:
  case TokenKind::For:
  case TokenKind::In:
  case TokenKind::Break:
  case TokenKind::Continue:
  case TokenKind::Fun:
  case TokenKind::Return:
  case TokenKind::And:
  case TokenKind::Or:
  case TokenKind::Not:
    return C::BRIGHTBLUE;
  case TokenKind::True:
  case TokenKind::False:
    return C::YELLOW;
  case TokenKind::IntegerLit:
  case TokenKind::RationalLit:
  case TokenKind::RealLit:
  case TokenKind::ImaginaryLit:
    return C::BROWN;
  case TokenKind::String:
    return C::BRIGHTGREEN;
  case TokenKind::Identifier:
    if (find_builtin(tok.text)) {
      return C::BRIGHTCYAN;
    }
    if (is_constant_name(tok.text)) {
      return C::CYAN;
    }
    return C::DEFAULT;
  case TokenKind::Plus:
  case TokenKind::Minus:
  case TokenKind::Star:
  case TokenKind::StarStar:
  case TokenKind::Slash:
  case TokenKind::Percent:
  case TokenKind::Equal:
  case TokenKind::EqEq:
  case TokenKind::NotEq:
  case TokenKind::Less:
  case TokenKind::LessEq:
  case TokenKind::Greater:
  case TokenKind::GreaterEq:
    return C::BRIGHTMAGENTA;
  case TokenKind::LParen:
  case TokenKind::RParen:
  case TokenKind::LBrace:
  case TokenKind::RBrace:
  case TokenKind::LBracket:
  case TokenKind::RBracket:
    return C::WHITE;
  case TokenKind::Comma:
  case TokenKind::Semicolon:
  case TokenKind::Dot:
  case TokenKind::Colon:
    return C::GRAY;
  case TokenKind::End:
    return C::DEFAULT;
  }
  return C::DEFAULT;
}

/// Підсвітка `//` коментарів (лексер їх пропускає).
void paint_comments(std::string_view input, Replxx::colors_t &colors) {
  bool in_string = false;
  bool escape = false;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char c = input[i];
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
    if (c == '/' && i + 1 < input.size() && input[i + 1] == '/') {
      const auto from = utf8_codepoint_index(input, i);
      std::size_t j = i;
      while (j < input.size() && input[j] != '\n') {
        ++j;
      }
      paint(colors, from, utf8_codepoint_index(input, j), Replxx::Color::GRAY);
      i = j;
    }
  }
}

void highlight(std::string const &input, Replxx::colors_t &colors) {
  const std::string_view src = input;

  // Команди REPL (:help …).
  const auto trimmed = trim(src);
  if (!trimmed.empty() && (trimmed.front() == ':' || trimmed.front() == '.')) {
    paint(colors, 0, colors.size(), Replxx::Color::BRIGHTMAGENTA);
    return;
  }

  auto lexed = tokenize(src);
  for (const auto &tok : lexed.tokens) {
    if (tok.kind == TokenKind::End || tok.text.empty()) {
      continue;
    }
    const auto from = utf8_codepoint_index(src, tok.pos.offset);
    const auto to = utf8_codepoint_index(src, tok.pos.offset + tok.text.size());
    paint(colors, from, to, color_for_token(tok));
  }

  // Незакритий рядок під час набору — підсвітити хвіст як string.
  if (lexed.error.code == ErrorCode::UnterminatedString) {
    paint(colors, utf8_codepoint_index(src, lexed.error.pos.offset),
          colors.size(), Replxx::Color::BRIGHTGREEN);
  } else if (lexed.error.code == ErrorCode::UnexpectedChar ||
             lexed.error.code == ErrorCode::InvalidNumber ||
             lexed.error.code == ErrorCode::InvalidEscape) {
    const auto at = utf8_codepoint_index(src, lexed.error.pos.offset);
    paint(colors, at, at + 1, Replxx::Color::BRIGHTRED);
  }

  paint_comments(src, colors);
}

[[nodiscard]] std::vector<std::string> completion_words() {
  static const std::vector<std::string> words = {
      "let",
      "print",
      "if",
      "else",
      "while",
      "for",
      "in",
      "break",
      "continue",
      "fun",
      "return",
      "true",
      "false",
      "and",
      "or",
      "not",
      // builtins
      "sin",
      "cos",
      "tan",
      "asin",
      "acos",
      "atan",
      "atan2",
      "sinh",
      "cosh",
      "tanh",
      "sqrt",
      "exp",
      "ln",
      "log",
      "log10",
      "log2",
      "pow",
      "hypot",
      "abs",
      "sign",
      "re",
      "im",
      "arg",
      "conj",
      "floor",
      "ceil",
      "round",
      "trunc",
      "min",
      "max",
      "deg",
      "rad",
      "len",
      "push",
      "pop",
      // constants (вибірка)
      "pi",
      "e",
      "tau",
      "phi",
      "i",
      "c",
      "g",
      "NA",
      "kB",
      "R",
      "F",
      // repl
      ":help",
      ":clear",
      ":quit",
  };
  return words;
}

Replxx::completions_t complete(std::string const &context, int &context_len) {
  // Префікс — останнє «слово» (літери/цифри/_ або команда з ':').
  int utf8_len = 0;
  int byte_start = static_cast<int>(context.size());
  for (int i = static_cast<int>(context.size()) - 1; i >= 0; --i) {
    const auto c =
        static_cast<unsigned char>(context[static_cast<std::size_t>(i)]);
    const bool cont = std::isalnum(c) || c == '_' || c == ':' || c == '.';
    if (!cont) {
      break;
    }
    byte_start = i;
  }
  const std::string prefix =
      context.substr(static_cast<std::size_t>(byte_start));
  context_len = 0;
  for (std::size_t i = 0; i < prefix.size();) {
    const auto c = static_cast<unsigned char>(prefix[i]);
    if ((c & 0x80U) == 0) {
      i += 1;
    } else if ((c & 0xE0U) == 0xC0U) {
      i += 2;
    } else if ((c & 0xF0U) == 0xE0U) {
      i += 3;
    } else if ((c & 0xF8U) == 0xF0U) {
      i += 4;
    } else {
      i += 1;
    }
    ++utf8_len;
  }
  context_len = utf8_len;

  Replxx::completions_t out;
  for (const auto &w : completion_words()) {
    if (w.size() >= prefix.size() && w.compare(0, prefix.size(), prefix) == 0) {
      out.emplace_back(w.c_str());
    }
  }
  return out;
}

} // namespace

int run_repl() {
  Replxx rx;
  rx.set_max_history_size(1000);
  rx.set_completion_count_cutoff(32);
  rx.set_word_break_characters(" \t\v\f\n+-*/%=<>!;,()[]{}\"");
  rx.set_highlighter_callback(highlight);
  rx.set_completion_callback(complete);

  const auto hist = history_path();
  rx.history_load(hist);

  std::cout << "calx (інтерактивно). :help — довідка, :quit — вихід.\n";

  std::string session;
  std::string prev_output;
  std::string pending; ///< незавершений багаторядковий ввід

  while (true) {
    const char *prompt =
        pending.empty() ? "\x1b[1;32mcalx>\x1b[0m " : "\x1b[1;32m...>\x1b[0m ";
    const char *raw = rx.input(prompt);
    if (raw == nullptr) {
      std::cout << '\n';
      break;
    }

    std::string line(raw);
    const auto cmd = trim(line);
    if (cmd.empty()) {
      continue;
    }

    if (is_quit(cmd)) {
      break;
    }
    if (is_help(cmd)) {
      print_repl_help();
      continue;
    }
    if (is_clear(cmd)) {
      session.clear();
      prev_output.clear();
      pending.clear();
      std::cout << "(сесію очищено)\n";
      continue;
    }

    rx.history_add(line);

    if (pending.empty()) {
      pending = line;
    } else {
      pending.push_back('\n');
      pending += line;
    }
    if (!repl_source_complete(pending)) {
      continue;
    }

    const std::string chunk = repl_ensure_stmt(pending);
    pending.clear();
    if (chunk.empty()) {
      continue;
    }

    const std::string trial = session + chunk;
    std::ostringstream captured;
    auto result = run(trial, captured);
    if (!result) {
      report(result.error());
      continue;
    }

    session = trial;
    const std::string full = captured.str();
    if (full.size() >= prev_output.size() &&
        full.compare(0, prev_output.size(), prev_output) == 0) {
      std::cout << full.substr(prev_output.size());
    } else {
      std::cout << full;
    }
    prev_output = full;
  }

  rx.history_save(hist);
  return 0;
}

} // namespace calc
