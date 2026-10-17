// Рендер print-формату сумісний зі std::format (плейсхолдери й specs).
#include "calc/format.hpp"

#include <format>
#include <vector>

namespace calc {
namespace {

struct Field {
  bool has_index{false};
  std::size_t index{0};
  std::string_view spec; ///< без ':'
};

/// Знаходить кінець replacement field після '{'.
/// Повертає позицію після '}', або npos.
[[nodiscard]] std::size_t find_field_end(std::string_view fmt,
                                         std::size_t open) {
  // open вказує на '{'. Шукаємо парну '}' (не екрановану — у spec '}' лише
  // як кінець поля у нашому спрощеному синтаксисі).
  for (std::size_t i = open + 1; i < fmt.size(); ++i) {
    if (fmt[i] == '}') {
      return i + 1;
    }
  }
  return std::string_view::npos;
}

/// Розбирає вміст між '{' і '}' → Field.
[[nodiscard]] std::expected<Field, ErrorCode>
parse_field_body(std::string_view body) {
  Field f;
  std::size_t i = 0;
  if (i < body.size() && body[i] >= '0' && body[i] <= '9') {
    std::size_t idx = 0;
    while (i < body.size() && body[i] >= '0' && body[i] <= '9') {
      idx = idx * 10 + static_cast<std::size_t>(body[i] - '0');
      ++i;
    }
    f.has_index = true;
    f.index = idx;
  }
  if (i < body.size()) {
    if (body[i] != ':') {
      return std::unexpected(ErrorCode::FormatMismatch);
    }
    ++i;
    f.spec = body.substr(i);
  }
  return f;
}

enum class IndexMode { None, Automatic, Manual };

struct WalkResult {
  FormatInfo info;
  std::vector<Field>
      fields; ///< у порядку появи (для runtime можна знову парсити)
};

[[nodiscard]] std::expected<WalkResult, ErrorCode>
walk_format(std::string_view fmt) {
  WalkResult wr;
  IndexMode mode = IndexMode::None;
  std::size_t auto_next = 0;
  std::size_t manual_max = 0;
  bool have_manual = false;

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
      auto field = parse_field_body(body);
      if (!field) {
        return std::unexpected(field.error());
      }
      if (field->has_index) {
        if (mode == IndexMode::Automatic) {
          return std::unexpected(ErrorCode::FormatMismatch);
        }
        mode = IndexMode::Manual;
        have_manual = true;
        if (field->index > manual_max) {
          manual_max = field->index;
        }
      } else {
        if (mode == IndexMode::Manual) {
          return std::unexpected(ErrorCode::FormatMismatch);
        }
        mode = IndexMode::Automatic;
        field->index = auto_next++;
        field->has_index = true;
      }
      wr.fields.push_back(*field);
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

  if (have_manual) {
    wr.info.arg_count = manual_max + 1;
  } else {
    wr.info.arg_count = auto_next;
  }
  return wr;
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
      // long double
      return std::vformat(pattern, std::make_format_args(*r));
    }
    if (const auto *q = std::get_if<Rational>(&v)) {
      if (spec.empty()) {
        return format_value(v);
      }
      // Числовий spec — через Real.
      const Real as = static_cast<Real>(q->num) / static_cast<Real>(q->den);
      return std::vformat(pattern, std::make_format_args(as));
    }
    if (const auto *s = std::get_if<String>(&v)) {
      if (spec.empty()) {
        return *s;
      }
      return std::vformat(pattern, std::make_format_args(*s));
    }
    // Complex / Array / Record
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
      auto piece = format_one(args[field.index], field.spec);
      if (!piece) {
        return std::unexpected(piece.error());
      }
      out += *piece;
      i = end;
      continue;
    }
    if (fmt[i] == '}') {
      // walk_format уже перевірив коректність → тут лише '}}'
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
