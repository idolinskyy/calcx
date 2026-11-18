// Попередження про цикли без досяжного виходу.
#include "calc/loops.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calc {
namespace {

enum class Tri { No, Yes, Maybe };

[[nodiscard]] Tri tri_not(Tri t) {
  if (t == Tri::Yes) {
    return Tri::No;
  }
  if (t == Tri::No) {
    return Tri::Yes;
  }
  return Tri::Maybe;
}

[[nodiscard]] Tri tri_and(Tri a, Tri b) {
  if (a == Tri::No || b == Tri::No) {
    return Tri::No;
  }
  if (a == Tri::Yes && b == Tri::Yes) {
    return Tri::Yes;
  }
  return Tri::Maybe;
}

/// Які знаки можливі: від'ємний, нуль, додатний.
struct Signs {
  bool neg{true};
  bool zero{true};
  bool pos{true};

  [[nodiscard]] static Signs unknown() { return {}; }
  [[nodiscard]] static Signs bottom() { return {false, false, false}; }
  [[nodiscard]] static Signs only_neg() { return {true, false, false}; }
  [[nodiscard]] static Signs only_zero() { return {false, true, false}; }
  [[nodiscard]] static Signs only_pos() { return {false, false, true}; }

  [[nodiscard]] bool is_bottom() const { return !neg && !zero && !pos; }

  [[nodiscard]] Signs join(Signs o) const {
    return {neg || o.neg, zero || o.zero, pos || o.pos};
  }
};

struct Abs {
  Signs signs{};
  std::optional<long double> exact;
};

[[nodiscard]] Abs abs_unknown() { return {}; }

[[nodiscard]] Abs abs_number(long double x) {
  Abs a;
  a.exact = x;
  if (x < 0) {
    a.signs = Signs::only_neg();
  } else if (x == 0) {
    a.signs = Signs::only_zero();
  } else {
    a.signs = Signs::only_pos();
  }
  return a;
}

[[nodiscard]] Abs abs_join(Abs a, Abs b) {
  Abs o;
  o.signs = a.signs.join(b.signs);
  if (a.exact && b.exact && *a.exact == *b.exact) {
    o.exact = a.exact;
  }
  return o;
}

using Env = std::map<std::string, Abs>;

[[nodiscard]] Abs lookup(const Env &env, const std::string &name) {
  const auto it = env.find(name);
  if (it == env.end()) {
    return abs_unknown();
  }
  return it->second;
}

[[nodiscard]] bool is_pure_call(std::string_view name) {
  static const std::set<std::string_view> kPure = {
      "sin",     "cos",     "tan",   "asin",    "acos",     "atan",
      "atan2",   "sinh",    "cosh",  "tanh",    "sqrt",     "exp",
      "ln",      "log",     "log10", "log2",    "pow",      "hypot",
      "abs",     "sign",    "re",    "im",      "arg",      "conj",
      "floor",   "ceil",    "round", "trunc",   "min",      "max",
      "deg",     "rad",     "len",   "clone",   "same",     "int",
      "real",    "bool",    "complex", "rational", "is_bool", "is_int",
      "is_rational", "is_real", "is_complex", "is_number", "is_array",
      "is_record", "is_string"};
  return kPure.contains(name);
}

void reads_of(const Expr &e, std::set<std::string> &out);

void reads_of(const Expr &e, std::set<std::string> &out) {
  std::visit(
      [&](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, VarExpr>) {
          out.insert(n.name);
        } else if constexpr (std::is_same_v<T, LiteralExpr>) {
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          reads_of(*n.operand, out);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          reads_of(*n.left, out);
          reads_of(*n.right, out);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          for (const auto &arg : n.args) {
            reads_of(*arg, out);
          }
        } else if constexpr (std::is_same_v<T, ArrayExpr>) {
          for (const auto &el : n.elements) {
            reads_of(*el, out);
          }
        } else if constexpr (std::is_same_v<T, RecordExpr>) {
          for (const auto &field : n.fields) {
            reads_of(*field.value, out);
          }
        } else if constexpr (std::is_same_v<T, IndexExpr>) {
          reads_of(*n.object, out);
          reads_of(*n.index, out);
        } else if constexpr (std::is_same_v<T, FieldExpr>) {
          reads_of(*n.object, out);
        } else if constexpr (std::is_same_v<T, UpdateExpr>) {
          reads_of(*n.target, out);
        } else if constexpr (std::is_same_v<T, HasExpr>) {
          reads_of(*n.object, out);
        }
      },
      e.node);
}

