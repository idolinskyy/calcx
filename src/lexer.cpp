// Лексер: розбір тексту на токени з позиціями.
#include "calc/lexer.hpp"
#include "calc/value.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <expected>
#include <limits>

namespace calc {
namespace {

struct Cursor {
  std::string_view source;
  std::size_t index{0};
  std::uint32_t line{1};
  std::uint32_t column{1};

  [[nodiscard]] bool at_end() const { return index >= source.size(); }

  [[nodiscard]] char peek(std::size_t ahead = 0) const {
    if (index + ahead >= source.size()) {
      return '\0';
    }
    return source[index + ahead];
  }

  [[nodiscard]] SourcePos pos() const { return SourcePos{index, line, column}; }

  char advance() {
    const char c = source[index++];
    if (c == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
    return c;
  }
};

[[nodiscard]] bool is_ident_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

[[nodiscard]] bool is_ident_cont(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

[[nodiscard]] bool is_digit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

void skip_whitespace_and_comments(Cursor &cur) {
  while (!cur.at_end()) {
    const char c = cur.peek();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      cur.advance();
      continue;
    }
    if (c == '/' && cur.peek(1) == '/') {
      while (!cur.at_end() && cur.peek() != '\n') {
        cur.advance();
      }
      continue;
    }
    break;
  }
}

[[nodiscard]] Diagnostic make_error(ErrorCode code, SourcePos pos,
                                    std::string message) {
  return Diagnostic{code, pos, std::move(message)};
}

/// Розбір рядкового літерала. Параметри: cur — курсор на відкриваючій лапці.
[[nodiscard]] std::expected<Token, Diagnostic> lex_string(Cursor &cur) {
  const SourcePos start = cur.pos();
  cur.advance(); // "
  std::string value;
  while (!cur.at_end()) {
    const char c = cur.peek();
    if (c == '\n') {
      return std::unexpected(make_error(ErrorCode::UnterminatedString, start,
                                        "Незакритий рядковий літерал"));
    }
    if (c == '"') {
      cur.advance();
      Token tok;
      tok.kind = TokenKind::String;
      tok.text = cur.source.substr(start.offset, cur.index - start.offset);
      tok.pos = start;
      tok.string_value = std::move(value);
      return tok;
    }
    if (c == '\\') {
      const SourcePos esc_pos = cur.pos();
      cur.advance();
      if (cur.at_end()) {
        return std::unexpected(make_error(ErrorCode::UnterminatedString, start,
                                          "Незакритий рядковий літерал"));
      }
      const char e = cur.advance();
      switch (e) {
      case 'n':
        value.push_back('\n');
        break;
      case 't':
        value.push_back('\t');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case '\\':
        value.push_back('\\');
        break;
      case '"':
        value.push_back('"');
        break;
      default:
        return std::unexpected(make_error(ErrorCode::InvalidEscape, esc_pos,
                                          "Невідома escape-послідовність"));
      }
      continue;
    }
    value.push_back(cur.advance());
  }
  return std::unexpected(make_error(ErrorCode::UnterminatedString, start,
                                    "Незакритий рядковий літерал"));
}

[[nodiscard]] std::expected<Token, Diagnostic> lex_number(Cursor &cur) {
  const SourcePos start = cur.pos();
  const std::size_t begin = cur.index;
  bool is_real = false;

  if (cur.peek() == '.') {
    is_real = true;
    cur.advance();
    if (!is_digit(cur.peek())) {
      return std::unexpected(
          make_error(ErrorCode::InvalidNumber, start, "Некоректний літерал"));
    }
    while (is_digit(cur.peek())) {
      cur.advance();
    }
  } else {
    while (is_digit(cur.peek())) {
      cur.advance();
    }
    if (cur.peek() == '.') {
      is_real = true;
      cur.advance();
      while (is_digit(cur.peek())) {
        cur.advance();
      }
    }
  }

  if (cur.peek() == 'e' || cur.peek() == 'E') {
    is_real = true;
    cur.advance();
    if (cur.peek() == '+' || cur.peek() == '-') {
      cur.advance();
    }
    if (!is_digit(cur.peek())) {
      return std::unexpected(
          make_error(ErrorCode::InvalidNumber, start, "Некоректний літерал"));
    }
    while (is_digit(cur.peek())) {
      cur.advance();
    }
  }

  const bool imaginary = (cur.peek() == 'i');
  if (imaginary) {
    cur.advance();
  }

  // Відхилити «злиття» з ідентифікатором: 12abc
  if (is_ident_start(cur.peek())) {
    return std::unexpected(
        make_error(ErrorCode::InvalidNumber, start, "Некоректний літерал"));
  }

  const std::string_view lexeme =
      cur.source.substr(begin, cur.index - begin - (imaginary ? 1 : 0));
  std::string buf(lexeme);
  errno = 0;
  char *end = nullptr;
  const long double parsed = std::strtold(buf.c_str(), &end);
  if (end != buf.c_str() + buf.size() || errno == ERANGE ||
      !std::isfinite(parsed)) {
    return std::unexpected(
        make_error(ErrorCode::InvalidNumber, start, "Некоректний літерал"));
  }

  Token tok;
  tok.text = cur.source.substr(begin, cur.index - begin);
  tok.pos = start;
  tok.number = parsed;

  if (imaginary) {
    tok.kind = TokenKind::ImaginaryLit;
  } else if (is_real) {
    tok.kind = TokenKind::RealLit;
  } else {
    // Integer: перевірка діапазону int64
    if (parsed <
            static_cast<long double>(std::numeric_limits<Integer>::min()) ||
        parsed >
            static_cast<long double>(std::numeric_limits<Integer>::max())) {
      return std::unexpected(
          make_error(ErrorCode::InvalidNumber, start, "Некоректний літерал"));
    }
    const auto as_int = static_cast<Integer>(parsed);
    if (static_cast<long double>(as_int) != parsed) {
      return std::unexpected(
          make_error(ErrorCode::InvalidNumber, start, "Некоректний літерал"));
    }
    tok.kind = TokenKind::IntegerLit;
    tok.number = static_cast<long double>(as_int);

    // Раціональний літерал: «1/2» без пробілів. «1 / 2» лишається Integer +
    // Slash.
    if (cur.peek() == '/') {
      const auto slash_index = cur.index;
      const auto slash_line = cur.line;
      const auto slash_column = cur.column;
      cur.advance(); // '/'
      if (is_digit(cur.peek())) {
        const auto den_begin = cur.index;
        while (is_digit(cur.peek())) {
          cur.advance();
        }
        // Не зливати з 2i / 2.0 / 2e1 — тоді це не раціональний літерал.
        const char after = cur.peek();
        if (!is_ident_start(after) && after != '.' && after != 'e' &&
            after != 'E') {
          const std::string den_buf(
              cur.source.substr(den_begin, cur.index - den_begin));
          errno = 0;
          char *den_end = nullptr;
          const long double den_parsed =
              std::strtold(den_buf.c_str(), &den_end);
          if (den_end == den_buf.c_str() + den_buf.size() && errno != ERANGE &&
              den_parsed >= 1.0L &&
              den_parsed <= static_cast<long double>(
                                std::numeric_limits<Integer>::max())) {
            const auto den_int = static_cast<Integer>(den_parsed);
            if (static_cast<long double>(den_int) == den_parsed &&
                den_int != 0) {
              tok.kind = TokenKind::RationalLit;
              tok.rat_num = as_int;
              tok.rat_den = den_int;
              tok.text = cur.source.substr(begin, cur.index - begin);
              return tok;
            }
          }
          return std::unexpected(
              make_error(ErrorCode::InvalidNumber, start,
                         "Некоректний раціональний літерал"));
        }
      }
      // Відкат: лишаємо Integer, наступний токен буде Slash.
      cur.index = slash_index;
      cur.line = slash_line;
      cur.column = slash_column;
    }
  }
  return tok;
}

[[nodiscard]] TokenKind keyword_or_ident(std::string_view text) {
  if (text == "let") {
    return TokenKind::Let;
  }
  if (text == "print") {
    return TokenKind::Print;
  }
  if (text == "if") {
    return TokenKind::If;
  }
  if (text == "else") {
    return TokenKind::Else;
  }
  if (text == "while") {
    return TokenKind::While;
  }
  if (text == "for") {
    return TokenKind::For;
  }
  if (text == "in") {
    return TokenKind::In;
  }
  if (text == "break") {
    return TokenKind::Break;
  }
  if (text == "continue") {
    return TokenKind::Continue;
  }
  if (text == "fun") {
    return TokenKind::Fun;
  }
  if (text == "return") {
    return TokenKind::Return;
  }
  if (text == "true") {
    return TokenKind::True;
  }
  if (text == "false") {
    return TokenKind::False;
  }
  if (text == "and") {
    return TokenKind::And;
  }
  if (text == "or") {
    return TokenKind::Or;
  }
  if (text == "not") {
    return TokenKind::Not;
  }
  return TokenKind::Identifier;
}

} // namespace

LexResult tokenize(std::string_view source) {
  LexResult result;
  Cursor cur{source};

  while (true) {
    skip_whitespace_and_comments(cur);
    if (cur.at_end()) {
      Token end;
      end.kind = TokenKind::End;
      end.pos = cur.pos();
      end.text = {};
      result.tokens.push_back(end);
      return result;
    }

    const SourcePos start = cur.pos();
    const char c = cur.peek();

    auto push_simple = [&](TokenKind kind, std::size_t len = 1) {
      Token tok;
      tok.kind = kind;
      tok.pos = start;
      tok.text = source.substr(start.offset, len);
      for (std::size_t i = 0; i < len; ++i) {
        cur.advance();
      }
      result.tokens.push_back(std::move(tok));
    };

    if (c == '+') {
      push_simple(TokenKind::Plus);
      continue;
    }
    if (c == '-') {
      push_simple(TokenKind::Minus);
      continue;
    }
    if (c == '*') {
      if (cur.peek(1) == '*') {
        push_simple(TokenKind::StarStar, 2);
      } else {
        push_simple(TokenKind::Star);
      }
      continue;
    }
    if (c == '/') {
      push_simple(TokenKind::Slash);
      continue;
    }
    if (c == '%') {
      push_simple(TokenKind::Percent);
      continue;
    }
    if (c == '=') {
      if (cur.peek(1) == '=') {
        push_simple(TokenKind::EqEq, 2);
      } else {
        push_simple(TokenKind::Equal);
      }
      continue;
    }
    if (c == '!') {
      if (cur.peek(1) == '=') {
        push_simple(TokenKind::NotEq, 2);
      } else {
        result.error =
            make_error(ErrorCode::UnexpectedChar, start, "Неочікуваний символ");
        return result;
      }
      continue;
    }
    if (c == '<') {
      if (cur.peek(1) == '=') {
        push_simple(TokenKind::LessEq, 2);
      } else {
        push_simple(TokenKind::Less);
      }
      continue;
    }
    if (c == '>') {
      if (cur.peek(1) == '=') {
        push_simple(TokenKind::GreaterEq, 2);
      } else {
        push_simple(TokenKind::Greater);
      }
      continue;
    }
    if (c == ',') {
      push_simple(TokenKind::Comma);
      continue;
    }
    if (c == ';') {
      push_simple(TokenKind::Semicolon);
      continue;
    }
    if (c == '(') {
      push_simple(TokenKind::LParen);
      continue;
    }
    if (c == ')') {
      push_simple(TokenKind::RParen);
      continue;
    }
    if (c == '{') {
      push_simple(TokenKind::LBrace);
      continue;
    }
    if (c == '}') {
      push_simple(TokenKind::RBrace);
      continue;
    }
    if (c == '[') {
      push_simple(TokenKind::LBracket);
      continue;
    }
    if (c == ']') {
      push_simple(TokenKind::RBracket);
      continue;
    }
    if (c == '.' && !is_digit(cur.peek(1))) {
      push_simple(TokenKind::Dot);
      continue;
    }
    if (c == ':') {
      push_simple(TokenKind::Colon);
      continue;
    }

    if (c == '"') {
      auto str = lex_string(cur);
      if (!str) {
        result.error = str.error();
        return result;
      }
      result.tokens.push_back(std::move(*str));
      continue;
    }

    if (is_digit(c) || (c == '.' && is_digit(cur.peek(1)))) {
      auto num = lex_number(cur);
      if (!num) {
        result.error = num.error();
        return result;
      }
      result.tokens.push_back(std::move(*num));
      continue;
    }

    if (is_ident_start(c)) {
      cur.advance();
      while (is_ident_cont(cur.peek())) {
        cur.advance();
      }
      Token tok;
      tok.text = source.substr(start.offset, cur.index - start.offset);
      tok.pos = start;
      tok.kind = keyword_or_ident(tok.text);
      result.tokens.push_back(std::move(tok));
      continue;
    }

    result.error =
        make_error(ErrorCode::UnexpectedChar, start, "Неочікуваний символ");
    return result;
  }
}

} // namespace calc
