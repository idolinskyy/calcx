#include "docks/DisasmDock.h"

#include "calc/disasm.hpp"

#include <QFont>
#include <sstream>

DisasmDock::DisasmDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Дізасемблер"), parent) {
  view_ = new QPlainTextEdit(this);
  view_->setReadOnly(true);
  QFont font(QStringLiteral("monospace"));
  font.setStyleHint(QFont::Monospace);
  font.setPointSize(10);
  view_->setFont(font);
  setWidget(view_);
  setObjectName(QStringLiteral("DisasmDock"));
}

void DisasmDock::setChunk(const calc::Chunk &chunk,
                          std::span<const calc::Value> locals, int highlightIp) {
  std::ostringstream oss;
  calc::dump_chunk(chunk, oss, locals, false);
  QString text = QString::fromStdString(oss.str());
  if (highlightIp >= 0) {
    text.prepend(QStringLiteral(">>> ip=%1\n\n").arg(highlightIp));
  }
  view_->setPlainText(text);
}

void DisasmDock::setText(const QString &text) { view_->setPlainText(text); }

void DisasmDock::clear() { view_->clear(); }