[[nodiscard]] std::optional<std::string> root_name(const Expr &e) {
  if (const auto *v = std::get_if<VarExpr>(&e.node)) {
    return v->name;
  }
  if (const auto *ix = std::get_if<IndexExpr>(&e.node)) {
    return root_name(*ix->object);
  }
  if (const auto *f = std::get_if<FieldExpr>(&e.node)) {
    return root_name(*f->object);
  }
  if (const auto *u = std::get_if<UpdateExpr>(&e.node)) {
    return root_name(*u->target);
  }
  return std::nullopt;
}

void writes_in_expr(const Expr &e, std::set<std::string> &out) {
  std::visit(
      [&](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, UpdateExpr>) {
          if (auto name = root_name(*n.target)) {
            out.insert(*name);
          }
          writes_in_expr(*n.target, out);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          if (!is_pure_call(n.callee)) {
            for (const auto &arg : n.args) {
              if (auto name = root_name(*arg)) {
                out.insert(*name);
              }
            }
          }
          for (const auto &arg : n.args) {
            writes_in_expr(*arg, out);
          }
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          writes_in_expr(*n.operand, out);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          writes_in_expr(*n.left, out);
          writes_in_expr(*n.right, out);
        } else if constexpr (std::is_same_v<T, ArrayExpr>) {
          for (const auto &el : n.elements) {
            writes_in_expr(*el, out);
          }
        } else if constexpr (std::is_same_v<T, RecordExpr>) {
          for (const auto &field : n.fields) {
            writes_in_expr(*field.value, out);
          }
        } else if constexpr (std::is_same_v<T, IndexExpr>) {
          writes_in_expr(*n.object, out);
          writes_in_expr(*n.index, out);
        } else if constexpr (std::is_same_v<T, FieldExpr>) {
          writes_in_expr(*n.object, out);
        } else if constexpr (std::is_same_v<T, HasExpr>) {
          writes_in_expr(*n.object, out);
        }
      },
      e.node);
}

void collect_writes(const Stmt &s, std::set<std::string> &out);

void write_target(const Expr &target, std::set<std::string> &out) {
  if (auto name = root_name(target)) {
    out.insert(*name);
  }
}

void collect_writes(const Stmt &s, std::set<std::string> &out) {
  std::visit(
      [&](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LetStmt>) {
          for (const auto &b : n.bindings) {
            out.insert(b.name);
            writes_in_expr(*b.init, out);
          }
        } else if constexpr (std::is_same_v<T, AssignStmt>) {
          write_target(*n.target, out);
          writes_in_expr(*n.value, out);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          for (const auto &st : n.statements) {
            collect_writes(*st, out);
          }
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          writes_in_expr(*n.condition, out);
          collect_writes(*n.then_branch, out);
          if (n.else_branch) {
            collect_writes(*n.else_branch, out);
          }
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          writes_in_expr(*n.condition, out);
          collect_writes(*n.body, out);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
          if (n.condition) {
            writes_in_expr(*n.condition, out);
          }
          if (n.step) {
            collect_writes(*n.step, out);
          }
          collect_writes(*n.body, out);
        } else if constexpr (std::is_same_v<T, ForInStmt>) {
          writes_in_expr(*n.iterable, out);
          out.insert(n.first);
          if (!n.second.empty()) {
            out.insert(n.second);
          }
          collect_writes(*n.body, out);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
          for (const auto &arg : n.args) {
            writes_in_expr(*arg, out);
          }
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
          writes_in_expr(*n.expr, out);
        } else if constexpr (std::is_same_v<T, FunStmt>) {
          collect_writes(*n.body, out);
        }
      },
      s.node);
}

void collect_names(const Expr &e, std::set<std::string> &out) { reads_of(e, out); }

void collect_names_stmt(const Stmt &s, std::set<std::string> &out) {
  std::visit(
      [&](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LetStmt>) {
          for (const auto &b : n.bindings) {
            collect_names(*b.init, out);
          }
        } else if constexpr (std::is_same_v<T, AssignStmt>) {
          collect_names(*n.target, out);
          collect_names(*n.value, out);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          for (const auto &st : n.statements) {
            collect_names_stmt(*st, out);
          }
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          collect_names(*n.condition, out);
          collect_names_stmt(*n.then_branch, out);
          if (n.else_branch) {
            collect_names_stmt(*n.else_branch, out);
          }
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          collect_names(*n.condition, out);
          collect_names_stmt(*n.body, out);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
          if (n.init) {
            collect_names_stmt(*n.init, out);
          }
          if (n.condition) {
            collect_names(*n.condition, out);
          }
          if (n.step) {
            collect_names_stmt(*n.step, out);
          }
          collect_names_stmt(*n.body, out);
        } else if constexpr (std::is_same_v<T, ForInStmt>) {
          collect_names(*n.iterable, out);
          collect_names_stmt(*n.body, out);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
          for (const auto &arg : n.args) {
            collect_names(*arg, out);
          }
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
          collect_names(*n.expr, out);
        } else if constexpr (std::is_same_v<T, FunStmt>) {
          collect_names_stmt(*n.body, out);
        }
      },
      s.node);
}

