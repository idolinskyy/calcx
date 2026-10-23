#pragma once

#include <QPlainTextEdit>
#include <QSet>
#include <QString>

class CalxHighlighter;
class QCompleter;
class LineNumberArea;

class CalxEditor final : public QPlainTextEdit {
  Q_OBJECT
public:
  explicit CalxEditor(QWidget *parent = nullptr);

  void lineNumberAreaPaintEvent(QPaintEvent *event);
  [[nodiscard]] int lineNumberAreaWidth() const;

  void setBreakpoint(int line1based, bool on);
  void toggleBreakpoint(int line1based);
  [[nodiscard]] QSet<int> breakpoints() const { return breakpoints_; }
  void clearBreakpoints();

  void setCurrentDebugLine(int line1based); ///< 0 = none
  void setDiagnosticUnderline(int line1based, int column1based, int length);
  void clearDiagnostics();

  void setExtraCompletions(const QStringList &names);
  /// Повне форматування; false — помилка розбору (текст не змінено).
  [[nodiscard]] bool formatDocument(QString *error_out = nullptr);
  void syncThemeFromPalette();

signals:
  void breakpointsChanged();
  void runRequested();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void changeEvent(QEvent *event) override;
  bool event(QEvent *event) override;

private slots:
  void updateLineNumberAreaWidth(int newBlockCount);
  void highlightCurrentLine();
  void updateLineNumberArea(const QRect &rect, int dy);
  void insertCompletion(const QString &completion);

private:
  friend class LineNumberArea;
  LineNumberArea *lineNumberArea_{nullptr};
  CalxHighlighter *highlighter_{nullptr};
  QCompleter *completer_{nullptr};
  QSet<int> breakpoints_;
  int debugLine_{0};
  int diagLine_{0};
  int diagCol_{0};
  int diagLen_{0};

  void updateExtraSelections();
  void showCompletions();
  [[nodiscard]] bool isDark() const;

  bool dark_{false};
};
