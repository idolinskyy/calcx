#pragma once

#include <QWidget>
#include <QString>
#include <string>

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

protected:
  void changeEvent(QEvent *event) override;

signals:
  void sessionUpdated(const QString &source, const QString &outputDelta);

private slots:
  void onSubmit();

private:
  QPlainTextEdit *console_{nullptr};
  CalxHighlighter *highlighter_{nullptr};

  std::string session_;
  std::string prev_output_;
  std::string pending_;
  QStringList history_;
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
};
