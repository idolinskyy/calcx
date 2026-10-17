// AST виразів і операторів (масиви, записи, індексація, поля).
#pragma once

#include "calc/error.hpp"
#include "calc/token.hpp"
#include "calc/value.hpp"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace calc {

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct LiteralExpr {
  Value value;
  SourcePos pos;
};

struct VarExpr {
  std::string name;
  SourcePos pos;
};

struct UnaryExpr {
  TokenKind op;
  ExprPtr operand;
  SourcePos pos;
};

struct BinaryExpr {
  TokenKind op;
  ExprPtr left;
  ExprPtr right;
  SourcePos pos;
};

struct CallExpr {
  std::string callee;
  SourcePos callee_pos;
  std::vector<ExprPtr> args;
  SourcePos pos;
};

struct ArrayExpr {
  std::vector<ExprPtr> elements;
  SourcePos pos;
};

struct RecordField {
  std::string name;
  SourcePos name_pos;
  ExprPtr value;
};

struct RecordExpr {
  std::vector<RecordField> fields;
  SourcePos pos;
};

struct IndexExpr {
  ExprPtr object;
  ExprPtr index;
  SourcePos pos;
};

struct FieldExpr {
  ExprPtr object;
  std::string field;
  SourcePos field_pos;
  SourcePos pos;
};

struct Expr {
  std::variant<LiteralExpr, VarExpr, UnaryExpr, BinaryExpr, CallExpr, ArrayExpr,
               RecordExpr, IndexExpr, FieldExpr>
      node;
};

struct LetBinding {
  std::string name;
  SourcePos name_pos;
  ExprPtr init;
};

struct LetStmt {
  std::vector<LetBinding> bindings;
  SourcePos pos;
};

struct PrintStmt {
  bool has_format{false};
  std::string format;
  SourcePos format_pos;
  std::vector<ExprPtr> args;
  SourcePos pos;
};

/// Присвоєння: змінна, a[i] або a.field.
struct AssignStmt {
  ExprPtr target; ///< VarExpr | IndexExpr | FieldExpr
  ExprPtr value;
  SourcePos pos;
};

struct BlockStmt {
  std::vector<StmtPtr> statements;
  SourcePos pos;
};

struct IfStmt {
  ExprPtr condition;
  StmtPtr then_branch;
  StmtPtr else_branch;
  SourcePos pos;
};

struct WhileStmt {
  ExprPtr condition;
  StmtPtr body;
  SourcePos pos;
};

struct ForStmt {
  StmtPtr init;
  ExprPtr condition;
  StmtPtr step;
  StmtPtr body;
  SourcePos pos;
};

/// for (x in xs) / for (a, b in xs) — масив або запис.
struct ForInStmt {
  std::string first;
  SourcePos first_pos;
  std::string second;  ///< порожньо, якщо одна змінна
  SourcePos second_pos;
  ExprPtr iterable;
  StmtPtr body;
  SourcePos pos;
};

struct BreakStmt {
  SourcePos pos;
};

struct ContinueStmt {
  SourcePos pos;
};

struct FunParam {
  std::string name;
  SourcePos pos;
};

struct FunStmt {
  std::string name;
  SourcePos name_pos;
  std::vector<FunParam> params;
  StmtPtr body;
  SourcePos pos;
};

struct ReturnStmt {
  ExprPtr value;
  SourcePos pos;
};

/// Вираз як оператор (результат відкидається): `push(a, 1);`.
struct ExprStmt {
  ExprPtr expr;
  SourcePos pos;
};

struct Stmt {
  std::variant<LetStmt, PrintStmt, AssignStmt, BlockStmt, IfStmt, WhileStmt,
               ForStmt, ForInStmt, BreakStmt, ContinueStmt, FunStmt, ReturnStmt,
               ExprStmt>
      node;
};

struct Program {
  std::vector<StmtPtr> statements;
};

template <class T> [[nodiscard]] StmtPtr make_stmt(T &&node) {
  auto s = std::make_unique<Stmt>();
  s->node = std::forward<T>(node);
  return s;
}

} // namespace calc
