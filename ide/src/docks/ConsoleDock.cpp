#include "docks/ConsoleDock.h"

#include <QFont>

ConsoleDock::ConsoleDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Консоль"), parent) {
  view_ = new QPlainTextEdit(this);
  view_->setReadOnly(true);
  QFont font(QStringLiteral("monospace"));
  font.setStyleHint(QFont::Monospace);
  font.setPointSize(10);
  view_->setFont(font);
  setWidget(view_);
  setObjectName(QStringLiteral("ConsoleDock"));
}

void ConsoleDock::appendText(const QString &text) {
  view_->moveCursor(QTextCursor::End);
  view_->insertPlainText(text);
  view_->moveCursor(QTextCursor::End);
}

void ConsoleDock::appendError(const QString &text) {
  view_->moveCursor(QTextCursor::End);
  view_->appendHtml(
      QStringLiteral("<span style=\"color:#cf222e\">%1</span>")
          .arg(text.toHtmlEscaped()));
}

void ConsoleDock::clear() { view_->clear(); }
