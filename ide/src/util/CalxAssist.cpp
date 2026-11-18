#include "util/CalxAssist.h"

#include "calc/builtin.hpp"
#include "calc/constants.hpp"
#include "calc/repl.hpp"
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
          QStringLiteral("or"),       QStringLiteral("not"),
          QStringLiteral("has")};
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
      QStringLiteral("push"),  QStringLiteral("pop"),  QStringLiteral("clone"),
      QStringLiteral("same"),  QStringLiteral("int"),  QStringLiteral("real"),
      QStringLiteral("bool"),  QStringLiteral("complex"),
      QStringLiteral("rational"),
      QStringLiteral("is_bool"),     QStringLiteral("is_int"),
      QStringLiteral("is_rational"), QStringLiteral("is_real"),
      QStringLiteral("is_complex"),  QStringLiteral("is_number"),
      QStringLiteral("is_array"),    QStringLiteral("is_record"),
      QStringLiteral("is_string")};
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

QString builtinHover(calc::BuiltinId id) {
  switch (id) {
  case calc::BuiltinId::Sin:
    return QStringLiteral("sin(x) — синус. Кут у радіанах.");
  case calc::BuiltinId::Cos:
    return QStringLiteral("cos(x) — косинус. Кут у радіанах.");
  case calc::BuiltinId::Tan:
    return QStringLiteral("tan(x) — тангенс. Кут у радіанах.");
  case calc::BuiltinId::Asin:
    return QStringLiteral("asin(x) — арксинус, результат у радіанах.");
  case calc::BuiltinId::Acos:
    return QStringLiteral("acos(x) — арккосинус, результат у радіанах.");
  case calc::BuiltinId::Atan:
    return QStringLiteral("atan(x) — арктангенс, результат у радіанах.");
  case calc::BuiltinId::Atan2:
    return QStringLiteral(
        "atan2(y, x) — кут від осі x до точки (x, y), у радіанах.");
  case calc::BuiltinId::Sinh:
    return QStringLiteral("sinh(x) — гіперболічний синус.");
  case calc::BuiltinId::Cosh:
    return QStringLiteral("cosh(x) — гіперболічний косинус.");
  case calc::BuiltinId::Tanh:
    return QStringLiteral("tanh(x) — гіперболічний тангенс.");
  case calc::BuiltinId::Sqrt:
    return QStringLiteral(
        "sqrt(x) — квадратний корінь. Для від'ємного x дає комплексне.");
  case calc::BuiltinId::Exp:
    return QStringLiteral("exp(x) — e у степені x.");
  case calc::BuiltinId::Ln:
    return QStringLiteral("ln(x) або log(x) — натуральний логарифм.");
  case calc::BuiltinId::Log10:
    return QStringLiteral("log10(x) — десятковий логарифм.");
  case calc::BuiltinId::Log2:
    return QStringLiteral("log2(x) — двійковий логарифм.");
  case calc::BuiltinId::Pow:
    return QStringLiteral("pow(a, b) — a у степені b.");
  case calc::BuiltinId::Hypot:
    return QStringLiteral("hypot(x, y) — довжина вектора, sqrt(x² + y²).");
  case calc::BuiltinId::Abs:
    return QStringLiteral("abs(x) — модуль числа.");
  case calc::BuiltinId::Sign:
    return QStringLiteral("sign(x) — знак: −1, 0 або 1.");
  case calc::BuiltinId::Re:
    return QStringLiteral("re(z) — дійсна частина.");
  case calc::BuiltinId::Im:
    return QStringLiteral("im(z) — уявна частина.");
  case calc::BuiltinId::Arg:
    return QStringLiteral("arg(z) — аргумент комплексного числа, у радіанах.");
  case calc::BuiltinId::Conj:
    return QStringLiteral("conj(z) — спряжене число.");
  case calc::BuiltinId::Floor:
    return QStringLiteral("floor(x) — округлення вниз до цілого (як дійсне).");
  case calc::BuiltinId::Ceil:
    return QStringLiteral("ceil(x) — округлення вгору до цілого (як дійсне).");
  case calc::BuiltinId::Round:
    return QStringLiteral(
        "round(x) — до найближчого цілого; половина йде від нуля.");
  case calc::BuiltinId::Trunc:
    return QStringLiteral("trunc(x) — відкидає дробову частину, до нуля.");
  case calc::BuiltinId::Min:
    return QStringLiteral("min(a, b) — менше з двох чисел.");
  case calc::BuiltinId::Max:
    return QStringLiteral("max(a, b) — більше з двох чисел.");
  case calc::BuiltinId::Deg:
    return QStringLiteral("deg(x) — радіани в градуси.");
  case calc::BuiltinId::Rad:
    return QStringLiteral("rad(x) — градуси в радіани.");
  case calc::BuiltinId::Len:
    return QStringLiteral("len(x) — кількість елементів масиву або полів запису.");
  case calc::BuiltinId::Push:
    return QStringLiteral("push(arr, x) — додає x у кінець масиву і повертає його.");
  case calc::BuiltinId::Pop:
    return QStringLiteral("pop(arr) — знімає останній елемент масиву і повертає його.");
  case calc::BuiltinId::Clone:
    return QStringLiteral(
        "clone(x) — повна копія масиву або запису. Число повертається як є.");
  case calc::BuiltinId::Same:
    return QStringLiteral(
        "same(a, b) — true, якщо це той самий масив або запис у пам'яті.");
  case calc::BuiltinId::Int:
    return QStringLiteral(
        "int(x) — до цілого, дробова частина відкидається. Inf і NaN — помилка.");
  case calc::BuiltinId::RealCast:
    return QStringLiteral("real(x) — до дійсного числа.");
  case calc::BuiltinId::BoolCast:
    return QStringLiteral("bool(x) — 0 стає false, інше число — true.");
  case calc::BuiltinId::ComplexCast:
    return QStringLiteral(
        "complex(re) або complex(re, im) — комплексне число.");
  case calc::BuiltinId::RationalCast:
    return QStringLiteral("rational(n, d) — дріб n/d.");
  case calc::BuiltinId::IsBool:
    return QStringLiteral("is_bool(x) — true, якщо значення саме логічне.");
  case calc::BuiltinId::IsInt:
    return QStringLiteral(
        "is_int(x) — true, якщо значення саме ціле. 2.0 — це не ціле.");
  case calc::BuiltinId::IsRational:
    return QStringLiteral("is_rational(x) — true, якщо значення — дріб.");
  case calc::BuiltinId::IsReal:
    return QStringLiteral("is_real(x) — true, якщо значення дійсне.");
  case calc::BuiltinId::IsComplex:
    return QStringLiteral("is_complex(x) — true, якщо значення комплексне.");
  case calc::BuiltinId::IsNumber:
    return QStringLiteral(
        "is_number(x) — true для цілого, дробу, дійсного і комплексного.");
  case calc::BuiltinId::IsArray:
    return QStringLiteral("is_array(x) — true, якщо значення — масив.");
  case calc::BuiltinId::IsRecord:
    return QStringLiteral("is_record(x) — true, якщо значення — запис.");
  case calc::BuiltinId::IsString:
    return QStringLiteral(
        "is_string(x) — true для ключа запису в циклі for.");
  }
  return {};
}

