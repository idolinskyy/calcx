#pragma once

#include <QDockWidget>
#include <QPlainTextEdit>

class ConsoleDock final : public QDockWidget {
  Q_OBJECT
public:
  explicit ConsoleDock(QWidget *parent = nullptr);

  void appendText(const QString &text);
  void appendError(const QString &text);
  void clear();

private:
  QPlainTextEdit *view_{nullptr};
};