enum class Rel { Always, Never, Sometimes };

[[nodiscard]] Rel rel_less(int sign, long double lit) {
  if (sign < 0) {
    return lit >= 0 ? Rel::Always : Rel::Sometimes;
  }
  if (sign == 0) {
    return 0 < lit ? Rel::Always : Rel::Never;
  }
  return lit <= 0 ? Rel::Never : Rel::Sometimes;
}

[[nodiscard]] Rel rel_lesseq(int sign, long double lit) {
  if (sign < 0) {
    return lit >= 0 ? Rel::Always : Rel::Sometimes;
  }
  if (sign == 0) {
    return 0 <= lit ? Rel::Always : Rel::Never;
  }
  return lit <= 0 ? Rel::Never : Rel::Sometimes;
}

[[nodiscard]] Rel rel_eq(int sign, long double lit) {
  if (sign == 0) {
    return lit == 0 ? Rel::Always : Rel::Never;
  }
  if (sign < 0) {
    return lit < 0 ? Rel::Sometimes : Rel::Never;
  }
  return lit > 0 ? Rel::Sometimes : Rel::Never;
}

[[nodiscard]] Rel rel_neq(int sign, long double lit) {
  const Rel eq = rel_eq(sign, lit);
  if (eq == Rel::Always) {
    return Rel::Never;
  }
  if (eq == Rel::Never) {
    return Rel::Always;
  }
  return Rel::Sometimes;
}

[[nodiscard]] Rel rel_greater(int sign, long double lit) {
  if (sign > 0) {
    return lit <= 0 ? Rel::Always : Rel::Sometimes;
  }
  if (sign == 0) {
    return 0 > lit ? Rel::Always : Rel::Never;
  }
  return lit >= 0 ? Rel::Never : Rel::Sometimes;
}

[[nodiscard]] Rel rel_greatereq(int sign, long double lit) {
  if (sign > 0) {
    return lit <= 0 ? Rel::Always : Rel::Sometimes;
  }
  if (sign == 0) {
    return 0 >= lit ? Rel::Always : Rel::Never;
  }
  return lit >= 0 ? Rel::Never : Rel::Sometimes;
}

[[nodiscard]] Rel rel_of(TokenKind op, int sign, long double lit) {
  switch (op) {
  case TokenKind::Less:
    return rel_less(sign, lit);
  case TokenKind::LessEq:
    return rel_lesseq(sign, lit);
  case TokenKind::Greater:
    return rel_greater(sign, lit);
  case TokenKind::GreaterEq:
    return rel_greatereq(sign, lit);
  case TokenKind::EqEq:
    return rel_eq(sign, lit);
  case TokenKind::NotEq:
    return rel_neq(sign, lit);
  default:
    return Rel::Sometimes;
  }
}

[[nodiscard]] TokenKind flip_cmp(TokenKind op) {
  switch (op) {
  case TokenKind::Less:
    return TokenKind::Greater;
  case TokenKind::LessEq:
    return TokenKind::GreaterEq;
  case TokenKind::Greater:
    return TokenKind::Less;
  case TokenKind::GreaterEq:
    return TokenKind::LessEq;
  default:
    return op;
  }
}

[[nodiscard]] Abs eval_abs_value(const Expr &e, const Env &env);

[[nodiscard]] Abs apply_abs_fn(Abs a) {
  if (a.exact) {
    return abs_number(std::fabs(*a.exact));
  }
  Abs o;
  o.signs = Signs::bottom();
  if (a.signs.zero) {
    o.signs.zero = true;
  }
  if (a.signs.neg || a.signs.pos) {
    o.signs.pos = true;
  }
  if (o.signs.is_bottom()) {
    return abs_unknown();
  }
  return o;
}

[[nodiscard]] Abs negate_abs(Abs a) {
  if (a.exact) {
    return abs_number(-*a.exact);
  }
  Abs o;
  o.signs.neg = a.signs.pos;
  o.signs.pos = a.signs.neg;
  o.signs.zero = a.signs.zero;
  return o;
}

