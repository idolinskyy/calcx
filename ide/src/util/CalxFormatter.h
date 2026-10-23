#pragma once

#include <QString>

namespace calx {

struct FormatResult {
  bool ok{false};
  QString text;
  QString error;
};

/// Повне форматування через parse → pretty-print AST.
/// Зберігає `//` коментарі (привʼязка за номером рядка).
/// При помилці розбору — ok=false, text без змін (порожній), error з діагностикою.
[[nodiscard]] FormatResult formatSource(const QString &source);

} // namespace calx
