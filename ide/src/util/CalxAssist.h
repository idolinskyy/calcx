#pragma once

#include "calc/repl.hpp"

#include <QString>
#include <QStringList>

namespace calx {

[[nodiscard]] QStringList keywords();
[[nodiscard]] QStringList builtinNames();
[[nodiscard]] QStringList constantNames();
[[nodiscard]] QStringList completionBase();

[[nodiscard]] QString hoverTextFor(const QString &ident);
/// Підпис оголошеної функції. Порожній рядок, якщо такого імені немає.
[[nodiscard]] QString
userFunctionHover(const std::vector<calc::UserFunction> &funs,
                  const QString &ident);
[[nodiscard]] int indentDepthAt(const QString &text, int cursorPos);
[[nodiscard]] QString historyFilePath();

} // namespace calx