[[nodiscard]] int sign_result(int sa, int sb, TokenKind op) {
  switch (op) {
  case TokenKind::Plus:
    if ((sa > 0 && sb > 0) || (sa < 0 && sb < 0)) {
      return sa > 0 ? 1 : -1;
    }
    if (sa == 0) {
      return sb;
    }
    if (sb == 0) {
      return sa;
    }
    return 2; // різні знаки — невідомо в межах пари класів
  case TokenKind::Minus:
    return sign_result(sa, -sb, TokenKind::Plus);
  case TokenKind::Star:
    if (sa == 0 || sb == 0) {
      return 0;
    }
    return (sa < 0) == (sb < 0) ? 1 : -1;
  case TokenKind::Slash:
  case TokenKind::StarStar:
    return 2;
  default:
    return 2;
  }
}

[[nodiscard]] Abs eval_arith(TokenKind op, Abs a, Abs b) {
  if (a.exact && b.exact) {
    const long double x = *a.exact;
    const long double y = *b.exact;
    if (op == TokenKind::Plus) {
      return abs_number(x + y);
    }
    if (op == TokenKind::Minus) {
      return abs_number(x - y);
    }
    if (op == TokenKind::Star) {
      return abs_number(x * y);
    }
    if (op == TokenKind::Slash && y != 0) {
      return abs_number(x / y);
    }
  }
  if (op == TokenKind::Slash || op == TokenKind::Percent ||
      op == TokenKind::StarStar) {
    if (op == TokenKind::Slash) {
      // Знак частки як у добутку, нульовий дільник пропускаємо.
      Abs o;
      o.signs = Signs::bottom();
      const int as[] = {-1, 0, 1};
      const int bs[] = {-1, 0, 1};
      for (int sa : as) {
        if ((sa < 0 && !a.signs.neg) || (sa == 0 && !a.signs.zero) ||
            (sa > 0 && !a.signs.pos)) {
          continue;
        }
        for (int sb : bs) {
          if (sb == 0) {
            continue;
          }
          if ((sb < 0 && !b.signs.neg) || (sb > 0 && !b.signs.pos)) {
            continue;
          }
          const int r = (sa == 0) ? 0 : ((sa < 0) == (sb < 0) ? 1 : -1);
          if (r < 0) {
            o.signs.neg = true;
          } else if (r == 0) {
            o.signs.zero = true;
          } else {
            o.signs.pos = true;
          }
        }
      }
      if (o.signs.is_bottom()) {
        return abs_unknown();
      }
      return o;
    }
    return abs_unknown();
  }

  Abs o;
  o.signs = Signs::bottom();
  bool unknown_pair = false;
  const int as[] = {-1, 0, 1};
  const int bs[] = {-1, 0, 1};
  for (int sa : as) {
    if ((sa < 0 && !a.signs.neg) || (sa == 0 && !a.signs.zero) ||
        (sa > 0 && !a.signs.pos)) {
      continue;
    }
    for (int sb : bs) {
      if ((sb < 0 && !b.signs.neg) || (sb == 0 && !b.signs.zero) ||
          (sb > 0 && !b.signs.pos)) {
        continue;
      }
      const int r = sign_result(sa, sb, op);
      if (r == 2) {
        unknown_pair = true;
      } else if (r < 0) {
        o.signs.neg = true;
      } else if (r == 0) {
        o.signs.zero = true;
      } else {
        o.signs.pos = true;
      }
    }
  }
  if (unknown_pair) {
    return abs_unknown();
  }
  if (o.signs.is_bottom()) {
    return abs_unknown();
  }
  return o;
}

[[nodiscard]] Abs eval_abs_value(const Expr &e, const Env &env) {
  return std::visit(
      [&](const auto &n) -> Abs {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LiteralExpr>) {
          if (const auto *i = std::get_if<Integer>(&n.value)) {
            return abs_number(static_cast<long double>(*i));
          }
          if (const auto *q = std::get_if<Rational>(&n.value)) {
            if (q->den == 0) {
              return abs_unknown();
            }
            return abs_number(static_cast<long double>(q->num) /
                              static_cast<long double>(q->den));
          }
          if (const auto *r = std::get_if<Real>(&n.value)) {
            return abs_number(*r);
          }
          return abs_unknown();
        } else if constexpr (std::is_same_v<T, VarExpr>) {
          return lookup(env, n.name);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          if (n.op == TokenKind::Minus) {
            return negate_abs(eval_abs_value(*n.operand, env));
          }
          if (n.op == TokenKind::Plus) {
            return eval_abs_value(*n.operand, env);
          }
          return abs_unknown();
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          if (n.op == TokenKind::Plus || n.op == TokenKind::Minus ||
              n.op == TokenKind::Star || n.op == TokenKind::Slash ||
              n.op == TokenKind::Percent || n.op == TokenKind::StarStar) {
            return eval_arith(n.op, eval_abs_value(*n.left, env),
                              eval_abs_value(*n.right, env));
          }
          return abs_unknown();
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          if (n.callee == "abs" && !n.args.empty()) {
            return apply_abs_fn(eval_abs_value(*n.args[0], env));
          }
          return abs_unknown();
        } else {
          return abs_unknown();
        }
      },
      e.node);
}

