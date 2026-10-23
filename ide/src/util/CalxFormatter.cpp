#include "util/CalxFormatter.h"

#include "calc/ast.hpp"
#include "calc/engine.hpp"
#include "calc/lexer.hpp"
#include "calc/parser.hpp"
#include "calc/token.hpp"
#include "calc/value.hpp"

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace calx {
namespace {

struct Comment {
  int line{1};
  QString text;
  bool lone{false};
  bool used{false};
};

[[nodiscard]] std::vector<Comment> extractComments(const QString &source) {
  std::vector<Comment> out;
  int line = 1;
  bool in_string = false;
  bool escape = false;
  bool saw_code = false;

  for (int i = 0; i < source.size(); ++i) {
    const QChar c = source[i];
    if (in_string) {
      if (escape) {
        escape = false;
      } else if (c == QLatin1Char('\\')) {
        escape = true;
      } else if (c == QLatin1Char('"')) {
        in_string = false;
      }
      if (c == QLatin1Char('\n')) {
        ++line;
        saw_code = false;
        in_string = false;
        escape = false;
      }
      continue;
    }
    if (c == QLatin1Char('"')) {
      in_string = true;
      saw_code = true;
      continue;
    }
    if (c == QLatin1Char('\n')) {
      ++line;
      saw_code = false;
      continue;
    }
    if (c == QLatin1Char('/') && i + 1 < source.size() &&
        source[i + 1] == QLatin1Char('/')) {
      const int start = i;
      while (i < source.size() && source[i] != QLatin1Char('\n')) {
        ++i;
      }
      Comment cm;
      cm.line = line;
      cm.text = source.mid(start, i - start).trimmed();
      cm.lone = !saw_code;
      out.push_back(std::move(cm));
      if (i < source.size() && source[i] == QLatin1Char('\n')) {
        ++line;
        saw_code = false;
      }
      continue;
    }
    if (!c.isSpace()) {
      saw_code = true;
    }
  }
  return out;
}

[[nodiscard]] int binPrec(calc::TokenKind op) {
  using K = calc::TokenKind;
  switch (op) {
  case K::Or:
    return 1;
  case K::And:
    return 2;
  case K::EqEq:
  case K::NotEq:
    return 3;
  case K::Less:
  case K::LessEq:
  case K::Greater:
  case K::GreaterEq:
    return 4;
  case K::Plus:
  case K::Minus:
    return 5;
  case K::Star:
  case K::Slash:
  case K::Percent:
    return 6;
  case K::StarStar:
    return 7;
  default:
    return 0;
  }
}

[[nodiscard]] bool rightAssoc(calc::TokenKind op) {
  return op == calc::TokenKind::StarStar;
}

[[nodiscard]] const char *opText(calc::TokenKind op) {
  using K = calc::TokenKind;
  switch (op) {
  case K::Plus:
    return "+";
  case K::Minus:
    return "-";
  case K::Star:
    return "*";
  case K::StarStar:
    return "**";
  case K::Slash:
    return "/";
  case K::Percent:
    return "%";
  case K::EqEq:
    return "==";
  case K::NotEq:
    return "!=";
  case K::Less:
    return "<";
  case K::LessEq:
    return "<=";
  case K::Greater:
    return ">";
  case K::GreaterEq:
    return ">=";
  case K::And:
    return "and";
  case K::Or:
    return "or";
  case K::Not:
    return "not";
  default:
    return "?";
  }
}

[[nodiscard]] QString escapeString(const std::string &s) {
  // Рядки в AST — UTF-8; ітеруємо Unicode, а не байти (інакше кирилиця
  // перетворюється на mojibake через QChar(byte)).
  const QString u =
      QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
  QString out;
  out.reserve(u.size() + 2);
  out += QLatin1Char('"');
  for (QChar c : u) {
    switch (c.unicode()) {
    case u'\\':
      out += QStringLiteral("\\\\");
      break;
    case u'"':
      out += QStringLiteral("\\\"");
      break;
    case u'\n':
      out += QStringLiteral("\\n");
      break;
    case u'\t':
      out += QStringLiteral("\\t");
      break;
    case u'\r':
      out += QStringLiteral("\\r");
      break;
    default:
      out += c;
      break;
    }
  }
  out += QLatin1Char('"');
  return out;
}

class Printer {
public:
  explicit Printer(std::vector<Comment> comments)
      : comments_(std::move(comments)) {}

  [[nodiscard]] QString printProgram(const calc::Program &prog) {
    for (const auto &stmt : prog.statements) {
      flushLoneBeforeAt(stmtLine(*stmt), 0);
      printStmt(*stmt, 0);
      attachTrailing(stmtLine(*stmt));
      ensureNewline();
    }
    flushRemainingLone();
    if (!out_.isEmpty() && !out_.endsWith(QLatin1Char('\n'))) {
      out_ += QLatin1Char('\n');
    }
    return out_;
  }

private:
  QString out_;
  std::vector<Comment> comments_;

  void write(QStringView s) { out_ += s; }
  void write(const char *s) { out_ += QLatin1String(s); }
  void write(const QString &s) { out_ += s; }

  void ensureNewline() {
    if (!out_.isEmpty() && !out_.endsWith(QLatin1Char('\n'))) {
      out_ += QLatin1Char('\n');
    }
  }

  void padIndent(int indent) {
    if (out_.isEmpty() || out_.endsWith(QLatin1Char('\n'))) {
      out_ += QString(indent * 2, QLatin1Char(' '));
    }
  }

  [[nodiscard]] static int stmtLine(const calc::Stmt &s) {
    return static_cast<int>(std::visit(
        [](const auto &n) -> std::uint32_t { return n.pos.line; }, s.node));
  }

  void flushLoneBeforeAt(int line, int indent) {
    for (auto &c : comments_) {
      if (c.used || !c.lone || c.line > line) {
        continue;
      }
      padIndent(indent);
      write(c.text);
      ensureNewline();
      c.used = true;
    }
  }

  void flushRemainingLone() {
    for (auto &c : comments_) {
      if (c.used || !c.lone) {
        continue;
      }
      padIndent(0);
      write(c.text);
      ensureNewline();
      c.used = true;
    }
  }

  void attachTrailing(int line) {
    for (auto &c : comments_) {
      if (c.used || c.lone || c.line != line) {
        continue;
      }
      if (out_.endsWith(QLatin1Char('\n'))) {
        out_.chop(1);
      }
      write(QStringLiteral("  "));
      write(c.text);
      ensureNewline();
      c.used = true;
    }
  }

  void printExpr(const calc::Expr &e, int parent_prec, bool is_right_child) {
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, calc::LiteralExpr>) {
            write(QString::fromStdString(calc::format_value(node.value)));
          } else if constexpr (std::is_same_v<T, calc::VarExpr>) {
            write(QString::fromStdString(node.name));
          } else if constexpr (std::is_same_v<T, calc::UnaryExpr>) {
            constexpr int prec = 8;
            const bool paren = prec < parent_prec;
            if (paren) {
              write("(");
            }
            write(opText(node.op));
            if (node.op == calc::TokenKind::Not) {
              write(" ");
            }
            printExpr(*node.operand, prec, true);
            if (paren) {
              write(")");
            }
          } else if constexpr (std::is_same_v<T, calc::BinaryExpr>) {
            const int prec = binPrec(node.op);
            // Дужки, якщо слабший за батька, або той самий пріоритет на
            // «незручній» стороні асоціативності.
            bool paren = prec < parent_prec;
            if (prec == parent_prec) {
              if (rightAssoc(node.op)) {
                paren = !is_right_child; // a ** (b ** c)
              } else {
                paren = is_right_child; // (a - b) - c  vs  a - (b - c)
              }
            }
            if (paren) {
              write("(");
            }
            printExpr(*node.left, prec, false);
            write(" ");
            write(opText(node.op));
            write(" ");
            printExpr(*node.right, prec, true);
            if (paren) {
              write(")");
            }
          } else if constexpr (std::is_same_v<T, calc::CallExpr>) {
            write(QString::fromStdString(node.callee));
            write("(");
            for (std::size_t i = 0; i < node.args.size(); ++i) {
              if (i) {
                write(", ");
              }
              printExpr(*node.args[i], 0, false);
            }
            write(")");
          } else if constexpr (std::is_same_v<T, calc::ArrayExpr>) {
            write("[");
            for (std::size_t i = 0; i < node.elements.size(); ++i) {
              if (i) {
                write(", ");
              }
              printExpr(*node.elements[i], 0, false);
            }
            write("]");
          } else if constexpr (std::is_same_v<T, calc::RecordExpr>) {
            write("{");
            for (std::size_t i = 0; i < node.fields.size(); ++i) {
              if (i) {
                write(", ");
              }
              write(QString::fromStdString(node.fields[i].name));
              write(": ");
              printExpr(*node.fields[i].value, 0, false);
            }
            write("}");
          } else if constexpr (std::is_same_v<T, calc::IndexExpr>) {
            printExpr(*node.object, 9, false);
            write("[");
            printExpr(*node.index, 0, false);
            write("]");
          } else if constexpr (std::is_same_v<T, calc::FieldExpr>) {
            printExpr(*node.object, 9, false);
            write(".");
            write(QString::fromStdString(node.field));
          }
        },
        e.node);
  }

  void printBlockBody(const calc::BlockStmt &bl, int indent) {
    write("{");
    if (bl.statements.empty()) {
      write("}");
      return;
    }
    ensureNewline();
    for (const auto &inner : bl.statements) {
      flushLoneBeforeAt(stmtLine(*inner), indent + 1);
      printStmt(*inner, indent + 1);
      attachTrailing(stmtLine(*inner));
      ensureNewline();
    }
    padIndent(indent);
    write("}");
  }

  void printStmtBody(const calc::Stmt &s, int indent) {
    if (const auto *bl = std::get_if<calc::BlockStmt>(&s.node)) {
      printBlockBody(*bl, indent);
    } else {
      ensureNewline();
      printStmt(s, indent + 1);
    }
  }

  void printForHeadStmt(const calc::Stmt &s) {
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, calc::LetStmt>) {
            write("let ");
            for (std::size_t i = 0; i < node.bindings.size(); ++i) {
              if (i) {
                write(", ");
              }
              write(QString::fromStdString(node.bindings[i].name));
              write(" = ");
              printExpr(*node.bindings[i].init, 0, false);
            }
          } else if constexpr (std::is_same_v<T, calc::AssignStmt>) {
            printExpr(*node.target, 0, false);
            write(" = ");
            printExpr(*node.value, 0, false);
          }
        },
        s.node);
  }

  void printStmt(const calc::Stmt &s, int indent) {
    padIndent(indent);
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, calc::LetStmt>) {
            write("let ");
            for (std::size_t i = 0; i < node.bindings.size(); ++i) {
              if (i) {
                write(", ");
              }
              write(QString::fromStdString(node.bindings[i].name));
              write(" = ");
              printExpr(*node.bindings[i].init, 0, false);
            }
            write(";");
          } else if constexpr (std::is_same_v<T, calc::PrintStmt>) {
            write("print(");
            if (node.has_format) {
              write(escapeString(node.format));
              for (const auto &a : node.args) {
                write(", ");
                printExpr(*a, 0, false);
              }
            } else if (!node.args.empty()) {
              printExpr(*node.args[0], 0, false);
            }
            write(");");
          } else if constexpr (std::is_same_v<T, calc::AssignStmt>) {
            printExpr(*node.target, 0, false);
            write(" = ");
            printExpr(*node.value, 0, false);
            write(";");
          } else if constexpr (std::is_same_v<T, calc::BlockStmt>) {
            printBlockBody(node, indent);
          } else if constexpr (std::is_same_v<T, calc::IfStmt>) {
            const calc::IfStmt *cur = &node;
            while (true) {
              write("if (");
              printExpr(*cur->condition, 0, false);
              write(") ");
              printStmtBody(*cur->then_branch, indent);
              if (!cur->else_branch) {
                break;
              }
              write(" else ");
              if (const auto *next =
                      std::get_if<calc::IfStmt>(&cur->else_branch->node)) {
                cur = next;
                continue;
              }
              printStmtBody(*cur->else_branch, indent);
              break;
            }
          } else if constexpr (std::is_same_v<T, calc::WhileStmt>) {
            write("while (");
            printExpr(*node.condition, 0, false);
            write(") ");
            printStmtBody(*node.body, indent);
          } else if constexpr (std::is_same_v<T, calc::ForStmt>) {
            write("for (");
            if (node.init) {
              printForHeadStmt(*node.init);
            }
            write("; ");
            if (node.condition) {
              printExpr(*node.condition, 0, false);
            }
            write("; ");
            if (node.step) {
              printForHeadStmt(*node.step);
            }
            write(") ");
            printStmtBody(*node.body, indent);
          } else if constexpr (std::is_same_v<T, calc::ForInStmt>) {
            write("for (");
            write(QString::fromStdString(node.first));
            if (!node.second.empty()) {
              write(", ");
              write(QString::fromStdString(node.second));
            }
            write(" in ");
            printExpr(*node.iterable, 0, false);
            write(") ");
            printStmtBody(*node.body, indent);
          } else if constexpr (std::is_same_v<T, calc::BreakStmt>) {
            write("break;");
          } else if constexpr (std::is_same_v<T, calc::ContinueStmt>) {
            write("continue;");
          } else if constexpr (std::is_same_v<T, calc::FunStmt>) {
            write("fun ");
            write(QString::fromStdString(node.name));
            write("(");
            for (std::size_t i = 0; i < node.params.size(); ++i) {
              if (i) {
                write(", ");
              }
              write(QString::fromStdString(node.params[i].name));
            }
            write(") ");
            printStmtBody(*node.body, indent);
          } else if constexpr (std::is_same_v<T, calc::ReturnStmt>) {
            write("return");
            if (node.value) {
              write(" ");
              printExpr(*node.value, 0, false);
            }
            write(";");
          } else if constexpr (std::is_same_v<T, calc::ExprStmt>) {
            printExpr(*node.expr, 0, false);
            write(";");
          }
        },
        s.node);
  }
};

} // namespace