QString keywordHover(const QString &ident) {
  if (ident == QLatin1String("let")) {
    return QStringLiteral(
        "let — оголосити змінну. Кілька імен через кому:\n"
        "let a = 1, b = 2, c = complex(1, 2);");
  }
  if (ident == QLatin1String("print")) {
    return QStringLiteral(
        "print — вивести значення. У файлі в кінці потрібна крапка з комою.");
  }
  if (ident == QLatin1String("if")) {
    return QStringLiteral(
        "if — гілка. Умова має бути true або false, не число.");
  }
  if (ident == QLatin1String("else")) {
    return QStringLiteral("else — гілка, коли if не спрацював.");
  }
  if (ident == QLatin1String("while")) {
    return QStringLiteral("while — цикл, поки умова true.");
  }
  if (ident == QLatin1String("for")) {
    return QStringLiteral(
        "for x in масив або запис — цикл по елементах. Для запису x — це ключ.");
  }
  if (ident == QLatin1String("in")) {
    return QStringLiteral("in — у циклі for: for x in xs { ... }");
  }
  if (ident == QLatin1String("break")) {
    return QStringLiteral("break — вийти з циклу.");
  }
  if (ident == QLatin1String("continue")) {
    return QStringLiteral("continue — одразу до наступної ітерації циклу.");
  }
  if (ident == QLatin1String("fun")) {
    return QStringLiteral(
        "fun ім'я(аргументи) { ... } — оголосити функцію.");
  }
  if (ident == QLatin1String("return")) {
    return QStringLiteral("return — повернути значення з функції.");
  }
  if (ident == QLatin1String("true")) {
    return QStringLiteral("true — логічне «так».");
  }
  if (ident == QLatin1String("false")) {
    return QStringLiteral("false — логічне «ні».");
  }
  if (ident == QLatin1String("and")) {
    return QStringLiteral("and — логічне «і». Обидва боки мають бути true/false.");
  }
  if (ident == QLatin1String("or")) {
    return QStringLiteral("or — логічне «або». Обидва боки мають бути true/false.");
  }
  if (ident == QLatin1String("not")) {
    return QStringLiteral("not — логічне «ні».");
  }
  if (ident == QLatin1String("has")) {
    return QStringLiteral(
        "запис has поле — true, якщо в записі є таке поле.\n"
        "Правий бік — ім'я поля, не вираз.");
  }
  return {};
}