[[nodiscard]] Tri cmp_exact(TokenKind op, long double x, long double y) {
  bool holds = false;
  switch (op) {
  case TokenKind::Less:
    holds = x < y;
    break;
  case TokenKind::LessEq:
    holds = x <= y;
    break;
  case TokenKind::Greater:
    holds = x > y;
    break;
  case TokenKind::GreaterEq:
    holds = x >= y;
    break;
  case TokenKind::EqEq:
    holds = x == y;
    break;
  case TokenKind::NotEq:
    holds = x != y;
    break;
  default:
    return Tri::Maybe;
  }
  return holds ? Tri::Yes : Tri::No;
}

[[nodiscard]] Tri cmp_signs(TokenKind op, Abs left, Abs right) {
  if (left.exact && right.exact) {
    return cmp_exact(op, *left.exact, *right.exact);
  }
  // Жодна сторона не число: порівнюємо класи знаків між собою.
  if (!left.exact && !right.exact) {
    bool any_true = false;
    bool any_false = false;
    const int signs[] = {-1, 0, 1};
    for (int sa : signs) {
      if ((sa < 0 && !left.signs.neg) || (sa == 0 && !left.signs.zero) ||
          (sa > 0 && !left.signs.pos)) {
        continue;
      }
      for (int sb : signs) {
        if ((sb < 0 && !right.signs.neg) || (sb == 0 && !right.signs.zero) ||
            (sb > 0 && !right.signs.pos)) {
          continue;
        }
        Rel r = Rel::Sometimes;
        switch (op) {
        case TokenKind::Less:
          if (sa < sb) {
            r = Rel::Always;
          } else if (sa > sb || (sa == 0 && sb == 0)) {
            r = Rel::Never;
          }
          break;
        case TokenKind::LessEq:
          if (sa < sb || (sa == 0 && sb == 0)) {
            r = Rel::Always;
          } else if (sa > sb) {
            r = Rel::Never;
          }
          break;
        case TokenKind::Greater:
          if (sa > sb) {
            r = Rel::Always;
          } else if (sa < sb || (sa == 0 && sb == 0)) {
            r = Rel::Never;
          }
          break;
        case TokenKind::GreaterEq:
          if (sa > sb || (sa == 0 && sb == 0)) {
            r = Rel::Always;
          } else if (sa < sb) {
            r = Rel::Never;
          }
          break;
        case TokenKind::EqEq:
          if (sa == 0 && sb == 0) {
            r = Rel::Always;
          } else if (sa != sb) {
            r = Rel::Never;
          }
          break;
        case TokenKind::NotEq:
          if (sa == 0 && sb == 0) {
            r = Rel::Never;
          } else if (sa != sb) {
            r = Rel::Always;
          }
          break;
        default:
          break;
        }
        if (r == Rel::Always || r == Rel::Sometimes) {
          any_true = true;
        }
        if (r == Rel::Never || r == Rel::Sometimes) {
          any_false = true;
        }
      }
    }
    if (any_true && any_false) {
      return Tri::Maybe;
    }
    if (any_true) {
      return Tri::Yes;
    }
    if (any_false) {
      return Tri::No;
    }
    return Tri::Maybe;
  }
  const Abs *var_side = nullptr;
  long double lit = 0;
  TokenKind use = op;
  if (right.exact && !left.exact) {
    var_side = &left;
    lit = *right.exact;
  } else {
    var_side = &right;
    lit = *left.exact;
    use = flip_cmp(op);
  }
  bool any_true = false;
  bool any_false = false;
  const int signs[] = {-1, 0, 1};
  for (int s : signs) {
    if ((s < 0 && !var_side->signs.neg) || (s == 0 && !var_side->signs.zero) ||
        (s > 0 && !var_side->signs.pos)) {
      continue;
    }
    const Rel r = rel_of(use, s, lit);
    if (r == Rel::Always || r == Rel::Sometimes) {
      any_true = true;
    }
    if (r == Rel::Never || r == Rel::Sometimes) {
      any_false = true;
    }
  }
  if (any_true && any_false) {
    return Tri::Maybe;
  }
  if (any_true) {
    return Tri::Yes;
  }
  if (any_false) {
    return Tri::No;
  }
  return Tri::Maybe;
}

[[nodiscard]] Tri eval_bool(const Expr &e, const Env &env);

