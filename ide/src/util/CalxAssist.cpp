#include "util/CalxAssist.h"

#include "calc/builtin.hpp"
#include "calc/constants.hpp"
#include "calc/value.hpp"

#include <QDir>

namespace calx {

QStringList keywords() {
  return {QStringLiteral("let"),      QStringLiteral("print"),
          QStringLiteral("if"),       QStringLiteral("else"),
          QStringLiteral("while"),    QStringLiteral("for"),
          QStringLiteral("in"),       QStringLiteral("break"),
          QStringLiteral("continue"), QStringLiteral("fun"),
          QStringLiteral("return"),   QStringLiteral("true"),
          QStringLiteral("false"),    QStringLiteral("and"),
          QStringLiteral("or"),       QStringLiteral("not")};
}

QStringList builtinNames() {
  return {
      QStringLiteral("sin"),   QStringLiteral("cos"),  QStringLiteral("tan"),
      QStringLiteral("asin"),  QStringLiteral("acos"), QStringLiteral("atan"),
      QStringLiteral("atan2"), QStringLiteral("sinh"), QStringLiteral("cosh"),
      QStringLiteral("tanh"),  QStringLiteral("sqrt"), QStringLiteral("exp"),
      QStringLiteral("ln"),    QStringLiteral("log"),  QStringLiteral("log10"),
      QStringLiteral("log2"),  QStringLiteral("pow"),  QStringLiteral("hypot"),
      QStringLiteral("abs"),   QStringLiteral("sign"), QStringLiteral("re"),
      QStringLiteral("im"),    QStringLiteral("arg"),  QStringLiteral("conj"),
      QStringLiteral("floor"), QStringLiteral("ceil"), QStringLiteral("round"),
      QStringLiteral("trunc"), QStringLiteral("min"),  QStringLiteral("max"),
      QStringLiteral("deg"),   QStringLiteral("rad"),  QStringLiteral("len"),
      QStringLiteral("push"),  QStringLiteral("pop")};
}

QStringList constantNames() {
  return {QStringLiteral("pi"),      QStringLiteral("e"),
          QStringLiteral("tau"),     QStringLiteral("phi"),
          QStringLiteral("sqrt2"),   QStringLiteral("sqrt3"),
          QStringLiteral("ln2"),     QStringLiteral("ln10"),
          QStringLiteral("i"),       QStringLiteral("c"),
          QStringLiteral("G"),       QStringLiteral("h"),
          QStringLiteral("hbar"),    QStringLiteral("qe"),
          QStringLiteral("me"),      QStringLiteral("mp"),
          QStringLiteral("mn"),      QStringLiteral("u"),
          QStringLiteral("eps0"),    QStringLiteral("mu0"),
          QStringLiteral("alpha"),   QStringLiteral("sigma"),
          QStringLiteral("g"),       QStringLiteral("atm"),
          QStringLiteral("Ry"),      QStringLiteral("a0"),
          QStringLiteral("NA"),      QStringLiteral("kB"),
          QStringLiteral("k"),       QStringLiteral("R"),
          QStringLiteral("F"),       QStringLiteral("Vm"),
          QStringLiteral("celsius0")};
}

QStringList completionBase() {
  QStringList out = keywords();
  out += builtinNames();
  out += constantNames();
  out << QStringLiteral(":help") << QStringLiteral(":clear");
  out.removeDuplicates();
  out.sort(Qt::CaseInsensitive);
  return out;
}

QString hoverTextFor(const QString &ident) {
  const auto name = ident.toStdString();
  if (const auto bid = calc::find_builtin(name)) {
    const auto arity = calc::builtin_arity(*bid);
    return QStringLiteral("builtin %1 (%2 аргумент%3)")
        .arg(ident)
        .arg(arity)
        .arg(arity == 1 ? QString() : QStringLiteral("и"));
  }
  if (const auto c = calc::find_constant(name)) {
    return QStringLiteral("константа %1 = %2")
        .arg(ident)
        .arg(QString::fromStdString(calc::format_value(*c)));
  }
  if (keywords().contains(ident)) {
    return QStringLiteral("ключове слово");
  }
  return {};
}

int indentDepthAt(const QString &text, int cursorPos) {
  int depth = 0;
  bool in_string = false;
  bool escape = false;
  const int end = qMin(cursorPos, text.size());
  for (int i = 0; i < end; ++i) {
    const QChar c = text[i];
    if (in_string) {
      if (escape) {
        escape = false;
      } else if (c == QLatin1Char('\\')) {
        escape = true;
      } else if (c == QLatin1Char('"')) {
        in_string = false;
      }
      continue;
    }
    if (c == QLatin1Char('"')) {
      in_string = true;
    } else if (c == QLatin1Char('{') || c == QLatin1Char('(') ||
               c == QLatin1Char('[')) {
      ++depth;
    } else if (c == QLatin1Char('}') || c == QLatin1Char(')') ||
               c == QLatin1Char(']')) {
      depth = qMax(0, depth - 1);
    }
  }
  return depth;
}

QString historyFilePath() {
  const QString home = QDir::homePath();
  if (!home.isEmpty()) {
    return home + QStringLiteral("/.calx_history");
  }
  return QStringLiteral(".calx_history");
}

} // namespace calx
