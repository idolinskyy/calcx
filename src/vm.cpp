// Віртуальна машина: виконання плаского байткоду.
#include "calc/vm.hpp"

#include "calc/builtin.hpp"
#include "calc/format.hpp"
#include "calc/value.hpp"

#include <expected>
#include <map>
#include <string>
#include <vector>

namespace calc {
namespace {

/// Глибина кадрів Call (включно з головним кадром).
constexpr std::size_t kMaxCallDepth = 10'000;

struct Frame {
  std::size_t return_ip{0};
  std::vector<Value> locals;
};

[[nodiscard]] std::uint16_t read_u16_unchecked(const std::vector<std::uint8_t> &code,
                                               std::size_t &ip) {
  const auto lo = code[ip++];
  const auto hi = code[ip++];
  return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

} // namespace

ExecStateResult execute_with_locals(const Chunk &chunk, std::ostream &out) {
  std::vector<Frame> frames;
  frames.push_back(Frame{0, std::vector<Value>(chunk.local_count)});
  std::vector<Value> stack;
  std::size_t ip = 0;

  auto runtime_error = [&](ErrorCode code,
                           std::string message) -> ExecStateResult {
    return std::unexpected(Diagnostic{code, {}, std::move(message)});
  };

  auto need_stack = [&](std::size_t n) -> bool {
    return stack.size() >= n;
  };

  auto read_u16 = [&]() -> std::expected<std::uint16_t, Diagnostic> {
    if (ip + 1 >= chunk.code.size()) {
      return std::unexpected(
          Diagnostic{ErrorCode::UnexpectedToken, {},
                     "Обрізаний операнд інструкції"});
    }
    return read_u16_unchecked(chunk.code, ip);
  };

  auto read_u8 = [&]() -> std::expected<std::uint8_t, Diagnostic> {
    if (ip >= chunk.code.size()) {
      return std::unexpected(
          Diagnostic{ErrorCode::UnexpectedToken, {},
                     "Обрізаний операнд інструкції"});
    }
    return chunk.code[ip++];
  };

  while (ip < chunk.code.size()) {
    const auto op = static_cast<OpCode>(chunk.code[ip++]);
    switch (op) {
    case OpCode::Halt:
      return ExecSuccess{std::move(frames.front().locals)};

    case OpCode::LoadConst: {
      auto idx = read_u16();
      if (!idx) {
        return std::unexpected(idx.error());
      }
      if (*idx >= chunk.constants.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Індекс константи поза межами");
      }
      stack.push_back(chunk.constants[*idx]);
      break;
    }

    case OpCode::LoadLocal: {
      auto slot = read_u16();
      if (!slot) {
        return std::unexpected(slot.error());
      }
      if (*slot >= frames.back().locals.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Слот локалі поза межами");
      }
      stack.push_back(frames.back().locals[*slot]);
      break;
    }

    case OpCode::StoreLocal: {
      auto slot = read_u16();
      if (!slot) {
        return std::unexpected(slot.error());
      }
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      if (*slot >= frames.back().locals.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Слот локалі поза межами");
      }
      frames.back().locals[*slot] = std::move(stack.back());
      stack.pop_back();
      break;
    }

    case OpCode::Neg: {
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto r = negate(stack.back());
      if (!r) {
        return runtime_error(r.error(), "Помилка унарного мінуса");
      }
      stack.back() = *r;
      break;
    }

    case OpCode::Pos: {
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto r = posit(stack.back());
      if (!r) {
        return runtime_error(r.error(), "Помилка унарного плюса");
      }
      stack.back() = *r;
      break;
    }

    case OpCode::Add:
    case OpCode::Sub:
    case OpCode::Mul:
    case OpCode::Div:
    case OpCode::Mod:
    case OpCode::Pow: {
      if (!need_stack(2)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      ArithResult r;
      const char *msg = "Помилка операції";
      switch (op) {
      case OpCode::Add:
        r = add(a, b);
        msg = "Помилка додавання";
        break;
      case OpCode::Sub:
        r = sub(a, b);
        msg = "Помилка віднімання";
        break;
      case OpCode::Mul:
        r = mul(a, b);
        msg = "Помилка множення";
        break;
      case OpCode::Div:
        r = div(a, b);
        msg = "Помилка ділення";
        break;
      case OpCode::Mod:
        r = mod(a, b);
        msg = "Помилка остачі";
        break;
      case OpCode::Pow:
        r = pow_op(a, b);
        msg = "Помилка степеня";
        break;
      default:
        r = std::unexpected(ErrorCode::UnexpectedToken);
        break;
      }
      if (!r) {
        return runtime_error(r.error(), msg);
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Eq:
    case OpCode::Ne:
    case OpCode::Lt:
    case OpCode::Le:
    case OpCode::Gt:
    case OpCode::Ge:
    case OpCode::And:
    case OpCode::Or: {
      if (!need_stack(2)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      ArithResult r;
      switch (op) {
      case OpCode::Eq:
        r = eq(a, b);
        break;
      case OpCode::Ne:
        r = ne(a, b);
        break;
      case OpCode::Lt:
        r = lt(a, b);
        break;
      case OpCode::Le:
        r = le(a, b);
        break;
      case OpCode::Gt:
        r = gt(a, b);
        break;
      case OpCode::Ge:
        r = ge(a, b);
        break;
      case OpCode::And:
        r = logical_and(a, b);
        break;
      case OpCode::Or:
        r = logical_or(a, b);
        break;
      default:
        r = std::unexpected(ErrorCode::UnexpectedToken);
        break;
      }
      if (!r) {
        return runtime_error(r.error(), "Помилка операції");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Not: {
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto r = logical_not(stack.back());
      if (!r) {
        return runtime_error(r.error(), "Помилка not");
      }
      stack.back() = *r;
      break;
    }

    case OpCode::Print: {
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      print_value(out, stack.back());
      out << '\n';
      stack.pop_back();
      break;
    }

    case OpCode::PrintFmt: {
      auto fmt_idx = read_u16();
      if (!fmt_idx) {
        return std::unexpected(fmt_idx.error());
      }
      auto arg_count = read_u8();
      if (!arg_count) {
        return std::unexpected(arg_count.error());
      }
      if (*fmt_idx >= chunk.formats.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Індекс формату поза межами");
      }
      if (!need_stack(*arg_count)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      std::vector<Value> args(*arg_count);
      for (int i = static_cast<int>(*arg_count) - 1; i >= 0; --i) {
        args[static_cast<std::size_t>(i)] = std::move(stack.back());
        stack.pop_back();
      }
      auto text = format_print(chunk.formats[*fmt_idx], args);
      if (!text) {
        return runtime_error(text.error(), "Помилка форматування print");
      }
      out << *text << '\n';
      break;
    }

    case OpCode::Jump: {
      auto target = read_u16();
      if (!target) {
        return std::unexpected(target.error());
      }
      if (*target > chunk.code.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Ціль Jump поза межами коду");
      }
      ip = *target;
      break;
    }

    case OpCode::JumpIfFalse: {
      auto target = read_u16();
      if (!target) {
        return std::unexpected(target.error());
      }
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      if (*target > chunk.code.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Ціль JumpIfFalse поза межами коду");
      }
      auto cond = std::move(stack.back());
      stack.pop_back();
      if (!is_boolean(cond)) {
        return runtime_error(ErrorCode::InvalidOperation,
                             "Умова має бути Boolean");
      }
      if (!std::get<Boolean>(cond)) {
        ip = *target;
      }
      break;
    }

    case OpCode::Call: {
      auto func_idx = read_u16();
      if (!func_idx) {
        return std::unexpected(func_idx.error());
      }
      auto argc = read_u8();
      if (!argc) {
        return std::unexpected(argc.error());
      }
      if (*func_idx >= chunk.functions.size()) {
        return runtime_error(ErrorCode::UnknownFunction, "Невірна функція");
      }
      const auto &proto = chunk.functions[*func_idx];
      if (*argc != proto.arity) {
        return runtime_error(ErrorCode::ArityMismatch,
                             "Невірна кількість аргументів");
      }
      if (proto.local_count < proto.arity) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "local_count менший за arity");
      }
      if (proto.entry > chunk.code.size()) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "entry функції поза межами коду");
      }
      if (!need_stack(*argc)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      if (frames.size() >= kMaxCallDepth) {
        return runtime_error(ErrorCode::StackOverflow,
                             "Перевищено ліміт глибини викликів");
      }
      Frame frame;
      frame.return_ip = ip;
      frame.locals.resize(proto.local_count);
      for (int i = static_cast<int>(*argc) - 1; i >= 0; --i) {
        frame.locals[static_cast<std::size_t>(i)] = std::move(stack.back());
        stack.pop_back();
      }
      frames.push_back(std::move(frame));
      ip = proto.entry;
      break;
    }

    case OpCode::Return: {
      if (frames.size() <= 1) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "return поза функцією");
      }
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto value = std::move(stack.back());
      stack.pop_back();
      const auto return_ip = frames.back().return_ip;
      frames.pop_back();
      ip = return_ip;
      stack.push_back(std::move(value));
      break;
    }

    case OpCode::CallBuiltin: {
      auto bid_raw = read_u16();
      if (!bid_raw) {
        return std::unexpected(bid_raw.error());
      }
      auto argc = read_u8();
      if (!argc) {
        return std::unexpected(argc.error());
      }
      if (!need_stack(*argc)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      const auto bid = static_cast<BuiltinId>(*bid_raw);
      std::vector<Value> args(*argc);
      for (int i = static_cast<int>(*argc) - 1; i >= 0; --i) {
        args[static_cast<std::size_t>(i)] = std::move(stack.back());
        stack.pop_back();
      }
      auto r = call_builtin(bid, args);
      if (!r) {
        return runtime_error(r.error(), "Помилка builtin");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::MakeArray: {
      auto count = read_u16();
      if (!count) {
        return std::unexpected(count.error());
      }
      if (!need_stack(*count)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      std::vector<Value> items(*count);
      for (int i = static_cast<int>(*count) - 1; i >= 0; --i) {
        items[static_cast<std::size_t>(i)] = std::move(stack.back());
        stack.pop_back();
      }
      stack.push_back(Value{make_array(std::move(items))});
      break;
    }

    case OpCode::MakeRecord: {
      auto count = read_u16();
      if (!count) {
        return std::unexpected(count.error());
      }
      std::vector<std::uint16_t> name_idxs(*count);
      for (std::uint16_t i = 0; i < *count; ++i) {
        auto ni = read_u16();
        if (!ni) {
          return std::unexpected(ni.error());
        }
        name_idxs[i] = *ni;
      }
      if (!need_stack(*count)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      std::map<std::string, Value> fields;
      for (int i = static_cast<int>(*count) - 1; i >= 0; --i) {
        const auto ni = name_idxs[static_cast<std::size_t>(i)];
        if (ni >= chunk.names.size()) {
          return runtime_error(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
        }
        fields[chunk.names[ni]] = std::move(stack.back());
        stack.pop_back();
      }
      stack.push_back(Value{make_record(std::move(fields))});
      break;
    }

    case OpCode::LoadIndex: {
      if (!need_stack(2)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto index = std::move(stack.back());
      stack.pop_back();
      auto arr = std::move(stack.back());
      stack.pop_back();
      auto r = array_get(arr, index);
      if (!r) {
        return runtime_error(r.error(), "Помилка індексації");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::StoreIndex: {
      if (!need_stack(3)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto value = std::move(stack.back());
      stack.pop_back();
      auto index = std::move(stack.back());
      stack.pop_back();
      auto arr = std::move(stack.back());
      stack.pop_back();
      auto r = array_set(arr, index, std::move(value));
      if (!r) {
        return runtime_error(r.error(), "Помилка запису в масив");
      }
      break;
    }

    case OpCode::LoadField: {
      auto ni = read_u16();
      if (!ni) {
        return std::unexpected(ni.error());
      }
      if (*ni >= chunk.names.size()) {
        return runtime_error(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
      }
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto rec = std::move(stack.back());
      stack.pop_back();
      auto r = record_get(rec, chunk.names[*ni]);
      if (!r) {
        return runtime_error(r.error(), "Помилка читання поля");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::StoreField: {
      auto ni = read_u16();
      if (!ni) {
        return std::unexpected(ni.error());
      }
      if (*ni >= chunk.names.size()) {
        return runtime_error(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
      }
      if (!need_stack(2)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto value = std::move(stack.back());
      stack.pop_back();
      auto rec = std::move(stack.back());
      stack.pop_back();
      auto r = record_set(rec, chunk.names[*ni], std::move(value));
      if (!r) {
        return runtime_error(r.error(), "Помилка запису поля");
      }
      break;
    }

    case OpCode::Pop: {
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken, "Порожній стек (Pop)");
      }
      stack.pop_back();
      break;
    }

    case OpCode::MakeIter: {
      auto mode = read_u8();
      if (!mode) {
        return std::unexpected(mode.error());
      }
      if (!need_stack(1)) {
        return runtime_error(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto coll = std::move(stack.back());
      stack.pop_back();
      auto r = make_iter_items(coll, *mode);
      if (!r) {
        return runtime_error(r.error(), "for-in очікує масив або запис");
      }
      stack.push_back(*r);
      break;
    }

    default:
      return runtime_error(ErrorCode::UnexpectedToken, "Невідомий опкод");
    }
  }
  return runtime_error(ErrorCode::UnexpectedToken,
                       "Код завершився без Halt");
}

ExecResult execute(const Chunk &chunk, std::ostream &out) {
  auto r = execute_with_locals(chunk, out);
  if (!r) {
    return std::unexpected(r.error());
  }
  return {};
}

} // namespace calc