[[nodiscard]] Tri eval_bool(const Expr &e, const Env &env) {
  if (const auto *lit = std::get_if<LiteralExpr>(&e.node)) {
    if (const auto *b = std::get_if<Boolean>(&lit->value)) {
      return *b ? Tri::Yes : Tri::No;
    }
    return Tri::Maybe;
  }
  if (const auto *u = std::get_if<UnaryExpr>(&e.node)) {
    if (u->op == TokenKind::Not) {
      return tri_not(eval_bool(*u->operand, env));
    }
    return Tri::Maybe;
  }
  if (const auto *b = std::get_if<BinaryExpr>(&e.node)) {
    if (b->op == TokenKind::And) {
      return tri_and(eval_bool(*b->left, env), eval_bool(*b->right, env));
    }
    if (b->op == TokenKind::Or) {
      const Tri l = eval_bool(*b->left, env);
      const Tri r = eval_bool(*b->right, env);
      if (l == Tri::Yes || r == Tri::Yes) {
        return Tri::Yes;
      }
      if (l == Tri::No && r == Tri::No) {
        return Tri::No;
      }
      return Tri::Maybe;
    }
    if (b->op == TokenKind::Less || b->op == TokenKind::LessEq ||
        b->op == TokenKind::Greater || b->op == TokenKind::GreaterEq ||
        b->op == TokenKind::EqEq || b->op == TokenKind::NotEq) {
      return cmp_signs(b->op, eval_abs_value(*b->left, env),
                       eval_abs_value(*b->right, env));
    }
  }
  return Tri::Maybe;
}

[[nodiscard]] Env join_env(const Env &a, const Env &b) {
  Env out;
  std::set<std::string> keys;
  for (const auto &[k, _] : a) {
    keys.insert(k);
  }
  for (const auto &[k, _] : b) {
    keys.insert(k);
  }
  for (const auto &k : keys) {
    const bool ina = a.contains(k);
    const bool inb = b.contains(k);
    if (ina && inb) {
      out[k] = abs_join(a.at(k), b.at(k));
    } else if (ina) {
      out[k] = abs_join(a.at(k), abs_unknown());
    } else {
      out[k] = abs_join(abs_unknown(), b.at(k));
    }
  }
  return out;
}

[[nodiscard]] Signs keep_sign(Signs cur, TokenKind op, long double lit,
                              bool want_true) {
  Signs out = Signs::bottom();
  auto consider = [&](bool has, int sign, bool &flag) {
    if (!has) {
      return;
    }
    const Rel r = rel_of(op, sign, lit);
    const bool keep = want_true ? (r != Rel::Never) : (r != Rel::Always);
    if (keep) {
      flag = true;
    }
  };
  consider(cur.neg, -1, out.neg);
  consider(cur.zero, 0, out.zero);
  consider(cur.pos, 1, out.pos);
  if (out.is_bottom()) {
    return cur;
  }
  return out;
}

[[nodiscard]] Env refine(const Expr &e, Env env, bool want);

[[nodiscard]] Env refine(const Expr &e, Env env, bool want) {
  if (const auto *u = std::get_if<UnaryExpr>(&e.node)) {
    if (u->op == TokenKind::Not) {
      return refine(*u->operand, std::move(env), !want);
    }
    return env;
  }
  if (const auto *b = std::get_if<BinaryExpr>(&e.node)) {
    if (b->op == TokenKind::And) {
      if (want) {
        return refine(*b->right, refine(*b->left, std::move(env), true), true);
      }
      return join_env(refine(*b->left, env, false), refine(*b->right, env, false));
    }
    if (b->op == TokenKind::Or) {
      if (!want) {
        return refine(*b->right, refine(*b->left, std::move(env), false), false);
      }
      return join_env(refine(*b->left, env, true), refine(*b->right, env, true));
    }
    if (b->op == TokenKind::Less || b->op == TokenKind::LessEq ||
        b->op == TokenKind::Greater || b->op == TokenKind::GreaterEq ||
        b->op == TokenKind::EqEq || b->op == TokenKind::NotEq) {
      const Abs left = eval_abs_value(*b->left, env);
      const Abs right = eval_abs_value(*b->right, env);
      const VarExpr *var = nullptr;
      Abs lit = abs_unknown();
      TokenKind op = b->op;
      if (const auto *lv = std::get_if<VarExpr>(&b->left->node); lv && right.exact) {
        var = lv;
        lit = right;
      } else if (const auto *rv = std::get_if<VarExpr>(&b->right->node);
                 rv && left.exact) {
        var = rv;
        lit = left;
        op = flip_cmp(b->op);
      }
      if (var != nullptr && lit.exact) {
        Abs cur = lookup(env, var->name);
        cur.signs = keep_sign(cur.signs, op, *lit.exact, want);
        cur.exact.reset();
        if (cur.signs.neg + cur.signs.zero + cur.signs.pos == 1) {
          if (cur.signs.zero) {
            cur.exact = 0;
          }
        }
        env[var->name] = cur;
      }
    }
  }
  return env;
}

