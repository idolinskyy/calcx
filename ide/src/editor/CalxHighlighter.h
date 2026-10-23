#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class CalxHighlighter final : public QSyntaxHighlighter {
  Q_OBJECT
public:
  explicit CalxHighlighter(QTextDocument *parent = nullptr);

  void setDark(bool dark);

protected:
  void highlightBlock(const QString &text) override;

private:
  void applyFormats();

  bool dark_{false};
  QTextCharFormat keywordFmt_;
  QTextCharFormat literalFmt_;
  QTextCharFormat stringFmt_;
  QTextCharFormat builtinFmt_;
  QTextCharFormat constantFmt_;
  QTextCharFormat commentFmt_;
  QTextCharFormat operatorFmt_;
  QTextCharFormat punctFmt_;
};
