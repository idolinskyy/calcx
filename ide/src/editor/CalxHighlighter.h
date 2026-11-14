#pragma once

#include <QSyntaxHighlighter>
#include <QStringList>
#include <QTextCharFormat>

class CalxHighlighter final : public QSyntaxHighlighter {
  Q_OBJECT
public:
  explicit CalxHighlighter(QTextDocument *parent = nullptr);

  void setDark(bool dark);
  /// Рядки REPL: префікс (`calx> `, `...> `) не лекситься як код.
  void setSkipPrefixes(QStringList prefixes);

protected:
  void highlightBlock(const QString &text) override;

private:
  void applyFormats();
  void highlightSource(const QString &text, int offset);

  bool dark_{false};
  QStringList skipPrefixes_;
  QTextCharFormat keywordFmt_;
  QTextCharFormat literalFmt_;
  QTextCharFormat stringFmt_;
  QTextCharFormat builtinFmt_;
  QTextCharFormat constantFmt_;
  QTextCharFormat commentFmt_;
  QTextCharFormat operatorFmt_;
  QTextCharFormat punctFmt_;
};