FormatResult formatSource(const QString &source) {
  FormatResult r;
  if (source.trimmed().isEmpty()) {
    r.ok = true;
    r.text = source.endsWith(QLatin1Char('\n')) || source.isEmpty()
                 ? source
                 : source + QLatin1Char('\n');
    if (r.text.isEmpty()) {
      r.text = QStringLiteral("\n");
    }
    return r;
  }
  const auto src = source.toStdString();
  auto lexed = calc::tokenize(src);
  if (lexed.error.code != calc::ErrorCode::Ok) {
    r.ok = false;
    r.error = QStringLiteral("помилка:%1:%2: %3")
                  .arg(lexed.error.pos.line)
                  .arg(lexed.error.pos.column)
                  .arg(lexed.error.message.empty()
                           ? QString::fromStdString(
                                 calc::error_message(lexed.error.code))
                           : QString::fromStdString(lexed.error.message));
    return r;
  }
  auto parsed = calc::parse(lexed.tokens);
  if (!parsed) {
    const auto &d = parsed.error();
    r.ok = false;
    r.error = QStringLiteral("помилка:%1:%2: %3")
                  .arg(d.pos.line)
                  .arg(d.pos.column)
                  .arg(d.message.empty()
                           ? QString::fromStdString(calc::error_message(d.code))
                           : QString::fromStdString(d.message));
    return r;
  }

  Printer p(extractComments(source));
  r.text = p.printProgram(*parsed);
  r.ok = true;
  return r;
}

} // namespace calx