QString constantGloss(const QString &ident) {
  if (ident == QLatin1String("pi")) {
    return QStringLiteral("число π");
  }
  if (ident == QLatin1String("e")) {
    return QStringLiteral("основа натурального логарифма");
  }
  if (ident == QLatin1String("tau")) {
    return QStringLiteral("2π");
  }
  if (ident == QLatin1String("phi")) {
    return QStringLiteral("золотий перетин");
  }
  if (ident == QLatin1String("sqrt2")) {
    return QStringLiteral("корінь з 2");
  }
  if (ident == QLatin1String("sqrt3")) {
    return QStringLiteral("корінь з 3");
  }
  if (ident == QLatin1String("ln2")) {
    return QStringLiteral("ln(2)");
  }
  if (ident == QLatin1String("ln10")) {
    return QStringLiteral("ln(10)");
  }
  if (ident == QLatin1String("i")) {
    return QStringLiteral("уявна одиниця");
  }
  if (ident == QLatin1String("c")) {
    return QStringLiteral("швидкість світла, м/с");
  }
  if (ident == QLatin1String("G")) {
    return QStringLiteral("гравітаційна стала");
  }
  if (ident == QLatin1String("g")) {
    return QStringLiteral("прискорення вільного падіння, м/с²");
  }
  if (ident == QLatin1String("h")) {
    return QStringLiteral("стала Планка");
  }
  if (ident == QLatin1String("hbar")) {
    return QStringLiteral("зведена стала Планка");
  }
  if (ident == QLatin1String("NA")) {
    return QStringLiteral("стала Авогадро");
  }
  if (ident == QLatin1String("kB") || ident == QLatin1String("k")) {
    return QStringLiteral("стала Больцмана");
  }
  return {};
}

QString userFunctionHover(const std::vector<calc::UserFunction> &funs,
                          const QString &ident) {
  const auto name = ident.toStdString();
  for (const auto &fn : funs) {
    if (fn.name != name) {
      continue;
    }
    if (!fn.has_signature) {
      return QStringLiteral("%1 — оголошена функція").arg(ident);
    }
    QStringList params;
    for (const auto &p : fn.params) {
      params << QString::fromStdString(p);
    }
    return QStringLiteral("%1(%2) — оголошена функція")
        .arg(ident, params.join(QStringLiteral(", ")));
  }
  return {};
}

QString hoverTextFor(const QString &ident) {
  const auto name = ident.toStdString();
  if (const auto bid = calc::find_builtin(name)) {
    return builtinHover(*bid);
  }
  if (const auto c = calc::find_constant(name)) {
    const QString gloss = constantGloss(ident);
    const QString value = QString::fromStdString(calc::format_value(*c));
    if (gloss.isEmpty()) {
      return QStringLiteral("константа %1 = %2").arg(ident, value);
    }
    return QStringLiteral("константа %1 — %2\n%3").arg(ident, gloss, value);
  }
  if (const QString tip = keywordHover(ident); !tip.isEmpty()) {
    return tip;
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
