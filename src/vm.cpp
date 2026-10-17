// Віртуальна машина: виконання плаского байткоду.
#include "calc/vm.hpp"

#include "calc/builtin.hpp"
#include "calc/format.hpp"
#include "calc/value.hpp"

#include <map>
#include <string>
#include <vector>

namespace calc {
namespace {

struct Frame {
  std::size_t return_ip{0};
  std::vector<Value> locals;
};

[[nodiscard]] std::uint16_t read_u16(const std::vector<std::uint8_t> &code,
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

  while (ip < chunk.code.size()) {
    const auto op = static_cast<OpCode>(chunk.code[ip++]);
    switch (op) {
    case OpCode::Halt:
      return ExecSuccess{std::move(frames.front().locals)};

    case OpCode::LoadConst: {
      const auto idx = read_u16(chunk.code, ip);
      stack.push_back(chunk.constants[idx]);
      break;
    }

    case OpCode::LoadLocal: {
      const auto slot = read_u16(chunk.code, ip);
      stack.push_back(frames.back().locals[slot]);
      break;
    }

    case OpCode::StoreLocal: {
      const auto slot = read_u16(chunk.code, ip);
      frames.back().locals[slot] = std::move(stack.back());
      stack.pop_back();
      break;
    }

    case OpCode::Neg: {
      auto r = negate(stack.back());
      if (!r) {
        return runtime_error(r.error(), "Помилка унарного мінуса");
      }
      stack.back() = *r;
      break;
    }

    case OpCode::Add: {
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      auto r = add(a, b);
      if (!r) {
        return runtime_error(r.error(), "Помилка додавання");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Sub: {
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      auto r = sub(a, b);
      if (!r) {
        return runtime_error(r.error(), "Помилка віднімання");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Mul: {
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      auto r = mul(a, b);
      if (!r) {
        return runtime_error(r.error(), "Помилка множення");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Div: {
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      auto r = div(a, b);
      if (!r) {
        return runtime_error(r.error(), "Помилка ділення");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Mod: {
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      auto r = mod(a, b);
      if (!r) {
        return runtime_error(r.error(), "Помилка остачі");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::Pow: {
      auto b = std::move(stack.back());
      stack.pop_back();
      auto a = std::move(stack.back());
      stack.pop_back();
      auto r = pow_op(a, b);
      if (!r) {
        return runtime_error(r.error(), "Помилка степеня");
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
      auto r = logical_not(stack.back());
      if (!r) {
        return runtime_error(r.error(), "Помилка not");
      }
      stack.back() = *r;
      break;
    }

    case OpCode::Print: {
      print_value(out, stack.back());
      out << '\n';
      stack.pop_back();
      break;
    }

    case OpCode::PrintFmt: {
      const auto fmt_idx = read_u16(chunk.code, ip);
      const auto arg_count = chunk.code[ip++];
      std::vector<Value> args(arg_count);
      for (int i = static_cast<int>(arg_count) - 1; i >= 0; --i) {
        args[static_cast<std::size_t>(i)] = std::move(stack.back());
        stack.pop_back();
      }
      auto text = format_print(chunk.formats[fmt_idx], args);
      if (!text) {
        return runtime_error(text.error(), "Помилка форматування print");
      }
      out << *text << '\n';
      break;
    }

    case OpCode::Jump: {
      const auto target = read_u16(chunk.code, ip);
      ip = target;
      break;
    }

    case OpCode::JumpIfFalse: {
      const auto target = read_u16(chunk.code, ip);
      auto cond = std::move(stack.back());
      stack.pop_back();
      if (!is_boolean(cond)) {
        return runtime_error(ErrorCode::InvalidOperation,
                             "Умова має бути Boolean");
      }
      if (!std::get<Boolean>(cond)) {
        ip = target;
      }
      break;
    }

    case OpCode::Call: {
      const auto func_idx = read_u16(chunk.code, ip);
      const auto argc = chunk.code[ip++];
      if (func_idx >= chunk.functions.size()) {
        return runtime_error(ErrorCode::UnknownFunction, "Невірна функція");
      }
      const auto &proto = chunk.functions[func_idx];
      if (argc != proto.arity) {
        return runtime_error(ErrorCode::ArityMismatch,
                             "Невірна кількість аргументів");
      }
      Frame frame;
      frame.return_ip = ip;
      frame.locals.resize(proto.local_count);
      for (int i = static_cast<int>(argc) - 1; i >= 0; --i) {
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
      auto value = std::move(stack.back());
      stack.pop_back();
      const auto return_ip = frames.back().return_ip;
      frames.pop_back();
      ip = return_ip;
      stack.push_back(std::move(value));
      break;
    }

    case OpCode::CallBuiltin: {
      const auto bid = static_cast<BuiltinId>(read_u16(chunk.code, ip));
      const auto argc = chunk.code[ip++];
      std::vector<Value> args(argc);
      for (int i = static_cast<int>(argc) - 1; i >= 0; --i) {
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
      const auto count = read_u16(chunk.code, ip);
      std::vector<Value> items(count);
      for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
        items[static_cast<std::size_t>(i)] = std::move(stack.back());
        stack.pop_back();
      }
      stack.push_back(Value{make_array(std::move(items))});
      break;
    }

    case OpCode::MakeRecord: {
      const auto count = read_u16(chunk.code, ip);
      std::vector<std::uint16_t> name_idxs(count);
      for (std::uint16_t i = 0; i < count; ++i) {
        name_idxs[i] = read_u16(chunk.code, ip);
      }
      std::map<std::string, Value> fields;
      for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
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
      const auto ni = read_u16(chunk.code, ip);
      if (ni >= chunk.names.size()) {
        return runtime_error(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
      }
      auto rec = std::move(stack.back());
      stack.pop_back();
      auto r = record_get(rec, chunk.names[ni]);
      if (!r) {
        return runtime_error(r.error(), "Помилка читання поля");
      }
      stack.push_back(*r);
      break;
    }

    case OpCode::StoreField: {
      const auto ni = read_u16(chunk.code, ip);
      if (ni >= chunk.names.size()) {
        return runtime_error(ErrorCode::UnexpectedToken, "Невірне імʼя поля");
      }
      auto value = std::move(stack.back());
      stack.pop_back();
      auto rec = std::move(stack.back());
      stack.pop_back();
      auto r = record_set(rec, chunk.names[ni], std::move(value));
      if (!r) {
        return runtime_error(r.error(), "Помилка запису поля");
      }
      break;
    }

    case OpCode::Pop: {
      if (stack.empty()) {
        return runtime_error(ErrorCode::UnexpectedToken, "Порожній стек (Pop)");
      }
      stack.pop_back();
      break;
    }

    case OpCode::MakeIter: {
      const auto mode = chunk.code[ip++];
      auto coll = std::move(stack.back());
      stack.pop_back();
      auto r = make_iter_items(coll, mode);
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
  return ExecSuccess{std::move(frames.front().locals)};
}

ExecResult execute(const Chunk &chunk, std::ostream &out) {
  auto r = execute_with_locals(chunk, out);
  if (!r) {
    return std::unexpected(r.error());
  }
  return {};
}

} // namespace calc
