#pragma once

#include <QString>
#include <QStringList>

namespace calx {

[[nodiscard]] QStringList keywords();
[[nodiscard]] QStringList builtinNames();
[[nodiscard]] QStringList constantNames();
[[nodiscard]] QStringList completionBase();

[[nodiscard]] QString hoverTextFor(const QString &ident);
[[nodiscard]] int indentDepthAt(const QString &text, int cursorPos);
[[nodiscard]] QString historyFilePath();

} // namespace calx
