#include "repl/ReplWidget.h"

#include "editor/CalxHighlighter.h"
#include "util/CalxAssist.h"

#include "calc/engine.hpp"
#include "calc/repl.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <fstream>
#include <functional>
#include <sstream>

namespace {

class ReplKeyFilter final : public QObject {
public:
  using Handler = std::function<bool(QKeyEvent *)>;
  explicit ReplKeyFilter(Handler h, QObject *parent = nullptr)
      : QObject(parent), handler_(std::move(h)) {}

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::KeyPress) {
      if (handler_(static_cast<QKeyEvent *>(event))) {
        return true;
      }
    }
    return QObject::eventFilter(obj, event);
  }

private:
  Handler handler_;
};

} // namespace

ReplWidget::ReplWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  console_ = new QPlainTextEdit(this);
  QFont font(QStringLiteral("monospace"));
  font.setStyleHint(QFont::Monospace);
  font.setPointSize(10);
  console_->setFont(font);
  console_->setUndoRedoEnabled(true);
  console_->setTabChangesFocus(false);
  highlighter_ = new CalxHighlighter(console_->document());
  highlighter_->setSkipPrefixes(
      {QStringLiteral("calx> "), QStringLiteral("...> ")});
  highlighter_->setDark(
      console_->palette().color(QPalette::Base).lightness() < 128);
  layout->addWidget(console_, 1);

  connect(console_->document(), &QTextDocument::contentsChange, this,
          [this](int from, int, int) {
            if (writing_ || from >= inputStart_) {
              return;
            }
            QTimer::singleShot(0, this, [this] {
              if (writing_) {
                return;
              }
              writing_ = true;
              console_->undo();
              writing_ = false;
              QTextCursor c = console_->textCursor();
              if (c.position() < inputStart_) {
                c.setPosition(docEnd());
                console_->setTextCursor(c);
              }
            });
          });

  console_->installEventFilter(new ReplKeyFilter(
      [this](QKeyEvent *ev) { return handleKey(ev); }, console_));

  appendOut(QStringLiteral(
      "Інтерактивний calx. :help — довідка, :clear — скинути сесію.\n"));
  beginInput(QStringLiteral("calx>"));
  loadHistory();
}

ReplWidget::~ReplWidget() { saveHistory(); }

void ReplWidget::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange) {
    if (highlighter_ && console_) {
      highlighter_->setDark(
          console_->palette().color(QPalette::Base).lightness() < 128);
    }
  }
}

QString ReplWidget::sessionSource() const {
  return QString::fromStdString(session_);
}

void ReplWidget::clearSession() {
  session_.clear();
  prev_output_.clear();
  pending_.clear();
  appendOut(QStringLiteral("(сесію очищено)\n"));
  beginInput(QStringLiteral("calx>"));
  Q_EMIT sessionUpdated({}, {});
}

int ReplWidget::docEnd() const {
  return qMax(0, console_->document()->characterCount() - 1);
}

void ReplWidget::writeAtEnd(const QString &text, const QColor &color) {
  writing_ = true;
  QTextCursor c(console_->document());
  c.movePosition(QTextCursor::End);
  QTextCharFormat fmt;
  fmt.setForeground(color.isValid() ? color
                                    : console_->palette().color(QPalette::Text));
  c.setCharFormat(fmt);
  c.insertText(text);
  writing_ = false;
  console_->setTextCursor(c);
  console_->ensureCursorVisible();
}

void ReplWidget::beginInput(const QString &prompt) {
  const QString existing = console_->toPlainText();
  if (!existing.isEmpty() && !existing.endsWith(QLatin1Char('\n'))) {
    writeAtEnd(QStringLiteral("\n"), {});
  }
  const bool dark =
      console_->palette().color(QPalette::Base).lightness() < 128;
  const QColor promptColor = dark ? QColor(0x7a, 0xa2, 0xf7)
                                  : QColor(0x05, 0x50, 0xae);
  writeAtEnd(prompt + QStringLiteral(" "), promptColor);
  inputStart_ = docEnd();
  QTextCursor c = console_->textCursor();
  c.movePosition(QTextCursor::End);
  QTextCharFormat normal;
  normal.setForeground(console_->palette().color(QPalette::Text));
  c.setCharFormat(normal);
  console_->setTextCursor(c);
  console_->ensureCursorVisible();
}

