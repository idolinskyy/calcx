#pragma once

#include "calc/repl.hpp"

#include <QString>
#include <QWidget>
#include <string>
#include <vector>

class QCompleter;

class QPlainTextEdit;
class QKeyEvent;
class CalxHighlighter;

class ReplWidget final : public QWidget {
  Q_OBJECT
public:
  explicit ReplWidget(QWidget *parent = nullptr);
  ~ReplWidget() override;

  [[nodiscard]] QString sessionSource() const;
  void clearSession();
  void cursorPlace(int &line, int &column, int &selected) const;

protected:
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void sessionUpdated(const QString &source, const QString &outputDelta);
  void cursorMoved();

private slots:
  void onSubmit();

private:
  QPlainTextEdit *console_{nullptr};
  CalxHighlighter *highlighter_{nullptr};
  QCompleter *completer_{nullptr};

  std::string session_;
  std::string prev_output_;
  std::string pending_;
  QStringList history_;
  QStringList completionNames_;
  int history_index_{-1};
  /// Позиція в документі, з якої можна редагувати (після поточного промпта).
  int inputStart_{0};
  bool writing_{false};

  void appendOut(const QString &text);
  void appendErr(const QString &text);
  void loadHistory();
  void saveHistory();
  void beginInput(const QString &prompt);
  void writeAtEnd(const QString &text, const QColor &color);
  [[nodiscard]] QString currentInput() const;
  void setCurrentInput(const QString &text);
  [[nodiscard]] bool handleKey(QKeyEvent *ev);
  [[nodiscard]] int docEnd() const;
  [[nodiscard]] QString symbolSource() const;
  void refreshCompletions();
  void showCompletions();
  void insertCompletion(const QString &completion);
};
