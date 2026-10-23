// Реалізація текстового дампу байткоду.
#include "calc/disasm.hpp"

#include "calc/builtin.hpp"
#include "calc/value.hpp"

#include <cstddef>
#include <format>
#include <string>

namespace calc {
namespace {

[[nodiscard]] std::uint16_t read_u16(const std::vector<std::uint8_t> &code,
                                     std::size_t &ip) {
  const auto lo = code[ip++];
  const auto hi = code[ip++];
  return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

[[nodiscard]] std::string value_debug(const Value &v) {
  if (is_boolean(v)) {
    return std::format("Boolean({})", format_value(v));
  }
  if (is_integer(v)) {
    return std::format("Integer({})", std::get<Integer>(v));
  }
  if (is_rational(v)) {
    return std::format("Rational({})", format_value(v));
  }
  if (is_real(v)) {
    return std::format("Real({})", format_value(v));
  }
  if (is_complex(v)) {
    return std::format("Complex({})", format_value(v));
  }
  if (is_array(v)) {
    return std::format("Array({})", format_value(v));
  }
  if (is_record(v)) {
    return std::format("Record({})", format_value(v));
  }
  return std::format("String({})", format_value(v));
}

[[nodiscard]] std::string escape_string(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (const char c : s) {
    switch (c) {
    case '\n':
      out += "\\n";
      break;
    case '\t':
      out += "\\t";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

/// Скільки байтів займає інструкція, починаючи з addr (байт опкоду вже
/// відомий).
[[nodiscard]] std::size_t
instruction_size(OpCode op, const std::vector<std::uint8_t> &code,
                 std::size_t addr) {
  switch (op) {
  case OpCode::LoadConst:
  case OpCode::LoadLocal:
  case OpCode::StoreLocal:
  case OpCode::Jump:
  case OpCode::JumpIfFalse:
  case OpCode::MakeArray:
  case OpCode::LoadField:
  case OpCode::StoreField:
    return 3; // op + u16
  case OpCode::PrintFmt:
  case OpCode::Call:
  case OpCode::CallBuiltin:
    return 4; // op + u16 + u8
  case OpCode::MakeRecord: {
    if (addr + 2 >= code.size()) {
      return 3;
    }
    const auto lo = code[addr + 1];
    const auto hi = code[addr + 2];
    const auto count =
        static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
    return 3 + static_cast<std::size_t>(count) * 2;
  }
  case OpCode::MakeIter:
    return 2; // op + u8
  default:
    return 1;
  }
}

[[nodiscard]] std::string format_bytes(const std::vector<std::uint8_t> &code,
                                       std::size_t addr, std::size_t size) {
  std::string s;
  for (std::size_t i = 0; i < size; ++i) {
    if (i > 0) {
      s.push_back(' ');
    }
    if (addr + i < code.size()) {
      s += std::format("{:02X}", code[addr + i]);
    } else {
      s += "??";
    }
  }
  return s;
}

/// Фіксована ширина колонки (мін. у `std::format` не обрізає — робимо самі).
[[nodiscard]] std::string fit_col(std::string_view s, std::size_t width) {
  if (s.size() > width) {
    if (width == 0) {
      return {};
    }
    if (width == 1) {
      return "…";
    }
    std::string out(s.substr(0, width - 1));
    out += "…";
    return out;
  }
  std::string out(s);
  out.append(width - s.size(), ' ');
  return out;
}

// Ширини колонок дампу коду (символи, не байти UTF-8).
constexpr std::size_t kAddrW = 4;
constexpr std::size_t kBytesW = 20; // до 7 байт (MakeRecord n=2); довше — «…»
constexpr std::size_t kOpcodeNumW = 2;
constexpr std::size_t kNameW = 12; // JumpIfFalse
constexpr std::size_t kOperandsW = 16;

void dump_opcode_legend(std::ostream &out) {
  out << "# таблиця опкодів (число = байт у коді):\n";
  const OpCode all[] = {
      OpCode::Halt,       OpCode::LoadConst,  OpCode::LoadLocal,
      OpCode::StoreLocal, OpCode::Neg,        OpCode::Pos,
      OpCode::Add,
      OpCode::Sub,        OpCode::Mul,        OpCode::Div,
      OpCode::Mod,        OpCode::Pow,        OpCode::Eq,
      OpCode::Ne,         OpCode::Lt,         OpCode::Le,
      OpCode::Gt,         OpCode::Ge,         OpCode::Not,
      OpCode::And,        OpCode::Or,         OpCode::Print,
      OpCode::PrintFmt,   OpCode::Jump,       OpCode::JumpIfFalse,
      OpCode::Call,       OpCode::Return,     OpCode::CallBuiltin,
      OpCode::MakeArray,  OpCode::MakeRecord, OpCode::LoadIndex,
      OpCode::StoreIndex, OpCode::LoadField,  OpCode::StoreField,
      OpCode::Pop,        OpCode::MakeIter,
  };
  constexpr std::size_t n = sizeof(all) / sizeof(all[0]);
  for (std::size_t i = 0; i < n; ++i) {
    if (i % 4 == 0) {
      if (i > 0) {
        out << '\n';
      }
      out << "  ";
    } else {
      out << "  ";
    }
    out << std::format("{:>2}={:<{}}", static_cast<unsigned>(all[i]),
                       opcode_name(all[i]), kNameW);
  }
  out << "\n\n";
}

} // namespace

std::string_view opcode_name(OpCode op) {
  switch (op) {
  case OpCode::Halt:
    return "Halt";
  case OpCode::LoadConst:
    return "LoadConst";
  case OpCode::LoadLocal:
    return "LoadLocal";
  case OpCode::StoreLocal:
    return "StoreLocal";
  case OpCode::Neg:
    return "Neg";
  case OpCode::Pos:
    return "Pos";
  case OpCode::Add:
    return "Add";
  case OpCode::Sub:
    return "Sub";
  case OpCode::Mul:
    return "Mul";
  case OpCode::Div:
    return "Div";
  case OpCode::Mod:
    return "Mod";
  case OpCode::Pow:
    return "Pow";
  case OpCode::Eq:
    return "Eq";
  case OpCode::Ne:
    return "Ne";
  case OpCode::Lt:
    return "Lt";
  case OpCode::Le:
    return "Le";
  case OpCode::Gt:
    return "Gt";
  case OpCode::Ge:
    return "Ge";
  case OpCode::Not:
    return "Not";
  case OpCode::And:
    return "And";
  case OpCode::Or:
    return "Or";
  case OpCode::Print:
    return "Print";
  case OpCode::PrintFmt:
    return "PrintFmt";
  case OpCode::Jump:
    return "Jump";
  case OpCode::JumpIfFalse:
    return "JumpIfFalse";
  case OpCode::Call:
    return "Call";
  case OpCode::Return:
    return "Return";
  case OpCode::CallBuiltin:
    return "CallBuiltin";
  case OpCode::MakeArray:
    return "MakeArray";
  case OpCode::MakeRecord:
    return "MakeRecord";
  case OpCode::LoadIndex:
    return "LoadIndex";
  case OpCode::StoreIndex:
    return "StoreIndex";
  case OpCode::LoadField:
    return "LoadField";
  case OpCode::StoreField:
    return "StoreField";
  case OpCode::Pop:
    return "Pop";
  case OpCode::MakeIter:
    return "MakeIter";
  }
  return "???";
}

namespace {

/// Службові імена, які виділяє компілятор (не `let` користувача).
[[nodiscard]] bool is_compiler_temp(std::string_view name) {
  return name.starts_with("__");
}

[[nodiscard]] std::string local_name_at(const Chunk &chunk, std::uint16_t i) {
  if (i < chunk.local_names.size() && !chunk.local_names[i].empty()) {
    return chunk.local_names[i];
  }
  return "?";
}

void dump_local_row(std::ostream &out, std::uint16_t i, std::string_view name,
                    std::span<const Value> local_values, bool have_values) {
  if (have_values && i < local_values.size()) {
    const auto &v = local_values[i];
    out << std::format("  [{:>2}]  {:<12}  {:<8}  {}\n", i, name,
                       value_type_name(v), format_value(v));
  } else if (have_values) {
    out << std::format("  [{:>2}]  {:<12}  {:<8}  {}\n", i, name, "?",
                       "<немає>");
  } else {
    out << std::format("  [{:>2}]  {}\n", i, name);
  }
}

} // namespace

void dump_chunk(const Chunk &chunk, std::ostream &out,
                std::span<const Value> local_values, bool show_temps) {
  std::uint16_t temp_count = 0;
  for (std::uint16_t i = 0; i < chunk.local_count; ++i) {
    if (is_compiler_temp(local_name_at(chunk, i))) {
      ++temp_count;
    }
  }
  const auto user_count =
      static_cast<std::uint16_t>(chunk.local_count - temp_count);
  const bool have_values = !local_values.empty();

  out << "=== змінні (local_count = " << chunk.local_count;
  if (temp_count > 0) {
    out << ", користувацьких = " << user_count;
  }
  out << ") ===\n";

  if (user_count == 0) {
    out << (chunk.local_count == 0 ? "  (немає)\n"
                                   : "  (немає користувацьких)\n");
  } else {
    if (have_values) {
      out << std::format("  {:<4}  {:<12}  {:<8}  {}\n", "slot", "name", "type",
                         "value");
      out << std::format("  {:-<4}  {:-<12}  {:-<8}  {:-<16}\n", "", "", "",
                         "");
    }
    for (std::uint16_t i = 0; i < chunk.local_count; ++i) {
      const auto name = local_name_at(chunk, i);
      if (is_compiler_temp(name)) {
        continue;
      }
      dump_local_row(out, i, name, local_values, have_values);
    }
  }
  if (temp_count > 0 && !show_temps) {
    out << std::format("  (сховано {} службових слотів for-in: __it/__i/__e — "
                       "--dump-temps)\n",
                       temp_count);
  } else if (temp_count > 0 && show_temps) {
    out << "\n=== службові слоти for-in (" << temp_count
        << ": __it/__i/__e) ===\n";
    if (have_values) {
      out << std::format("  {:<4}  {:<12}  {:<8}  {}\n", "slot", "name", "type",
                         "value");
      out << std::format("  {:-<4}  {:-<12}  {:-<8}  {:-<16}\n", "", "", "",
                         "");
    }
    for (std::uint16_t i = 0; i < chunk.local_count; ++i) {
      const auto name = local_name_at(chunk, i);
      if (!is_compiler_temp(name)) {
        continue;
      }
      dump_local_row(out, i, name, local_values, have_values);
    }
  }

  out << "\n=== функції (" << chunk.functions.size() << ") ===\n";
  if (chunk.functions.empty()) {
    out << "  (немає)\n";
  } else {
    for (std::size_t i = 0; i < chunk.functions.size(); ++i) {
      const auto &f = chunk.functions[i];
      out << std::format("  [{:>2}] {} arity={} locals={} entry={:04}\n", i,
                         f.name, f.arity, f.local_count, f.entry);
    }
  }

  out << "\n=== константи (" << chunk.constants.size() << ") ===\n";
  if (chunk.constants.empty()) {
    out << "  (немає)\n";
  } else {
    for (std::size_t i = 0; i < chunk.constants.size(); ++i) {
      out << std::format("  [{:>2}] {}\n", i, value_debug(chunk.constants[i]));
    }
  }

  out << "\n=== імена полів (" << chunk.names.size() << ") ===\n";
  if (chunk.names.empty()) {
    out << "  (немає)\n";
  } else {
    for (std::size_t i = 0; i < chunk.names.size(); ++i) {
      out << std::format("  [{:>2}] {}\n", i, chunk.names[i]);
    }
  }

  out << "\n=== формати (" << chunk.formats.size() << ") ===\n";
  if (chunk.formats.empty()) {
    out << "  (немає)\n";
  } else {
    for (std::size_t i = 0; i < chunk.formats.size(); ++i) {
      out << std::format("  [{:>2}] {}\n", i, escape_string(chunk.formats[i]));
    }
  }

  out << "\n=== код (" << chunk.code.size() << " байт) ===\n";
  dump_opcode_legend(out);

  // Колонки: addr | bytes | # | name | operands | ; comment
  out << "  " << fit_col("addr", kAddrW) << "  " << fit_col("bytes", kBytesW)
      << "  " << fit_col(std::format("{:>{}}", "#", kOpcodeNumW), kOpcodeNumW)
      << "  " << fit_col("name", kNameW) << "  "
      << fit_col("operands", kOperandsW) << "  comment\n";
  out << "  " << std::string(kAddrW, '-') << "  " << std::string(kBytesW, '-')
      << "  " << std::string(kOpcodeNumW, '-') << "  "
      << std::string(kNameW, '-') << "  " << std::string(kOperandsW, '-')
      << "  " << std::string(20, '-') << '\n';

  std::size_t ip = 0;
  while (ip < chunk.code.size()) {
    const auto addr = ip;
    const auto raw = chunk.code[ip];
    const auto op = static_cast<OpCode>(raw);
    const auto size = instruction_size(op, chunk.code, addr);
    ++ip;

    std::string operands;
    std::string comment;

    switch (op) {
    case OpCode::LoadConst: {
      const auto idx = read_u16(chunk.code, ip);
      operands = std::format("{}", idx);
      if (idx < chunk.constants.size()) {
        comment = value_debug(chunk.constants[idx]);
      }
      break;
    }
    case OpCode::LoadLocal:
    case OpCode::StoreLocal: {
      const auto slot = read_u16(chunk.code, ip);
      operands = std::format("{}", slot);
      if (slot < chunk.local_names.size() && !chunk.local_names[slot].empty()) {
        comment = chunk.local_names[slot];
      }
      break;
    }
    case OpCode::PrintFmt: {
      const auto fmt_idx = read_u16(chunk.code, ip);
      const auto argc = chunk.code[ip++];
      operands =
          std::format("fmt={} args={}", fmt_idx, static_cast<unsigned>(argc));
      if (fmt_idx < chunk.formats.size()) {
        comment = escape_string(chunk.formats[fmt_idx]);
      }
      break;
    }
    case OpCode::Jump:
    case OpCode::JumpIfFalse: {
      const auto target = read_u16(chunk.code, ip);
      operands = std::format("-> {}", target);
      comment = std::format("addr {:04}", target);
      break;
    }
    case OpCode::Call: {
      const auto idx = read_u16(chunk.code, ip);
      const auto argc = chunk.code[ip++];
      operands =
          std::format("fun={} args={}", idx, static_cast<unsigned>(argc));
      if (idx < chunk.functions.size()) {
        comment = chunk.functions[idx].name;
      }
      break;
    }
    case OpCode::CallBuiltin: {
      const auto idx = read_u16(chunk.code, ip);
      const auto argc = chunk.code[ip++];
      operands = std::format("id={} args={}", idx, static_cast<unsigned>(argc));
      comment = std::string(builtin_name(static_cast<BuiltinId>(idx)));
      break;
    }
    case OpCode::MakeArray: {
      const auto n = read_u16(chunk.code, ip);
      operands = std::format("n={}", n);
      break;
    }
    case OpCode::MakeRecord: {
      const auto n = read_u16(chunk.code, ip);
      operands = std::format("n={}", n);
      for (std::uint16_t i = 0; i < n; ++i) {
        const auto ni = read_u16(chunk.code, ip);
        if (i > 0) {
          comment += ",";
        }
        if (ni < chunk.names.size()) {
          comment += chunk.names[ni];
        } else {
          comment += "?";
        }
      }
      break;
    }
    case OpCode::LoadField:
    case OpCode::StoreField: {
      const auto ni = read_u16(chunk.code, ip);
      operands = std::format("{}", ni);
      if (ni < chunk.names.size()) {
        comment = chunk.names[ni];
      }
      break;
    }
    case OpCode::MakeIter: {
      const auto mode = chunk.code[ip++];
      operands = std::format("mode={}", static_cast<unsigned>(mode));
      break;
    }
    default:
      break;
    }

    const auto bytes = format_bytes(chunk.code, addr, size);
    const auto comment_col =
        comment.empty() ? std::string{} : std::format("; {}", comment);

    out << "  " << fit_col(std::format("{:04}", addr), kAddrW) << "  "
        << fit_col(bytes, kBytesW) << "  "
        << fit_col(
               std::format("{:>{}}", static_cast<unsigned>(raw), kOpcodeNumW),
               kOpcodeNumW)
        << "  " << fit_col(opcode_name(op), kNameW) << "  "
        << fit_col(operands, kOperandsW) << "  " << comment_col << '\n';
  }
}

} // namespace calc