struct Atom {
  const Expr *expr{nullptr};
  bool want_true{true};
};

struct Exit {
  std::vector<Atom> path;
};

struct Sim {
  Env env;
  bool reachable{true};
  std::vector<Exit> exits;
  std::vector<Atom> path;
  int depth{0};
  bool quiet{false};
};

class Analyzer {
 public:
  std::vector<LoopWarning> warnings;
  std::vector<std::string> params;

  void run(const Program &program) {
    Sim sim;
    for (const auto &st : program.statements) {
      sim = exec(*st, std::move(sim));
      if (!sim.reachable) {
        break;
      }
    }
  }

 private:
  void fuzz(Env &env, const std::set<std::string> &names) {
    for (const auto &name : names) {
      env[name] = abs_unknown();
    }
  }

  void consider(SourcePos pos, const Expr *cond, const Stmt *body,
                const Stmt *step, const Env &entry, bool quiet) {
    if (quiet) {
      return;
    }
    std::set<std::string> writes;
    if (body != nullptr) {
      collect_writes(*body, writes);
    }
    if (step != nullptr) {
      collect_writes(*step, writes);
    }
    std::set<std::string> reads;
    if (cond != nullptr) {
      reads_of(*cond, reads);
    }
    bool progress = false;
    for (const auto &name : reads) {
      if (writes.contains(name)) {
        progress = true;
      }
    }
    const Tri cond_tri =
        cond != nullptr ? eval_bool(*cond, entry) : Tri::Yes;
    if (cond_tri == Tri::No) {
      return;
    }
    const bool cond_can_end =
        (cond_tri == Tri::Maybe && progress) ||
        (cond_tri == Tri::Yes && progress && !reads.empty());
    if (cond_can_end) {
      return;
    }

    Sim sim;
    sim.env = entry;
    if (body != nullptr) {
      sim = exec(*body, std::move(sim));
    }
    if (!sim.exits.empty()) {
      warn_blocked_sign(pos, entry, writes, body);
      return;
    }
    warnings.push_back(
        LoopWarning{pos, "Цей цикл не має виходу."});
  }

  void warn_blocked_sign(SourcePos pos, const Env &entry,
                         const std::set<std::string> &writes,
                         const Stmt *body) {
    std::set<std::string> names;
    if (body != nullptr) {
      collect_names_stmt(*body, names);
    }
    std::vector<std::string> ordered;
    for (const auto &p : params) {
      if (names.contains(p) && !writes.contains(p)) {
        ordered.push_back(p);
      }
    }
    for (const auto &name : names) {
      if (writes.contains(name)) {
        continue;
      }
      if (std::find(ordered.begin(), ordered.end(), name) == ordered.end()) {
        ordered.push_back(name);
      }
    }
    const struct {
      bool Signs::*flag;
      Abs (*make)();
      const char *text;
    } probes[] = {
        {&Signs::neg, [] { return Abs{Signs::only_neg(), std::nullopt}; },
         "від'ємн"},
        {&Signs::pos, [] { return Abs{Signs::only_pos(), std::nullopt}; },
         "додатн"},
        {&Signs::zero, [] { return Abs{Signs::only_zero(), 0}; }, "нульов"},
    };
    for (const auto &name : ordered) {
      const Abs cur = lookup(entry, name);
      for (const auto &probe : probes) {
        if (!(cur.signs.*probe.flag)) {
          continue;
        }
        Env forced = entry;
        forced[name] = probe.make();
        Sim sim;
        sim.quiet = true;
        sim.env = forced;
        if (body != nullptr) {
          sim = exec(*body, std::move(sim));
        }
        if (sim.exits.empty()) {
          std::string why = "додатний";
          if (std::string(probe.text) == "від'ємн") {
            why = "від'ємний";
          } else if (std::string(probe.text) == "нульов") {
            why = "дорівнює нулю";
          }
          warnings.push_back(LoopWarning{
              pos, "Якщо " + name + " " + why +
                       ", вихід з циклу не спрацює."});
          return;
        }
      }
    }
  }

