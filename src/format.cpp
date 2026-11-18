// Рендер print-формату сумісний зі std::format (плейсхолдери й specs).
#include "calc/format.hpp"

#include <format>
#include <limits>
#include <string>
#include <vector>

namespace calc {
namespace {

/// Ширина чи точність, підставлена з аргументу. Більше — ризикований рядок.
constexpr Integer kMaxDynamicExtent = 4096;

struct SpecPart {
  bool dynamic{false};
  std::string text;
  std::size_t index{0};
};

struct Field {
  std::size_t index{0};
  std::vector<SpecPart> spec;
};

enum class IndexMode { None, Automatic, Manual };

struct IndexState {
  IndexMode mode{IndexMode::None};
  std::size_t auto_next{0};
  std::size_t manual_max{0};
  bool have_manual{false};
};

/// Призначає індекс полю: явний або наступний автоматичний.
/// Автоматичну й ручну індексацію в одному рядку не змішуємо.
[[nodiscard]] std::expected<std::size_t, ErrorCode>
bind_index(bool has_index, std::size_t index, IndexState &st) {
  if (has_index) {
    if (st.mode == IndexMode::Automatic) {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    st.mode = IndexMode::Manual;
    st.have_manual = true;
    if (index > st.manual_max) {
      st.manual_max = index;
    }
    return index;
  }
  if (st.mode == IndexMode::Manual) {
    return std::unexpected(ErrorCode::FormatMismatch);
  }
  st.mode = IndexMode::Automatic;
  return st.auto_next++;
}

[[nodiscard]] bool is_digit(char c) { return c >= '0' && c <= '9'; }

/// Читає десятковий індекс з поточної позиції.
[[nodiscard]] std::expected<std::size_t, ErrorCode>
parse_index(std::string_view s, std::size_t &i) {
  if (i >= s.size() || !is_digit(s[i])) {
    return std::unexpected(ErrorCode::FormatMismatch);
  }
  std::size_t idx = 0;
  while (i < s.size() && is_digit(s[i])) {
    const auto digit = static_cast<std::size_t>(s[i] - '0');
    if (idx > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    idx = idx * 10 + digit;
    ++i;
  }
  return idx;
}

/// Кінець replacement field. Вкладене `{...}` у ширині чи точності
/// не закриває зовнішнє поле. Глибше за один рівень — помилка.
[[nodiscard]] std::size_t find_field_end(std::string_view fmt,
                                         std::size_t open) {
  int depth = 1;
  for (std::size_t i = open + 1; i < fmt.size(); ++i) {
    if (fmt[i] == '{') {
      ++depth;
      if (depth > 2) {
        return std::string_view::npos;
      }
    } else if (fmt[i] == '}') {
      --depth;
      if (depth == 0) {
        return i + 1;
      }
    }
  }
  return std::string_view::npos;
}

/// Розбирає spec після ':'. Динамічні шматки — лише `{` [індекс] `}`.
[[nodiscard]] std::expected<std::vector<SpecPart>, ErrorCode>
parse_spec(std::string_view spec, IndexState &st) {
  std::vector<SpecPart> parts;
  std::string literal;
  auto flush = [&] {
    if (!literal.empty()) {
      parts.push_back(SpecPart{false, literal, 0});
      literal.clear();
    }
  };

  for (std::size_t i = 0; i < spec.size();) {
    if (spec[i] == '}') {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    if (spec[i] != '{') {
      literal.push_back(spec[i]);
      ++i;
      continue;
    }
    flush();
    ++i;
    bool has_index = false;
    std::size_t index = 0;
    if (i < spec.size() && is_digit(spec[i])) {
      auto parsed = parse_index(spec, i);
      if (!parsed) {
        return std::unexpected(parsed.error());
      }
      has_index = true;
      index = *parsed;
    }
    if (i >= spec.size() || spec[i] != '}') {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    ++i;
    auto bound = bind_index(has_index, index, st);
    if (!bound) {
      return std::unexpected(bound.error());
    }
    parts.push_back(SpecPart{true, {}, *bound});
  }
  flush();
  return parts;
}

/// Розбирає вміст між '{' і '}'. Індекс самого поля призначається
/// раніше за вкладені поля ширини й точності — як у std::format.
[[nodiscard]] std::expected<Field, ErrorCode>
parse_field_body(std::string_view body, IndexState &st) {
  Field f;
  std::size_t i = 0;
  bool has_index = false;
  std::size_t index = 0;
  if (i < body.size() && is_digit(body[i])) {
    auto parsed = parse_index(body, i);
    if (!parsed) {
      return std::unexpected(parsed.error());
    }
    has_index = true;
    index = *parsed;
  }
  auto bound = bind_index(has_index, index, st);
  if (!bound) {
    return std::unexpected(bound.error());
  }
  f.index = *bound;

  std::string_view spec;
  if (i < body.size()) {
    if (body[i] != ':') {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    ++i;
    spec = body.substr(i);
  }
  auto parts = parse_spec(spec, st);
  if (!parts) {
    return std::unexpected(parts.error());
  }
  f.spec = std::move(*parts);
  return f;
}

struct WalkResult {
  FormatInfo info;
  std::vector<Field> fields;
};

[[nodiscard]] std::expected<WalkResult, ErrorCode>
walk_format(std::string_view fmt) {
  WalkResult wr;
  IndexState st;

  for (std::size_t i = 0; i < fmt.size();) {
    if (fmt[i] == '{') {
      if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
        i += 2;
        continue;
      }
      const auto end = find_field_end(fmt, i);
      if (end == std::string_view::npos) {
        return std::unexpected(ErrorCode::FormatMismatch);
      }
      const auto body = fmt.substr(i + 1, end - i - 2);
      auto field = parse_field_body(body, st);
      if (!field) {
        return std::unexpected(field.error());
      }
      wr.fields.push_back(std::move(*field));
      i = end;
      continue;
    }
    if (fmt[i] == '}') {
      if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
        i += 2;
        continue;
      }
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    ++i;
  }

  if (st.have_manual) {
    wr.info.arg_count = st.manual_max + 1;
  } else {
    wr.info.arg_count = st.auto_next;
  }
  return wr;
}

/// Підставляє цілі аргументи у ширину й точність. Лишається звичайний spec.
[[nodiscard]] std::expected<std::string, ErrorCode>
materialize_spec(const std::vector<SpecPart> &parts,
                 std::span<const Value> args) {
  std::string spec;
  for (const auto &part : parts) {
    if (!part.dynamic) {
      spec += part.text;
      continue;
    }
    if (part.index >= args.size()) {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    const auto *n = std::get_if<Integer>(&args[part.index]);
    if (n == nullptr || *n < 0 || *n > kMaxDynamicExtent) {
      return std::unexpected(ErrorCode::InvalidOperation);
    }
    spec += std::to_string(*n);
  }
  return spec;
}

[[nodiscard]] std::expected<std::string, ErrorCode>
format_one(const Value &v, std::string_view spec) {
  const std::string pattern =
      spec.empty() ? std::string("{}") : std::format("{{:{}}}", spec);

  try {
    if (const auto *b = std::get_if<Boolean>(&v)) {
      return std::vformat(pattern, std::make_format_args(*b));
    }
    if (const auto *i = std::get_if<Integer>(&v)) {
      return std::vformat(pattern, std::make_format_args(*i));
    }
    if (const auto *r = std::get_if<Real>(&v)) {
      return std::vformat(pattern, std::make_format_args(*r));
    }
    if (const auto *q = std::get_if<Rational>(&v)) {
      if (spec.empty()) {
        return format_value(v);
      }
      const Real as = static_cast<Real>(q->num) / static_cast<Real>(q->den);
      return std::vformat(pattern, std::make_format_args(as));
    }
    if (const auto *s = std::get_if<String>(&v)) {
      if (spec.empty()) {
        return *s;
      }
      return std::vformat(pattern, std::make_format_args(*s));
    }
    if (spec.empty()) {
      return format_value(v);
    }
    return std::unexpected(ErrorCode::InvalidOperation);
  } catch (const std::format_error &) {
    return std::unexpected(ErrorCode::InvalidOperation);
  }
}

} // namespace

std::expected<FormatInfo, ErrorCode> analyze_format(std::string_view fmt) {
  auto wr = walk_format(fmt);
  if (!wr) {
    return std::unexpected(wr.error());
  }
  return wr->info;
}

std::expected<std::string, ErrorCode>
format_print(std::string_view fmt, std::span<const Value> args) {
  auto wr = walk_format(fmt);
  if (!wr) {
    return std::unexpected(wr.error());
  }
  if (wr->info.arg_count != args.size()) {
    return std::unexpected(ErrorCode::FormatMismatch);
  }

  std::string out;
  std::size_t field_i = 0;
  for (std::size_t i = 0; i < fmt.size();) {
    if (fmt[i] == '{') {
      if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
        out.push_back('{');
        i += 2;
        continue;
      }
      const auto end = find_field_end(fmt, i);
      const auto &field = wr->fields[field_i++];
      if (field.index >= args.size()) {
        return std::unexpected(ErrorCode::FormatMismatch);
      }
      auto spec = materialize_spec(field.spec, args);
      if (!spec) {
        return std::unexpected(spec.error());
      }
      auto piece = format_one(args[field.index], *spec);
      if (!piece) {
        return std::unexpected(piece.error());
      }
      out += *piece;
      i = end;
      continue;
    }
    if (fmt[i] == '}') {
      out.push_back('}');
      i += 2;
      continue;
    }
    out.push_back(fmt[i]);
    ++i;
  }
  return out;
}

} // namespace calc