QString ReplWidget::currentInput() const {
  return console_->toPlainText().mid(inputStart_);
}

void ReplWidget::setCurrentInput(const QString &text) {
  writing_ = true;
  QTextCursor c(console_->document());
  c.setPosition(inputStart_);
  c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  c.removeSelectedText();
  QTextCharFormat normal;
  normal.setForeground(console_->palette().color(QPalette::Text));
  c.setCharFormat(normal);
  c.insertText(text);
  writing_ = false;
  console_->setTextCursor(c);
  console_->ensureCursorVisible();
}

void ReplWidget::appendOut(const QString &text) {
  const QString existing = console_->toPlainText();
  if (!existing.isEmpty() && !existing.endsWith(QLatin1Char('\n')) &&
      !text.startsWith(QLatin1Char('\n'))) {
    writeAtEnd(QStringLiteral("\n"), {});
  }
  writeAtEnd(text, {});
}

void ReplWidget::appendErr(const QString &text) {
  const QString existing = console_->toPlainText();
  if (!existing.isEmpty() && !existing.endsWith(QLatin1Char('\n'))) {
    writeAtEnd(QStringLiteral("\n"), {});
  }
  const QString line =
      text.endsWith(QLatin1Char('\n')) ? text : text + QLatin1Char('\n');
  writeAtEnd(line, QColor(0xcf, 0x22, 0x2e));
}

void ReplWidget::loadHistory() {
  std::ifstream in(calx::historyFilePath().toStdString());
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      history_.append(QString::fromStdString(line));
    }
  }
}

void ReplWidget::saveHistory() {
  std::ofstream out(calx::historyFilePath().toStdString(), std::ios::trunc);
  const int start = qMax(0, history_.size() - 1000);
  for (int i = start; i < history_.size(); ++i) {
    out << history_[i].toStdString() << '\n';
  }
}

bool ReplWidget::handleKey(QKeyEvent *ev) {
  QTextCursor cursor = console_->textCursor();
  const int pos = cursor.position();
  const int selStart = qMin(pos, cursor.anchor());
  const int selEnd = qMax(pos, cursor.anchor());

  const auto block = cursor.block();
  const bool onFirstInputLine =
      block.position() <= inputStart_ &&
      inputStart_ < block.position() + block.length();
  const bool onLastLine = block == console_->document()->lastBlock();

  const bool copy = ev->matches(QKeySequence::Copy);
  const bool paste = ev->matches(QKeySequence::Paste);
  const bool cut = ev->matches(QKeySequence::Cut);
  const bool selectAll = ev->matches(QKeySequence::SelectAll);

  if (selectAll) {
    QTextCursor c(console_->document());
    c.setPosition(inputStart_);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    console_->setTextCursor(c);
    return true;
  }
  if (copy) {
    return false;
  }

  if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
    if (ev->modifiers() & Qt::ShiftModifier) {
      if (selStart < inputStart_) {
        QTextCursor c = cursor;
        c.setPosition(docEnd());
        console_->setTextCursor(c);
      }
      console_->insertPlainText(QStringLiteral("\n"));
      return true;
    }
    onSubmit();
    return true;
  }

  if (ev->key() == Qt::Key_Up &&
      (ev->modifiers() == Qt::NoModifier ||
       ev->modifiers() == Qt::KeypadModifier) &&
      onFirstInputLine) {
    if (history_.isEmpty()) {
      return true;
    }
    if (history_index_ < 0) {
      history_index_ = history_.size() - 1;
    } else if (history_index_ > 0) {
      --history_index_;
    }
    setCurrentInput(history_[history_index_]);
    return true;
  }
  if (ev->key() == Qt::Key_Down &&
      (ev->modifiers() == Qt::NoModifier ||
       ev->modifiers() == Qt::KeypadModifier) &&
      onLastLine) {
    if (history_index_ < 0) {
      return true;
    }
    if (history_index_ + 1 >= history_.size()) {
      history_index_ = -1;
      setCurrentInput({});
    } else {
      ++history_index_;
      setCurrentInput(history_[history_index_]);
    }
    return true;
  }

  if (ev->key() == Qt::Key_Home && onFirstInputLine &&
      (ev->modifiers() == Qt::NoModifier ||
       ev->modifiers() == Qt::ShiftModifier)) {
    QTextCursor c = cursor;
    c.setPosition(inputStart_, ev->modifiers() == Qt::ShiftModifier
                                   ? QTextCursor::KeepAnchor
                                   : QTextCursor::MoveAnchor);
    console_->setTextCursor(c);
    return true;
  }

  if ((ev->key() == Qt::Key_Backspace || ev->key() == Qt::Key_Left) &&
      selStart == selEnd && pos <= inputStart_) {
    return true;
  }
  if (ev->key() == Qt::Key_Delete && selStart == selEnd && pos < inputStart_) {
    return true;
  }

  const bool edits = !ev->text().isEmpty() || cut || paste ||
                     ev->key() == Qt::Key_Backspace ||
                     ev->key() == Qt::Key_Delete || ev->key() == Qt::Key_Tab;
  if (edits && selStart < inputStart_) {
    QTextCursor c = cursor;
    c.setPosition(docEnd());
    console_->setTextCursor(c);
    return false;
  }
  return false;
}