  [[nodiscard]] Sim exec(const Stmt &s, Sim sim) {
    if (!sim.reachable || sim.depth > 24) {
      if (sim.depth > 24) {
        sim.exits.push_back(Exit{});
      }
      return sim;
    }
    return std::visit(
        [&](const auto &n) -> Sim {
          using T = std::decay_t<decltype(n)>;
          if constexpr (std::is_same_v<T, LetStmt>) {
            for (const auto &b : n.bindings) {
              sim.env[b.name] = eval_abs_value(*b.init, sim.env);
            }
            return sim;
          } else if constexpr (std::is_same_v<T, AssignStmt>) {
            if (const auto *v = std::get_if<VarExpr>(&n.target->node)) {
              sim.env[v->name] = eval_abs_value(*n.value, sim.env);
            } else if (auto name = root_name(*n.target)) {
              sim.env[*name] = abs_unknown();
            }
            return sim;
          } else if constexpr (std::is_same_v<T, BlockStmt>) {
            for (const auto &st : n.statements) {
              if (!sim.reachable) {
                break;
              }
              sim = exec(*st, std::move(sim));
            }
            return sim;
          } else if constexpr (std::is_same_v<T, IfStmt>) {
            const Tri cond = eval_bool(*n.condition, sim.env);
            const bool go_then = cond != Tri::No;
            const bool go_else = cond != Tri::Yes;
            Sim then_sim = sim;
            Sim else_sim = sim;
            if (go_then) {
              then_sim.env = refine(*n.condition, sim.env, true);
              then_sim.path.push_back(Atom{n.condition.get(), true});
              then_sim.depth = sim.depth + 1;
              then_sim = exec(*n.then_branch, std::move(then_sim));
            }
            if (go_else) {
              else_sim.env = refine(*n.condition, sim.env, false);
              else_sim.path.push_back(Atom{n.condition.get(), false});
              else_sim.depth = sim.depth + 1;
              if (n.else_branch) {
                else_sim = exec(*n.else_branch, std::move(else_sim));
              }
            }
            Sim out = sim;
            out.exits = sim.exits;
            if (go_then) {
              out.exits.insert(out.exits.end(), then_sim.exits.begin(),
                               then_sim.exits.end());
            }
            if (go_else) {
              out.exits.insert(out.exits.end(), else_sim.exits.begin(),
                               else_sim.exits.end());
            }
            const bool then_reach = go_then && then_sim.reachable;
            const bool else_reach = go_else && else_sim.reachable;
            if (!then_reach && !else_reach) {
              out.reachable = false;
              return out;
            }
            if (!then_reach) {
              out.env = else_sim.env;
              out.reachable = true;
              return out;
            }
            if (!else_reach) {
              out.env = then_sim.env;
              out.reachable = true;
              return out;
            }
            out.env = join_env(then_sim.env, else_sim.env);
            out.reachable = true;
            return out;
          } else if constexpr (std::is_same_v<T, WhileStmt>) {
            consider(n.pos, n.condition.get(), n.body.get(), nullptr, sim.env,
                     sim.quiet);
            std::set<std::string> w;
            collect_writes(*n.body, w);
            fuzz(sim.env, w);
            return sim;
          } else if constexpr (std::is_same_v<T, ForStmt>) {
            if (n.init) {
              sim = exec(*n.init, std::move(sim));
            }
            consider(n.pos, n.condition.get(), n.body.get(),
                     n.step ? n.step.get() : nullptr, sim.env, sim.quiet);
            std::set<std::string> w;
            collect_writes(*n.body, w);
            if (n.step) {
              collect_writes(*n.step, w);
            }
            fuzz(sim.env, w);
            return sim;
          } else if constexpr (std::is_same_v<T, ForInStmt>) {
            sim.env[n.first] = abs_unknown();
            if (!n.second.empty()) {
              sim.env[n.second] = abs_unknown();
            }
            std::set<std::string> w;
            collect_writes(*n.body, w);
            fuzz(sim.env, w);
            return sim;
          } else if constexpr (std::is_same_v<T, BreakStmt> ||
                               std::is_same_v<T, ReturnStmt>) {
            Exit ex;
            ex.path = sim.path;
            sim.exits.push_back(std::move(ex));
            sim.reachable = false;
            return sim;
          } else if constexpr (std::is_same_v<T, FunStmt>) {
            const auto saved = params;
            params.clear();
            for (const auto &p : n.params) {
              params.push_back(p.name);
            }
            Sim inner;
            for (const auto &p : n.params) {
              inner.env[p.name] = abs_unknown();
            }
            inner = exec(*n.body, std::move(inner));
            params = saved;
            (void)inner;
            return sim;
          } else if constexpr (std::is_same_v<T, ExprStmt>) {
            if (const auto *u = std::get_if<UpdateExpr>(&n.expr->node)) {
              if (auto name = root_name(*u->target)) {
                sim.env[*name] = abs_unknown();
              }
            }
            return sim;
          } else {
            return sim;
          }
        },
        s.node);
  }
};

} // namespace

std::vector<LoopWarning> find_loop_warnings(const Program &program) {
  Analyzer analyzer;
  analyzer.run(program);
  return analyzer.warnings;
}

} // namespace calc
