// Компілятор: AST → плаский байткод Chunk.
#include "calc/compiler.hpp"
#include "calc/builtin.hpp"
#include "calc/constants.hpp"
#include "calc/format.hpp"

#include <expected>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace calc {
namespace {

/// Байткод Call / CallBuiltin / PrintFmt кодує argc як u8.
constexpr std::size_t kMaxArgCount = 255;
constexpr std::size_t kMaxU16 = std::numeric_limits<std::uint16_t>::max();

class Compiler {
public:
  [[nodiscard]] CompileResult compile(const Program &program) {
    // 1) Зареєструвати fun на верхньому рівні (для взаємної рекурсії).
    for (const auto &stmt : program.statements) {
      if (const auto *fun = std::get_if<FunStmt>(&stmt->node)) {
        auto r = register_function(*fun);
        if (!r) {
          return std::unexpected(r.error());
        }
      }
    }

    // 2) Головний код (без тіл fun).
    for (const auto &stmt : program.statements) {
      if (std::holds_alternative<FunStmt>(stmt->node)) {
        continue;
      }
      auto r = compile_stmt(*stmt);
      if (!r) {
        return std::unexpected(r.error());
      }
    }
    chunk_.emit(OpCode::Halt);
    if (next_slot_ > kMaxU16) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, {},
                "Забагато локальних змінних (макс. 65535)"));
    }
    chunk_.local_count = static_cast<std::uint16_t>(next_slot_);
    chunk_.local_names = std::move(local_names_);

    // 3) Тіла функцій після Halt (лише через Call).
    for (const auto *fun : pending_funs_) {
      auto r = compile_function(*fun);
      if (!r) {
        return std::unexpected(r.error());
      }
    }
    if (chunk_.code.size() > kMaxU16 + 1) {
      // Адреси інструкцій — u16; розмір може бути 65536 (останній байт @65535).
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, {},
                "Завеликий байткод (адресa > 65535)"));
    }
    return std::move(chunk_);
  }

