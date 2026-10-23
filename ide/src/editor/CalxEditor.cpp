#include "editor/CalxEditor.h"
#include "editor/CalxHighlighter.h"
#include "util/CalxAssist.h"
#include "util/CalxFormatter.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QFont>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QToolTip>

class LineNumberArea final : public QWidget {
public:
  explicit LineNumberArea(CalxEditor *editor)
      : QWidget(editor), editor_(editor) {}
  [[nodiscard]] QSize sizeHint() const override {
    return {editor_->lineNumberAreaWidth(), 0};
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    editor_->lineNumberAreaPaintEvent(event);
  }
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) {
      return;
    }
    QTextBlock block = editor_->firstVisibleBlock();
    int top = qRound(editor_->blockBoundingGeometry(block)
                         .translated(editor_->contentOffset())
                         .top());
    const int y = event->position().toPoint().y();
    while (block.isValid()) {
      const int bottom =
          top + qRound(editor_->blockBoundingRect(block).height());
      if (y >= top && y < bottom) {
        editor_->toggleBreakpoint(block.blockNumber() + 1);
        return;
      }
      block = block.next();
      top = bottom;
    }
  }

private:
  CalxEditor *editor_;
};

CalxEditor::CalxEditor(QWidget *parent) : QPlainTextEdit(parent) {
  QFont font(QStringLiteral("monospace"));
  font.setStyleHint(QFont::Monospace);
  font.setFixedPitch(true);
  font.setPointSize(11);
  setFont(font);

  lineNumberArea_ = new LineNumberArea(this);
  highlighter_ = new CalxHighlighter(document());

  completer_ = new QCompleter(calx::completionBase(), this);
  completer_->setWidget(this);
  completer_->setCompletionMode(QCompleter::PopupCompletion);
  completer_->setCaseSensitivity(Qt::CaseInsensitive);
  completer_->setFilterMode(Qt::MatchStartsWith);
  connect(completer_, QOverload<const QString &>::of(&QCompleter::activated),
          this, &CalxEditor::insertCompletion);

  connect(this, &CalxEditor::blockCountChanged, this,
          &CalxEditor::updateLineNumberAreaWidth);
  connect(this, &CalxEditor::updateRequest, this,
          &CalxEditor::updateLineNumberArea);
  connect(this, &CalxEditor::cursorPositionChanged, this,
          &CalxEditor::highlightCurrentLine);

  updateLineNumberAreaWidth(0);
  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 2);
  syncThemeFromPalette();
}

bool CalxEditor::isDark() const {
  return palette().color(QPalette::Base).lightness() < 128;
}

void CalxEditor::syncThemeFromPalette() {
  dark_ = isDark();
  if (highlighter_) {
    highlighter_->setDark(dark_);
  }
  updateExtraSelections();
  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
}

void CalxEditor::changeEvent(QEvent *event) {
  QPlainTextEdit::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange) {
    syncThemeFromPalette();
  }
}

int CalxEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }
  return 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 14;
}

void CalxEditor::updateLineNumberAreaWidth(int) {
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CalxEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (dy) {
    lineNumberArea_->scroll(0, dy);
  } else {
    lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(),
                            rect.height());
  }
  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

void CalxEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  const QRect cr = contentsRect();
  lineNumberArea_->setGeometry(
      QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CalxEditor::highlightCurrentLine() { updateExtraSelections(); }

void CalxEditor::updateExtraSelections() {
  QList<QTextEdit::ExtraSelection> extras;
  const bool dark = dark_;

  if (!isReadOnly()) {
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(dark ? QColor(QStringLiteral("#1f2a36"))
                                  : QColor(QStringLiteral("#e8f2ff")));
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    sel.cursor = textCursor();
    sel.cursor.clearSelection();
    extras.append(sel);
  }

  if (debugLine_ > 0) {
    QTextBlock block = document()->findBlockByNumber(debugLine_ - 1);
    if (block.isValid()) {
      QTextEdit::ExtraSelection sel;
      sel.format.setBackground(dark ? QColor(QStringLiteral("#3d2e00"))
                                    : QColor(QStringLiteral("#fff3bf")));
      sel.format.setProperty(QTextFormat::FullWidthSelection, true);
      sel.cursor = QTextCursor(block);
      sel.cursor.clearSelection();
      extras.append(sel);
    }
  }

  if (diagLine_ > 0) {
    QTextBlock block = document()->findBlockByNumber(diagLine_ - 1);
    if (block.isValid()) {
      QTextEdit::ExtraSelection sel;
      sel.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
      sel.format.setUnderlineColor(Qt::red);
      sel.cursor = QTextCursor(block);
      if (diagCol_ > 0) {
        sel.cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                diagCol_ - 1);
        sel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                                qMax(1, diagLen_));
      } else {
        sel.cursor.select(QTextCursor::LineUnderCursor);
      }
      extras.append(sel);
    }
  }

  setExtraSelections(extras);
}

void CalxEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
  QPainter painter(lineNumberArea_);
  painter.fillRect(event->rect(), dark_ ? QColor(QStringLiteral("#21262d"))
                                        : QColor(QStringLiteral("#f6f8fa")));

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top =
      qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const int line = blockNumber + 1;
      constexpr int gutterW = 14;
      if (breakpoints_.contains(line)) {
        painter.setBrush(QColor(QStringLiteral("#f85149")));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(2, top + 4, 10, 10);
      }
      painter.setPen(dark_ ? QColor(QStringLiteral("#8b949e"))
                           : QColor(QStringLiteral("#6e7781")));
      painter.drawText(gutterW, top, lineNumberArea_->width() - gutterW - 4,
                       fontMetrics().height(), Qt::AlignRight,
                       QString::number(line));
    }
    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

void CalxEditor::setBreakpoint(int line1based, bool on) {
  if (on) {
    breakpoints_.insert(line1based);
  } else {
    breakpoints_.remove(line1based);
  }
  lineNumberArea_->update();
  Q_EMIT breakpointsChanged();
}

void CalxEditor::toggleBreakpoint(int line1based) {
  setBreakpoint(line1based, !breakpoints_.contains(line1based));
}

void CalxEditor::clearBreakpoints() {
  breakpoints_.clear();
  lineNumberArea_->update();
  Q_EMIT breakpointsChanged();
}

void CalxEditor::setCurrentDebugLine(int line1based) {
  debugLine_ = line1based;
  updateExtraSelections();
  if (line1based > 0) {
    QTextBlock block = document()->findBlockByNumber(line1based - 1);
    if (block.isValid()) {
      setTextCursor(QTextCursor(block));
      centerCursor();
    }
  }
}

void CalxEditor::setDiagnosticUnderline(int line1based, int column1based,
                                        int length) {
  diagLine_ = line1based;
  diagCol_ = column1based;
  diagLen_ = length;
  updateExtraSelections();
}

void CalxEditor::clearDiagnostics() {
  diagLine_ = 0;
  updateExtraSelections();
}

void CalxEditor::setExtraCompletions(const QStringList &names) {
  QStringList all = calx::completionBase();
  all += names;
  all.removeDuplicates();
  all.sort(Qt::CaseInsensitive);
  completer_->setModel(new QStringListModel(all, completer_));
}

bool CalxEditor::formatDocument(QString *error_out) {
  const auto result = calx::formatSource(toPlainText());
  if (!result.ok) {
    if (error_out) {
      *error_out = result.error;
    }
    return false;
  }
  const int pos = textCursor().position();
  setPlainText(result.text);
  QTextCursor c = textCursor();
  c.setPosition(qMin(pos, static_cast<int>(toPlainText().size())));
  setTextCursor(c);
  return true;
}

bool CalxEditor::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    auto *help = static_cast<QHelpEvent *>(event);
    QTextCursor cur =
        cursorForPosition(viewport()->mapFromGlobal(help->globalPos()));
    cur.select(QTextCursor::WordUnderCursor);
    const QString word = cur.selectedText();
    const QString tip = calx::hoverTextFor(word);
    if (!tip.isEmpty()) {
      QToolTip::showText(help->globalPos(), tip, this);
    } else {
      QToolTip::hideText();
    }
    return true;
  }
  return QPlainTextEdit::event(event);
}

void CalxEditor::insertCompletion(const QString &completion) {
  if (completer_->widget() != this) {
    return;
  }
  QTextCursor tc = textCursor();
  const int extra =
      completion.length() - completer_->completionPrefix().length();
  tc.movePosition(QTextCursor::Left);
  tc.movePosition(QTextCursor::EndOfWord);
  tc.insertText(completion.right(extra));
  setTextCursor(tc);
}

void CalxEditor::showCompletions() {
  QTextCursor tc = textCursor();
  tc.select(QTextCursor::WordUnderCursor);
  const QString prefix = tc.selectedText();
  if (prefix.size() < 1) {
    completer_->popup()->hide();
    return;
  }
  completer_->setCompletionPrefix(prefix);
  if (completer_->completionCount() == 0) {
    completer_->popup()->hide();
    return;
  }
  QRect cr = cursorRect();
  cr.setWidth(completer_->popup()->sizeHintForColumn(0) +
              completer_->popup()->verticalScrollBar()->sizeHint().width());
  completer_->complete(cr);
}

void CalxEditor::keyPressEvent(QKeyEvent *event) {
  if (completer_ && completer_->popup()->isVisible()) {
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Escape:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
      event->ignore();
      return;
    default:
      break;
    }
  }

  if (event->key() == Qt::Key_Space &&
      (event->modifiers() & Qt::ControlModifier)) {
    showCompletions();
    return;
  }

  if (event->key() == Qt::Key_Tab &&
      !(event->modifiers() & Qt::ShiftModifier)) {
    insertPlainText(QStringLiteral("  "));
    return;
  }

  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    const int depth =
        calx::indentDepthAt(toPlainText(), textCursor().position());
    QPlainTextEdit::keyPressEvent(event);
    insertPlainText(QString(depth * 2, QLatin1Char(' ')));
    return;
  }

  QPlainTextEdit::keyPressEvent(event);

  const bool ctrl = event->modifiers() & Qt::ControlModifier;
  if (!ctrl && !event->text().isEmpty() && event->text()[0].isLetter()) {
    showCompletions();
  }
}