void ReplWidget::onSubmit() {
  const QString raw = currentInput().trimmed();
  history_index_ = -1;
  if (raw.isEmpty()) {
    return;
  }

  inputStart_ = docEnd();

  const std::string cmd = raw.toStdString();
  if (cmd == ":q" || cmd == ":quit" || cmd == ":exit" || cmd == "quit" ||
      cmd == "exit") {
    appendOut(QStringLiteral(
        "(у GUI використовуйте меню Файл → Вийти)\n"));
    beginInput(QStringLiteral("calx>"));
    return;
  }
  if (cmd == ":h" || cmd == ":help" || cmd == "help") {
    appendOut(QStringLiteral(
        "Рядки накопичуються в сесію. Незакриті { ( [ — "
        "продовження (...>).\n"
        "Shift+Enter — новий рядок. Enter — виконати.\n"
        "Команди: :help  :clear\n"));
    beginInput(QStringLiteral("calx>"));
    return;
  }
  if (cmd == ":clear" || cmd == ":reset") {
    clearSession();
    return;
  }

  history_.append(raw);
  if (pending_.empty()) {
    pending_ = cmd;
  } else {
    pending_.push_back('\n');
    pending_ += cmd;
  }
  if (!calc::repl_source_complete(pending_)) {
    beginInput(QStringLiteral("...>"));
    return;
  }

  const std::string chunk = calc::repl_ensure_stmt(pending_);
  pending_.clear();
  if (chunk.empty()) {
    beginInput(QStringLiteral("calx>"));
    return;
  }

  const std::string trial = session_ + chunk;
  std::ostringstream captured;
  auto result = calc::run(trial, captured);
  if (!result) {
    const auto &d = result.error();
    appendErr(QStringLiteral("помилка:%1:%2: %3")
                  .arg(d.pos.line)
                  .arg(d.pos.column)
                  .arg(d.message.empty()
                           ? QString::fromStdString(calc::error_message(d.code))
                           : QString::fromStdString(d.message)));
    beginInput(QStringLiteral("calx>"));
    return;
  }

  session_ = trial;
  const std::string full = captured.str();
  QString delta;
  if (full.size() >= prev_output_.size() &&
      full.compare(0, prev_output_.size(), prev_output_) == 0) {
    delta = QString::fromStdString(full.substr(prev_output_.size()));
  } else {
    delta = QString::fromStdString(full);
  }
  prev_output_ = full;
  if (!delta.isEmpty()) {
    appendOut(delta);
  }
  beginInput(QStringLiteral("calx>"));
  Q_EMIT sessionUpdated(QString::fromStdString(session_), delta);
}