private:
  /// Контекст активного циклу для break / continue.
  struct LoopCtx {
    std::optional<std::uint16_t> continue_target; ///< відомий одразу (while)
    std::vector<std::size_t> continue_patches;    ///< for: patch після body
    std::vector<std::size_t> break_patches;
  };

  Chunk chunk_;
  /// Стек областей видимості (імʼя → абсолютний слот).
  std::vector<std::unordered_map<std::string, std::uint16_t>> scopes_{{}};
  /// Наступний вільний слот локалі (не зменшується при виході з блоку).
  std::size_t next_slot_{0};
  std::vector<std::string> local_names_;
  std::vector<LoopCtx> loops_;
  std::unordered_map<std::string, std::uint16_t> fun_index_;
  std::vector<const FunStmt *> pending_funs_;
  bool in_function_{false};

  [[nodiscard]] Diagnostic error(ErrorCode code, SourcePos pos,
                                 std::string message) const {
    return Diagnostic{code, pos, std::move(message)};
  }

  void push_scope() { scopes_.emplace_back(); }

  void pop_scope() { scopes_.pop_back(); }

  [[nodiscard]] std::expected<std::uint16_t, Diagnostic>
  code_addr(SourcePos pos) {
    if (chunk_.code.size() > kMaxU16) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, pos,
                "Завеликий байткод (адресa > 65535)"));
    }
    return static_cast<std::uint16_t>(chunk_.code.size());
  }

  [[nodiscard]] std::expected<std::uint16_t, Diagnostic>
  alloc_slot(SourcePos pos) {
    // local_count — u16; слоти 0..65534 (65535 штук).
    if (next_slot_ >= kMaxU16) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, pos,
                "Забагато локальних змінних (макс. 65535)"));
    }
    return static_cast<std::uint16_t>(next_slot_++);
  }

  [[nodiscard]] std::expected<std::uint16_t, Diagnostic>
  add_const(Value v, SourcePos pos) {
    auto idx = chunk_.add_constant(std::move(v));
    if (!idx) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, pos,
                "Забагато констант (макс. 65536)"));
    }
    return *idx;
  }

  [[nodiscard]] std::expected<std::uint16_t, Diagnostic>
  add_fmt(std::string fmt, SourcePos pos) {
    auto idx = chunk_.add_format(std::move(fmt));
    if (!idx) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, pos,
                "Забагато рядків формату (макс. 65536)"));
    }
    return *idx;
  }

  [[nodiscard]] std::expected<std::uint16_t, Diagnostic>
  add_field_name(std::string name, SourcePos pos) {
    auto idx = chunk_.add_name(std::move(name));
    if (!idx) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, pos,
                "Забагато імен полів (макс. 65536)"));
    }
    return *idx;
  }

  /// Еміт Jump/JumpIfFalse з плейсхолдером; повертає зміщення u16 для patch.
  [[nodiscard]] std::size_t emit_jump(OpCode op) {
    chunk_.emit_u16(op, 0);
    return chunk_.code.size() - 2;
  }

  [[nodiscard]] std::expected<void, Diagnostic> patch_jump(std::size_t offset,
                                                           SourcePos pos) {
    auto target = code_addr(pos);
    if (!target) {
      return std::unexpected(target.error());
    }
    patch_u16(chunk_, offset, *target);
    return {};
  }

  void patch_jump_to(std::size_t offset, std::uint16_t target) {
    patch_u16(chunk_, offset, target);
  }

  [[nodiscard]] std::optional<std::uint16_t>
  resolve_local(const std::string &name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
      const auto found = it->find(name);
      if (found != it->end()) {
        return found->second;
      }
    }
    return std::nullopt;
  }

  void declare_local(const std::string &name, std::uint16_t slot) {
    scopes_.back().emplace(name, slot);
    if (local_names_.size() <= slot) {
      local_names_.resize(static_cast<std::size_t>(slot) + 1);
    }
    local_names_[slot] = name;
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  register_function(const FunStmt &fun) {
    if (fun_index_.contains(fun.name) || find_builtin(fun.name) ||
        is_constant_name(fun.name)) {
      return std::unexpected(error(ErrorCode::RedeclaredVariable, fun.name_pos,
                                   "Повторне оголошення функції"));
    }
    if (fun.params.size() > kMaxArgCount) {
      return std::unexpected(error(ErrorCode::ArityMismatch, fun.name_pos,
                                   "Занадто багато параметрів функції (макс. 255)"));
    }
    // Унікальні імена параметрів.
    std::unordered_map<std::string, bool> seen;
    for (const auto &p : fun.params) {
      if (seen.contains(p.name)) {
        return std::unexpected(error(ErrorCode::RedeclaredVariable, p.pos,
                                     "Повторний параметр функції"));
      }
      seen.emplace(p.name, true);
    }
    if (chunk_.functions.size() > kMaxU16) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, fun.name_pos,
                "Забагато функцій (макс. 65536)"));
    }
    FunctionProto proto;
    proto.name = fun.name;
    proto.arity = static_cast<std::uint16_t>(fun.params.size());
    const auto idx = static_cast<std::uint16_t>(chunk_.functions.size());
    chunk_.functions.push_back(std::move(proto));
    fun_index_.emplace(fun.name, idx);
    pending_funs_.push_back(&fun);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_function(const FunStmt &fun) {
    const auto idx = fun_index_.at(fun.name);
    scopes_ = {{}};
    next_slot_ = 0;
    local_names_.clear();
    loops_.clear();
    in_function_ = true;

    for (const auto &p : fun.params) {
      auto slot = alloc_slot(p.pos);
      if (!slot) {
        in_function_ = false;
        return std::unexpected(slot.error());
      }
      declare_local(p.name, *slot);
    }

    auto entry = code_addr(fun.name_pos);
    if (!entry) {
      in_function_ = false;
      return std::unexpected(entry.error());
    }
    chunk_.functions[idx].entry = *entry;

    auto body = compile_stmt(*fun.body);
    if (!body) {
      in_function_ = false;
      return std::unexpected(body.error());
    }

    // Неявний return 0, якщо тіло не завершилось Return.
    auto zero = add_const(Value{Integer{0}}, fun.name_pos);
    if (!zero) {
      in_function_ = false;
      return std::unexpected(zero.error());
    }
    chunk_.emit_u16(OpCode::LoadConst, *zero);
    chunk_.emit(OpCode::Return);

    if (next_slot_ > kMaxU16) {
      in_function_ = false;
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, fun.name_pos,
                "Забагато локальних змінних (макс. 65535)"));
    }
    chunk_.functions[idx].local_count = static_cast<std::uint16_t>(next_slot_);
    chunk_.functions[idx].local_names = std::move(local_names_);
    in_function_ = false;
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic> compile_stmt(const Stmt &stmt) {
    return std::visit(
        [this](const auto &s) -> std::expected<void, Diagnostic> {
          return this->compile_one(s);
        },
        stmt.node);
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const LetStmt &stmt) {
    for (const auto &binding : stmt.bindings) {
      auto init = compile_expr(*binding.init);
      if (!init) {
        return std::unexpected(init.error());
      }
      auto &scope = scopes_.back();
      if (scope.contains(binding.name)) {
        return std::unexpected(error(ErrorCode::RedeclaredVariable,
                                     binding.name_pos,
                                     "Повторне оголошення змінної"));
      }
      auto slot = alloc_slot(binding.name_pos);
      if (!slot) {
        return std::unexpected(slot.error());
      }
      declare_local(binding.name, *slot);
      chunk_.emit_u16(OpCode::StoreLocal, *slot);
    }
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const AssignStmt &stmt) {
    if (const auto *var = std::get_if<VarExpr>(&stmt.target->node)) {
      auto value = compile_expr(*stmt.value);
      if (!value) {
        return std::unexpected(value.error());
      }
      auto slot = resolve_local(var->name);
      if (!slot) {
        return std::unexpected(error(ErrorCode::UndeclaredVariable, var->pos,
                                     "Неоголошена змінна"));
      }
      chunk_.emit_u16(OpCode::StoreLocal, *slot);
      return {};
    }
    if (const auto *idx = std::get_if<IndexExpr>(&stmt.target->node)) {
      auto obj = compile_expr(*idx->object);
      if (!obj) {
        return std::unexpected(obj.error());
      }
      auto index = compile_expr(*idx->index);
      if (!index) {
        return std::unexpected(index.error());
      }
      auto value = compile_expr(*stmt.value);
      if (!value) {
        return std::unexpected(value.error());
      }
      chunk_.emit(OpCode::StoreIndex);
      return {};
    }
    if (const auto *fld = std::get_if<FieldExpr>(&stmt.target->node)) {
      auto obj = compile_expr(*fld->object);
      if (!obj) {
        return std::unexpected(obj.error());
      }
      auto value = compile_expr(*stmt.value);
      if (!value) {
        return std::unexpected(value.error());
      }
      auto ni = add_field_name(fld->field, fld->pos);
      if (!ni) {
        return std::unexpected(ni.error());
      }
      chunk_.emit_u16(OpCode::StoreField, *ni);
      return {};
    }
    return std::unexpected(error(ErrorCode::UnexpectedToken, stmt.pos,
                                 "Некоректна ліва частина присвоєння"));
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const PrintStmt &stmt) {
    if (stmt.has_format) {
      auto info = analyze_format(stmt.format);
      if (!info) {
        return std::unexpected(error(ErrorCode::FormatMismatch, stmt.format_pos,
                                     "Некоректний рядок формату"));
      }
      if (info->arg_count != stmt.args.size()) {
        return std::unexpected(
            error(ErrorCode::FormatMismatch, stmt.format_pos,
                  "Кількість плейсхолдерів не збігається з аргументами"));
      }
      if (stmt.args.size() > kMaxArgCount) {
        return std::unexpected(
            error(ErrorCode::ArityMismatch, stmt.format_pos,
                  "Занадто багато аргументів print (макс. 255)"));
      }
      for (const auto &arg : stmt.args) {
        auto r = compile_expr(*arg);
        if (!r) {
          return std::unexpected(r.error());
        }
      }
      auto idx = add_fmt(stmt.format, stmt.format_pos);
      if (!idx) {
        return std::unexpected(idx.error());
      }
      chunk_.emit_print_fmt(*idx, static_cast<std::uint8_t>(stmt.args.size()));
      return {};
    }

    if (stmt.args.size() != 1) {
      return std::unexpected(error(ErrorCode::UnexpectedToken, stmt.pos,
                                   "print без формату очікує один вираз"));
    }
    auto r = compile_expr(*stmt.args[0]);
    if (!r) {
      return std::unexpected(r.error());
    }
    chunk_.emit(OpCode::Print);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const BlockStmt &stmt) {
    push_scope();
    for (const auto &inner : stmt.statements) {
      auto r = compile_stmt(*inner);
      if (!r) {
        pop_scope();
        return std::unexpected(r.error());
      }
    }
    pop_scope();
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const IfStmt &stmt) {
    auto cond = compile_expr(*stmt.condition);
    if (!cond) {
      return std::unexpected(cond.error());
    }

    const auto else_jump = emit_jump(OpCode::JumpIfFalse);

    auto then_r = compile_stmt(*stmt.then_branch);
    if (!then_r) {
      return std::unexpected(then_r.error());
    }

    if (stmt.else_branch) {
      const auto end_jump = emit_jump(OpCode::Jump);
      if (auto p = patch_jump(else_jump, stmt.pos); !p) {
        return p;
      }
      auto else_r = compile_stmt(*stmt.else_branch);
      if (!else_r) {
        return std::unexpected(else_r.error());
      }
      return patch_jump(end_jump, stmt.pos);
    }
    return patch_jump(else_jump, stmt.pos);
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const WhileStmt &stmt) {
    auto loop_start = code_addr(stmt.pos);
    if (!loop_start) {
      return std::unexpected(loop_start.error());
    }
    auto cond = compile_expr(*stmt.condition);
    if (!cond) {
      return std::unexpected(cond.error());
    }
    const auto exit_jump = emit_jump(OpCode::JumpIfFalse);

    loops_.push_back(LoopCtx{*loop_start, {}, {}});
    auto body = compile_stmt(*stmt.body);
    if (!body) {
      loops_.pop_back();
      return std::unexpected(body.error());
    }
    chunk_.emit_u16(OpCode::Jump, *loop_start);

    auto ctx = std::move(loops_.back());
    loops_.pop_back();
    if (auto p = patch_jump(exit_jump, stmt.pos); !p) {
      return p;
    }
    for (const auto off : ctx.break_patches) {
      if (auto p = patch_jump(off, stmt.pos); !p) {
        return p;
      }
    }
    return {};
  }

  [[nodiscard]] std::expected<std::uint16_t, Diagnostic>
  alloc_temp(std::string name, SourcePos pos) {
    auto slot = alloc_slot(pos);
    if (!slot) {
      return std::unexpected(slot.error());
    }
    declare_local(std::move(name), *slot);
    return *slot;
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const ForInStmt &stmt) {
    // { it = MakeIter(xs); i = 0; while (i < len(it)) { bind; body; i = i+1 } }
    push_scope();
    auto iter_e = compile_expr(*stmt.iterable);
    if (!iter_e) {
      pop_scope();
      return std::unexpected(iter_e.error());
    }
    const std::uint8_t mode = stmt.second.empty() ? 1 : 2;
    chunk_.emit(OpCode::MakeIter);
    chunk_.code.push_back(mode);

    auto it_slot = alloc_temp("__it", stmt.pos);
    if (!it_slot) {
      pop_scope();
      return std::unexpected(it_slot.error());
    }
    chunk_.emit_u16(OpCode::StoreLocal, *it_slot);

    auto i_slot = alloc_temp("__i", stmt.pos);
    if (!i_slot) {
      pop_scope();
      return std::unexpected(i_slot.error());
    }
    auto zero = add_const(Value{Integer{0}}, stmt.pos);
    if (!zero) {
      pop_scope();
      return std::unexpected(zero.error());
    }
    chunk_.emit_u16(OpCode::LoadConst, *zero);
    chunk_.emit_u16(OpCode::StoreLocal, *i_slot);

    auto one = add_const(Value{Integer{1}}, stmt.pos);
    if (!one) {
      pop_scope();
      return std::unexpected(one.error());
    }
    auto loop_start = code_addr(stmt.pos);
    if (!loop_start) {
      pop_scope();
      return std::unexpected(loop_start.error());
    }

    // cond: i < len(it)
    chunk_.emit_u16(OpCode::LoadLocal, *i_slot);
    chunk_.emit_u16(OpCode::LoadLocal, *it_slot);
    chunk_.emit_call(OpCode::CallBuiltin,
                     static_cast<std::uint16_t>(BuiltinId::Len), 1);
    chunk_.emit(OpCode::Lt);
    const auto exit_jump = emit_jump(OpCode::JumpIfFalse);

    // bind variables in inner scope so each iteration is fresh let
    push_scope();
    if (mode == 1) {
      chunk_.emit_u16(OpCode::LoadLocal, *it_slot);
      chunk_.emit_u16(OpCode::LoadLocal, *i_slot);
      chunk_.emit(OpCode::LoadIndex);
      auto x_slot = alloc_slot(stmt.pos);
      if (!x_slot) {
        pop_scope();
        pop_scope();
        return std::unexpected(x_slot.error());
      }
      declare_local(stmt.first, *x_slot);
      chunk_.emit_u16(OpCode::StoreLocal, *x_slot);
    } else {
      chunk_.emit_u16(OpCode::LoadLocal, *it_slot);
      chunk_.emit_u16(OpCode::LoadLocal, *i_slot);
      chunk_.emit(OpCode::LoadIndex);
      auto e_slot = alloc_temp("__e", stmt.pos);
      if (!e_slot) {
        pop_scope();
        pop_scope();
        return std::unexpected(e_slot.error());
      }
      chunk_.emit_u16(OpCode::StoreLocal, *e_slot);

      chunk_.emit_u16(OpCode::LoadLocal, *e_slot);
      chunk_.emit_u16(OpCode::LoadConst, *zero);
      chunk_.emit(OpCode::LoadIndex);
      auto a_slot = alloc_slot(stmt.pos);
      if (!a_slot) {
        pop_scope();
        pop_scope();
        return std::unexpected(a_slot.error());
      }
      declare_local(stmt.first, *a_slot);
      chunk_.emit_u16(OpCode::StoreLocal, *a_slot);

      chunk_.emit_u16(OpCode::LoadLocal, *e_slot);
      chunk_.emit_u16(OpCode::LoadConst, *one);
      chunk_.emit(OpCode::LoadIndex);
      auto b_slot = alloc_slot(stmt.pos);
      if (!b_slot) {
        pop_scope();
        pop_scope();
        return std::unexpected(b_slot.error());
      }
      declare_local(stmt.second, *b_slot);
      chunk_.emit_u16(OpCode::StoreLocal, *b_slot);
    }

    loops_.push_back(LoopCtx{});
    auto body = compile_stmt(*stmt.body);
    if (!body) {
      loops_.pop_back();
      pop_scope();
      pop_scope();
      return std::unexpected(body.error());
    }
    pop_scope(); // bindings scope

    auto continue_target = code_addr(stmt.pos);
    if (!continue_target) {
      pop_scope();
      return std::unexpected(continue_target.error());
    }
    auto ctx = std::move(loops_.back());
    loops_.pop_back();
    for (const auto off : ctx.continue_patches) {
      patch_jump_to(off, *continue_target);
    }

    // i = i + 1
    chunk_.emit_u16(OpCode::LoadLocal, *i_slot);
    chunk_.emit_u16(OpCode::LoadConst, *one);
    chunk_.emit(OpCode::Add);
    chunk_.emit_u16(OpCode::StoreLocal, *i_slot);
    chunk_.emit_u16(OpCode::Jump, *loop_start);

    if (auto p = patch_jump(exit_jump, stmt.pos); !p) {
      pop_scope();
      return p;
    }
    for (const auto off : ctx.break_patches) {
      if (auto p = patch_jump(off, stmt.pos); !p) {
        pop_scope();
        return p;
      }
    }
    pop_scope();
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const ForStmt &stmt) {
    // { init; loop: cond; body; continue: step; Jump loop; end: }
    push_scope();
    if (stmt.init) {
      auto init = compile_stmt(*stmt.init);
      if (!init) {
        pop_scope();
        return std::unexpected(init.error());
      }
    }

    auto loop_start = code_addr(stmt.pos);
    if (!loop_start) {
      pop_scope();
      return std::unexpected(loop_start.error());
    }
    if (stmt.condition) {
      auto cond = compile_expr(*stmt.condition);
      if (!cond) {
        pop_scope();
        return std::unexpected(cond.error());
      }
    } else {
      auto idx = add_const(Value{Boolean{true}}, stmt.pos);
      if (!idx) {
        pop_scope();
        return std::unexpected(idx.error());
      }
      chunk_.emit_u16(OpCode::LoadConst, *idx);
    }
    const auto exit_jump = emit_jump(OpCode::JumpIfFalse);

    loops_.push_back(LoopCtx{}); // continue_target заповнимо після body
    auto body = compile_stmt(*stmt.body);
    if (!body) {
      loops_.pop_back();
      pop_scope();
      return std::unexpected(body.error());
    }

    auto continue_target = code_addr(stmt.pos);
    if (!continue_target) {
      pop_scope();
      return std::unexpected(continue_target.error());
    }
    auto ctx = std::move(loops_.back());
    loops_.pop_back();
    for (const auto off : ctx.continue_patches) {
      patch_jump_to(off, *continue_target);
    }

    if (stmt.step) {
      auto step = compile_stmt(*stmt.step);
      if (!step) {
        pop_scope();
        return std::unexpected(step.error());
      }
    }
    chunk_.emit_u16(OpCode::Jump, *loop_start);

    if (auto p = patch_jump(exit_jump, stmt.pos); !p) {
      pop_scope();
      return p;
    }
    for (const auto off : ctx.break_patches) {
      if (auto p = patch_jump(off, stmt.pos); !p) {
        pop_scope();
        return p;
      }
    }
    pop_scope();
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const BreakStmt &stmt) {
    if (loops_.empty()) {
      return std::unexpected(
          error(ErrorCode::UnexpectedToken, stmt.pos, "break поза циклом"));
    }
    loops_.back().break_patches.push_back(emit_jump(OpCode::Jump));
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const ContinueStmt &stmt) {
    if (loops_.empty()) {
      return std::unexpected(
          error(ErrorCode::UnexpectedToken, stmt.pos, "continue поза циклом"));
    }
    auto &loop = loops_.back();
    if (loop.continue_target) {
      chunk_.emit_u16(OpCode::Jump, *loop.continue_target);
    } else {
      loop.continue_patches.push_back(emit_jump(OpCode::Jump));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const FunStmt &stmt) {
    return std::unexpected(error(ErrorCode::UnexpectedToken, stmt.pos,
                                 "fun лише на верхньому рівні програми"));
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const ReturnStmt &stmt) {
    if (!in_function_) {
      return std::unexpected(
          error(ErrorCode::UnexpectedToken, stmt.pos, "return поза функцією"));
    }
    if (stmt.value) {
      auto r = compile_expr(*stmt.value);
      if (!r) {
        return std::unexpected(r.error());
      }
    } else {
      auto zero = add_const(Value{Integer{0}}, stmt.pos);
      if (!zero) {
        return std::unexpected(zero.error());
      }
      chunk_.emit_u16(OpCode::LoadConst, *zero);
    }
    chunk_.emit(OpCode::Return);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_one(const ExprStmt &stmt) {
    auto r = compile_expr(*stmt.expr);
    if (!r) {
      return std::unexpected(r.error());
    }
    chunk_.emit(OpCode::Pop); // відкинути результат
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic> compile_expr(const Expr &expr) {
    return std::visit(
        [this](const auto &node) -> std::expected<void, Diagnostic> {
          return this->compile_node(node);
        },
        expr.node);
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const LiteralExpr &lit) {
    auto idx = add_const(lit.value, lit.pos);
    if (!idx) {
      return std::unexpected(idx.error());
    }
    chunk_.emit_u16(OpCode::LoadConst, *idx);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const VarExpr &var) {
    if (auto slot = resolve_local(var.name)) {
      chunk_.emit_u16(OpCode::LoadLocal, *slot);
      return {};
    }
    if (auto c = find_constant(var.name)) {
      auto idx = add_const(*c, var.pos);
      if (!idx) {
        return std::unexpected(idx.error());
      }
      chunk_.emit_u16(OpCode::LoadConst, *idx);
      return {};
    }
    return std::unexpected(
        error(ErrorCode::UndeclaredVariable, var.pos, "Неоголошена змінна"));
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const ArrayExpr &arr) {
    if (arr.elements.size() > kMaxU16) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, arr.pos,
                "Занадто великий літерал масиву (макс. 65535 елементів)"));
    }
    for (const auto &el : arr.elements) {
      auto r = compile_expr(*el);
      if (!r) {
        return std::unexpected(r.error());
      }
    }
    chunk_.emit_u16(OpCode::MakeArray,
                    static_cast<std::uint16_t>(arr.elements.size()));
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const RecordExpr &rec) {
    if (rec.fields.size() > kMaxU16) {
      return std::unexpected(
          error(ErrorCode::BytecodeLimit, rec.pos,
                "Занадто великий літерал запису (макс. 65535 полів)"));
    }
    std::vector<std::uint16_t> name_idxs;
    name_idxs.reserve(rec.fields.size());
    for (const auto &f : rec.fields) {
      auto r = compile_expr(*f.value);
      if (!r) {
        return std::unexpected(r.error());
      }
      auto ni = add_field_name(f.name, rec.pos);
      if (!ni) {
        return std::unexpected(ni.error());
      }
      name_idxs.push_back(*ni);
    }
    chunk_.emit_u16(OpCode::MakeRecord,
                    static_cast<std::uint16_t>(rec.fields.size()));
    for (const auto ni : name_idxs) {
      chunk_.emit_u16_raw(ni);
    }
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const IndexExpr &idx) {
    auto obj = compile_expr(*idx.object);
    if (!obj) {
      return std::unexpected(obj.error());
    }
    auto index = compile_expr(*idx.index);
    if (!index) {
      return std::unexpected(index.error());
    }
    chunk_.emit(OpCode::LoadIndex);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const FieldExpr &fld) {
    auto obj = compile_expr(*fld.object);
    if (!obj) {
      return std::unexpected(obj.error());
    }
    auto ni = add_field_name(fld.field, fld.pos);
    if (!ni) {
      return std::unexpected(ni.error());
    }
    chunk_.emit_u16(OpCode::LoadField, *ni);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const CallExpr &call) {
    if (call.args.size() > kMaxArgCount) {
      return std::unexpected(error(ErrorCode::ArityMismatch, call.callee_pos,
                                   "Занадто багато аргументів (макс. 255)"));
    }
    for (const auto &arg : call.args) {
      auto r = compile_expr(*arg);
      if (!r) {
        return std::unexpected(r.error());
      }
    }
    const auto argc = static_cast<std::uint8_t>(call.args.size());

    if (const auto bid = find_builtin(call.callee)) {
      if (argc != builtin_arity(*bid)) {
        return std::unexpected(error(ErrorCode::ArityMismatch, call.callee_pos,
                                     "Невірна кількість аргументів builtin"));
      }
      chunk_.emit_call(OpCode::CallBuiltin, static_cast<std::uint16_t>(*bid),
                       argc);
      return {};
    }

    const auto found = fun_index_.find(call.callee);
    if (found == fun_index_.end()) {
      return std::unexpected(error(ErrorCode::UnknownFunction, call.callee_pos,
                                   "Невідома функція"));
    }
    const auto &proto = chunk_.functions[found->second];
    if (argc != proto.arity) {
      return std::unexpected(error(ErrorCode::ArityMismatch, call.callee_pos,
                                   "Невірна кількість аргументів"));
    }
    chunk_.emit_call(OpCode::Call, found->second, argc);
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const UnaryExpr &unary) {
    auto r = compile_expr(*unary.operand);
    if (!r) {
      return r;
    }
    switch (unary.op) {
    case TokenKind::Minus:
      chunk_.emit(OpCode::Neg);
      break;
    case TokenKind::Not:
      chunk_.emit(OpCode::Not);
      break;
    case TokenKind::Plus:
      chunk_.emit(OpCode::Pos);
      break;
    default:
      return std::unexpected(error(ErrorCode::UnexpectedToken, unary.pos,
                                   "Невідомий унарний оператор"));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, Diagnostic>
  compile_node(const BinaryExpr &binary) {
    auto left = compile_expr(*binary.left);
    if (!left) {
      return left;
    }
    auto right = compile_expr(*binary.right);
    if (!right) {
      return right;
    }
    switch (binary.op) {
    case TokenKind::Plus:
      chunk_.emit(OpCode::Add);
      break;
    case TokenKind::Minus:
      chunk_.emit(OpCode::Sub);
      break;
    case TokenKind::Star:
      chunk_.emit(OpCode::Mul);
      break;
    case TokenKind::Slash:
      chunk_.emit(OpCode::Div);
      break;
    case TokenKind::Percent:
      chunk_.emit(OpCode::Mod);
      break;
    case TokenKind::StarStar:
      chunk_.emit(OpCode::Pow);
      break;
    case TokenKind::EqEq:
      chunk_.emit(OpCode::Eq);
      break;
    case TokenKind::NotEq:
      chunk_.emit(OpCode::Ne);
      break;
    case TokenKind::Less:
      chunk_.emit(OpCode::Lt);
      break;
    case TokenKind::LessEq:
      chunk_.emit(OpCode::Le);
      break;
    case TokenKind::Greater:
      chunk_.emit(OpCode::Gt);
      break;
    case TokenKind::GreaterEq:
      chunk_.emit(OpCode::Ge);
      break;
    case TokenKind::And:
      chunk_.emit(OpCode::And);
      break;
    case TokenKind::Or:
      chunk_.emit(OpCode::Or);
      break;
    default:
      return std::unexpected(
          error(ErrorCode::UnexpectedToken, binary.pos, "Невідомий оператор"));
    }
    return {};
  }
};

} // namespace

