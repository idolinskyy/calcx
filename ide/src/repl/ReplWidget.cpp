#include "repl/ReplWidget.h"
#include "editor/CalxEditor.h"
#include "util/CalxAssist.h"

#include "calc/engine.hpp"
#include "calc/repl.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <fstream>
#include <functional>
#include <sstream>

namespace {

class ReplInputFilter final : public QObject {
public:
  using Handler = std::function<bool(QKeyEvent *)>;
  explicit ReplInputFilter(Handler h, QObject *parent = nullptr)
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

  output_ = new QPlainTextEdit(this);
  output_->setReadOnly(true);
  QFont font(QStringLiteral("monospace"));
  font.setStyleHint(QFont::Monospace);
  font.setPointSize(10);
  output_->setFont(font);
  layout->addWidget(output_, 3);

  auto *row = new QHBoxLayout();
  prompt_ = new QLabel(QStringLiteral("calx>"), this);
  prompt_->setFont(font);
  input_ = new CalxEditor(this);
  input_->setMaximumBlockCount(0);
  input_->document()->setMaximumBlockCount(0);
  // Однорядковий за замовчуванням — висота ~3 рядки для continuation.
  input_->setFixedHeight(fontMetrics().height() * 4 + 8);
  row->addWidget(prompt_, 0);
  row->addWidget(input_, 1);
  layout->addLayout(row);

  appendOut(QStringLiteral("Інтерактивний calx. :help — довідка, :clear — "
                           "скинути сесію.\n"));

  input_->installEventFilter(new ReplInputFilter(
      [this](QKeyEvent *ev) {
        if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
          if (ev->modifiers() & Qt::ShiftModifier) {
            return false; // новий рядок
          }
          onSubmit();
          return true;
        }
        if (ev->key() == Qt::Key_Up &&
            (ev->modifiers() == Qt::NoModifier ||
             ev->modifiers() == Qt::KeypadModifier)) {
          if (history_.isEmpty()) {
            return true;
          }
          if (history_index_ < 0) {
            history_index_ = history_.size() - 1;
          } else if (history_index_ > 0) {
            --history_index_;
          }
          input_->setPlainText(history_[history_index_]);
          return true;
        }
        if (ev->key() == Qt::Key_Down &&
            (ev->modifiers() == Qt::NoModifier ||
             ev->modifiers() == Qt::KeypadModifier)) {
          if (history_index_ < 0) {
            return true;
          }
          if (history_index_ + 1 >= history_.size()) {
            history_index_ = -1;
            input_->clear();
          } else {
            ++history_index_;
            input_->setPlainText(history_[history_index_]);
          }
          return true;
        }
        return false;
      },
      input_));

  loadHistory();
}

ReplWidget::~ReplWidget() { saveHistory(); }

QString ReplWidget::sessionSource() const {
  return QString::fromStdString(session_);
}

void ReplWidget::clearSession() {
  session_.clear();
  prev_output_.clear();
  pending_.clear();
  setPrompt(false);
  appendOut(QStringLiteral("(сесію очищено)\n"));
  Q_EMIT sessionUpdated({}, {});
}

void ReplWidget::setPrompt(bool continuation) {
  prompt_->setText(continuation ? QStringLiteral("...>")
                                : QStringLiteral("calx>"));
}

void ReplWidget::appendOut(const QString &text) {
  output_->moveCursor(QTextCursor::End);
  output_->insertPlainText(text);
  output_->moveCursor(QTextCursor::End);
}

void ReplWidget::appendErr(const QString &text) {
  output_->moveCursor(QTextCursor::End);
  output_->appendHtml(
      QStringLiteral("<span style=\"color:#cf222e\">%1</span><br/>")
          .arg(text.toHtmlEscaped()));
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

void ReplWidget::onSubmit() {
  const QString raw = input_->toPlainText().trimmed();
  input_->clear();
  history_index_ = -1;
  if (raw.isEmpty()) {
    return;
  }

  appendOut(prompt_->text() + QLatin1Char(' ') + raw + QLatin1Char('\n'));

  const std::string cmd = raw.toStdString();
  if (cmd == ":q" || cmd == ":quit" || cmd == ":exit" || cmd == "quit" ||
      cmd == "exit") {
    appendOut(QStringLiteral("(у GUI використовуйте меню Файл → Вийти)\n"));
    return;
  }
  if (cmd == ":h" || cmd == ":help" || cmd == "help") {
    appendOut(
        QStringLiteral("Рядки накопичуються в сесію. Незакриті { ( [ — "
                       "продовження (...>).\n"
                       "Shift+Enter — новий рядок у вводі. Enter — виконати.\n"
                       "Команди: :help  :clear\n"));
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
    setPrompt(true);
    return;
  }

  const std::string chunk = calc::repl_ensure_stmt(pending_);
  pending_.clear();
  setPrompt(false);
  if (chunk.empty()) {
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
  Q_EMIT sessionUpdated(QString::fromStdString(session_), delta);
}
