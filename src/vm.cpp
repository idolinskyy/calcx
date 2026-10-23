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

constexpr std::size_t kMaxCallDepth = 10'000;

[[nodiscard]] std::uint16_t read_u16_unchecked(const std::vector<std::uint8_t> &code,
                                               std::size_t &ip) {
  const auto lo = code[ip++];
  const auto hi = code[ip++];
  return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

} // namespace

void vm_reset(VmState &st, const Chunk &chunk) {
  st.frames.clear();
  st.frames.push_back(VmFrame{0, std::vector<Value>(chunk.local_count)});
  st.stack.clear();
  st.ip = 0;
}

std::expected<VmStepStatus, Diagnostic>
vm_step(VmState &st, const Chunk &chunk, std::ostream &out) {
  auto fail = [&](ErrorCode code, std::string message)
      -> std::expected<VmStepStatus, Diagnostic> {
    return std::unexpected(Diagnostic{code, {}, std::move(message)});
  };

  auto need_stack = [&](std::size_t n) -> bool {
    return st.stack.size() >= n;
  };

  auto read_u16 = [&]() -> std::expected<std::uint16_t, Diagnostic> {
    if (st.ip + 1 >= chunk.code.size()) {
      return std::unexpected(
          Diagnostic{ErrorCode::UnexpectedToken, {},
                     "Обрізаний операнд інструкції"});
    }
    return read_u16_unchecked(chunk.code, st.ip);
  };

  auto read_u8 = [&]() -> std::expected<std::uint8_t, Diagnostic> {
    if (st.ip >= chunk.code.size()) {
      return std::unexpected(
          Diagnostic{ErrorCode::UnexpectedToken, {},
                     "Обрізаний операнд інструкції"});
    }
    return chunk.code[st.ip++];
  };

  if (st.ip >= chunk.code.size()) {
    return fail(ErrorCode::UnexpectedToken, "Код завершився без Halt");
  }

  const auto op = static_cast<OpCode>(chunk.code[st.ip++]);
    switch (op) {
    case OpCode::Halt:
      return VmStepStatus::Halted;

    case OpCode::LoadConst: {
      auto idx = read_u16();
      if (!idx) {
        return std::unexpected(idx.error());
      }
      if (*idx >= chunk.constants.size()) {
        return fail(ErrorCode::UnexpectedToken,
                             "Індекс константи поза межами");
      }
      st.stack.push_back(chunk.constants[*idx]);
      break;
    }

    case OpCode::LoadLocal: {
      auto slot = read_u16();
      if (!slot) {
        return std::unexpected(slot.error());
      }
      if (*slot >= st.frames.back().locals.size()) {
        return fail(ErrorCode::UnexpectedToken,
                             "Слот локалі поза межами");
      }
      st.stack.push_back(st.frames.back().locals[*slot]);
      break;
    }

    case OpCode::StoreLocal: {
      auto slot = read_u16();
      if (!slot) {
        return std::unexpected(slot.error());
      }
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      if (*slot >= st.frames.back().locals.size()) {
        return fail(ErrorCode::UnexpectedToken,
                             "Слот локалі поза межами");
      }
      st.frames.back().locals[*slot] = std::move(st.stack.back());
      st.stack.pop_back();
      break;
    }

    case OpCode::Neg: {
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto r = negate(st.stack.back());
      if (!r) {
        return fail(r.error(), "Помилка унарного мінуса");
      }
      st.stack.back() = *r;
      break;
    }

    case OpCode::Pos: {
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto r = posit(st.stack.back());
      if (!r) {
        return fail(r.error(), "Помилка унарного плюса");
      }
      st.stack.back() = *r;
      break;
    }

    case OpCode::Add:
    case OpCode::Sub:
    case OpCode::Mul:
    case OpCode::Div:
    case OpCode::Mod:
    case OpCode::Pow: {
      if (!need_stack(2)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto b = std::move(st.stack.back());
      st.stack.pop_back();
      auto a = std::move(st.stack.back());
      st.stack.pop_back();
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
        return fail(r.error(), msg);
      }
      st.stack.push_back(*r);
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
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto b = std::move(st.stack.back());
      st.stack.pop_back();
      auto a = std::move(st.stack.back());
      st.stack.pop_back();
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
        return fail(r.error(), "Помилка операції");
      }
      st.stack.push_back(*r);
      break;
    }

    case OpCode::Not: {
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto r = logical_not(st.stack.back());
      if (!r) {
        return fail(r.error(), "Помилка not");
      }
      st.stack.back() = *r;
      break;
    }

    case OpCode::Print: {
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      print_value(out, st.stack.back());
      out << '\n';
      st.stack.pop_back();
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
        return fail(ErrorCode::UnexpectedToken,
                             "Індекс формату поза межами");
      }
      if (!need_stack(*arg_count)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      std::vector<Value> args(*arg_count);
      for (int i = static_cast<int>(*arg_count) - 1; i >= 0; --i) {
        args[static_cast<std::size_t>(i)] = std::move(st.stack.back());
        st.stack.pop_back();
      }
      auto text = format_print(chunk.formats[*fmt_idx], args);
      if (!text) {
        return fail(text.error(), "Помилка форматування print");
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
        return fail(ErrorCode::UnexpectedToken,
                             "Ціль Jump поза межами коду");
      }
      st.ip = *target;
      break;
    }

    case OpCode::JumpIfFalse: {
      auto target = read_u16();
      if (!target) {
        return std::unexpected(target.error());
      }
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      if (*target > chunk.code.size()) {
        return fail(ErrorCode::UnexpectedToken,
                             "Ціль JumpIfFalse поза межами коду");
      }
      auto cond = std::move(st.stack.back());
      st.stack.pop_back();
      if (!is_boolean(cond)) {
        return fail(ErrorCode::InvalidOperation,
                             "Умова має бути Boolean");
      }
      if (!std::get<Boolean>(cond)) {
        st.ip = *target;
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
        return fail(ErrorCode::UnknownFunction, "Невірна функція");
      }
      const auto &proto = chunk.functions[*func_idx];
      if (*argc != proto.arity) {
        return fail(ErrorCode::ArityMismatch,
                             "Невірна кількість аргументів");
      }
      if (proto.local_count < proto.arity) {
        return fail(ErrorCode::UnexpectedToken,
                             "local_count менший за arity");
      }
      if (proto.entry > chunk.code.size()) {
        return fail(ErrorCode::UnexpectedToken,
                             "entry функції поза межами коду");
      }
      if (!need_stack(*argc)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      if (st.frames.size() >= kMaxCallDepth) {
        return fail(ErrorCode::StackOverflow,
                             "Перевищено ліміт глибини викликів");
      }
      VmFrame frame;
      frame.return_ip = st.ip;
      frame.locals.resize(proto.local_count);
      for (int i = static_cast<int>(*argc) - 1; i >= 0; --i) {
        frame.locals[static_cast<std::size_t>(i)] = std::move(st.stack.back());
        st.stack.pop_back();
      }
      st.frames.push_back(std::move(frame));
      st.ip = proto.entry;
      break;
    }

    case OpCode::Return: {
      if (st.frames.size() <= 1) {
        return fail(ErrorCode::UnexpectedToken,
                             "return поза функцією");
      }
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto value = std::move(st.stack.back());
      st.stack.pop_back();
      const auto return_ip = st.frames.back().return_ip;
      st.frames.pop_back();
      st.ip = return_ip;
      st.stack.push_back(std::move(value));
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
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      const auto bid = static_cast<BuiltinId>(*bid_raw);
      std::vector<Value> args(*argc);
      for (int i = static_cast<int>(*argc) - 1; i >= 0; --i) {
        args[static_cast<std::size_t>(i)] = std::move(st.stack.back());
        st.stack.pop_back();
      }
      auto r = call_builtin(bid, args);
      if (!r) {
        return fail(r.error(), "Помилка builtin");
      }
      st.stack.push_back(*r);
      break;
    }

    case OpCode::MakeArray: {
      auto count = read_u16();
      if (!count) {
        return std::unexpected(count.error());
      }
      if (!need_stack(*count)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      std::vector<Value> items(*count);
      for (int i = static_cast<int>(*count) - 1; i >= 0; --i) {
        items[static_cast<std::size_t>(i)] = std::move(st.stack.back());
        st.stack.pop_back();
      }
      st.stack.push_back(Value{make_array(std::move(items))});
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
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      std::map<std::string, Value> fields;
      for (int i = static_cast<int>(*count) - 1; i >= 0; --i) {
        const auto ni = name_idxs[static_cast<std::size_t>(i)];
        if (ni >= chunk.names.size()) {
          return fail(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
        }
        fields[chunk.names[ni]] = std::move(st.stack.back());
        st.stack.pop_back();
      }
      st.stack.push_back(Value{make_record(std::move(fields))});
      break;
    }

    case OpCode::LoadIndex: {
      if (!need_stack(2)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto index = std::move(st.stack.back());
      st.stack.pop_back();
      auto arr = std::move(st.stack.back());
      st.stack.pop_back();
      auto r = array_get(arr, index);
      if (!r) {
        return fail(r.error(), "Помилка індексації");
      }
      st.stack.push_back(*r);
      break;
    }

    case OpCode::StoreIndex: {
      if (!need_stack(3)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto value = std::move(st.stack.back());
      st.stack.pop_back();
      auto index = std::move(st.stack.back());
      st.stack.pop_back();
      auto arr = std::move(st.stack.back());
      st.stack.pop_back();
      auto r = array_set(arr, index, std::move(value));
      if (!r) {
        return fail(r.error(), "Помилка запису в масив");
      }
      break;
    }

    case OpCode::LoadField: {
      auto ni = read_u16();
      if (!ni) {
        return std::unexpected(ni.error());
      }
      if (*ni >= chunk.names.size()) {
        return fail(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
      }
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto rec = std::move(st.stack.back());
      st.stack.pop_back();
      auto r = record_get(rec, chunk.names[*ni]);
      if (!r) {
        return fail(r.error(), "Помилка читання поля");
      }
      st.stack.push_back(*r);
      break;
    }

    case OpCode::StoreField: {
      auto ni = read_u16();
      if (!ni) {
        return std::unexpected(ni.error());
      }
      if (*ni >= chunk.names.size()) {
        return fail(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
      }
      if (!need_stack(2)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto value = std::move(st.stack.back());
      st.stack.pop_back();
      auto rec = std::move(st.stack.back());
      st.stack.pop_back();
      auto r = record_set(rec, chunk.names[*ni], std::move(value));
      if (!r) {
        return fail(r.error(), "Помилка запису поля");
      }
      break;
    }

    case OpCode::Pop: {
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken, "Порожній стек (Pop)");
      }
      st.stack.pop_back();
      break;
    }

    case OpCode::MakeIter: {
      auto mode = read_u8();
      if (!mode) {
        return std::unexpected(mode.error());
      }
      if (!need_stack(1)) {
        return fail(ErrorCode::UnexpectedToken,
                             "Недостатньо значень на стеку");
      }
      auto coll = std::move(st.stack.back());
      st.stack.pop_back();
      auto r = make_iter_items(coll, *mode);
      if (!r) {
        return fail(r.error(), "for-in очікує масив або запис");
      }
      st.stack.push_back(*r);
      break;
    }

    default:
      return fail(ErrorCode::UnexpectedToken, "Невідомий опкод");
    }

  return VmStepStatus::Continue;
}

ExecStateResult execute_with_locals(const Chunk &chunk, std::ostream &out) {
  VmState st;
  vm_reset(st, chunk);
  while (true) {
    auto step = vm_step(st, chunk, out);
    if (!step) {
      return std::unexpected(step.error());
    }
    if (*step == VmStepStatus::Halted) {
      return ExecSuccess{std::move(st.frames.front().locals)};
    }
  }
}

ExecResult execute(const Chunk &chunk, std::ostream &out) {
  auto r = execute_with_locals(chunk, out);
  if (!r) {
    return std::unexpected(r.error());
  }
  return {};
}

} // namespace calc