CompileResult compile(const Program &program) {
  Compiler compiler;
  return compiler.compile(program);
}

void Chunk::emit(OpCode op) { code.push_back(static_cast<std::uint8_t>(op)); }

void Chunk::emit_u16(OpCode op, std::uint16_t operand) {
  code.push_back(static_cast<std::uint8_t>(op));
  code.push_back(static_cast<std::uint8_t>(operand & 0xFFu));
  code.push_back(static_cast<std::uint8_t>((operand >> 8) & 0xFFu));
}

void Chunk::emit_print_fmt(std::uint16_t format_index, std::uint8_t arg_count) {
  code.push_back(static_cast<std::uint8_t>(OpCode::PrintFmt));
  code.push_back(static_cast<std::uint8_t>(format_index & 0xFFu));
  code.push_back(static_cast<std::uint8_t>((format_index >> 8) & 0xFFu));
  code.push_back(arg_count);
}

void Chunk::emit_call(OpCode op, std::uint16_t index, std::uint8_t arg_count) {
  code.push_back(static_cast<std::uint8_t>(op));
  code.push_back(static_cast<std::uint8_t>(index & 0xFFu));
  code.push_back(static_cast<std::uint8_t>((index >> 8) & 0xFFu));
  code.push_back(arg_count);
}

std::optional<std::uint16_t> Chunk::add_constant(Value v) {
  if (constants.size() > std::numeric_limits<std::uint16_t>::max()) {
    return std::nullopt;
  }
  const auto idx = static_cast<std::uint16_t>(constants.size());
  constants.push_back(std::move(v));
  return idx;
}

std::optional<std::uint16_t> Chunk::add_format(std::string fmt) {
  if (formats.size() > std::numeric_limits<std::uint16_t>::max()) {
    return std::nullopt;
  }
  const auto idx = static_cast<std::uint16_t>(formats.size());
  formats.push_back(std::move(fmt));
  return idx;
}

std::optional<std::uint16_t> Chunk::add_name(std::string name) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == name) {
      return static_cast<std::uint16_t>(i);
    }
  }
  if (names.size() > std::numeric_limits<std::uint16_t>::max()) {
    return std::nullopt;
  }
  const auto idx = static_cast<std::uint16_t>(names.size());
  names.push_back(std::move(name));
  return idx;
}

void Chunk::emit_u16_raw(std::uint16_t operand) {
  code.push_back(static_cast<std::uint8_t>(operand & 0xFFu));
  code.push_back(static_cast<std::uint8_t>((operand >> 8) & 0xFFu));
}

void patch_u16(Chunk &chunk, std::size_t offset, std::uint16_t value) {
  chunk.code[offset] = static_cast<std::uint8_t>(value & 0xFFu);
  chunk.code[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

} // namespace calc
