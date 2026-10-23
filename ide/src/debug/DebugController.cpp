#include "debug/DebugController.h"

#include "calc/disasm.hpp"
#include "calc/engine.hpp"
#include "calc/value.hpp"

#include <QThread>
#include <sstream>
#include <utility>

namespace {

QList<int> toList(const QSet<int> &s) {
  QList<int> out;
  out.reserve(s.size());
  for (int v : s) {
    out.push_back(v);
  }
  return out;
}

} // namespace

DebugWorker::DebugWorker(QObject *parent) : QObject(parent) {}

void DebugWorker::load(calc::Chunk chunk, QList<int> breakpoints) {
  session_ = std::make_unique<calc::DebugSession>();
  session_->load(std::move(chunk));
  for (int line : breakpoints) {
    session_->set_breakpoint(static_cast<std::uint32_t>(line), true);
  }
  prev_output_.clear();
  Q_EMIT stateBusy(false);
  emitPausedState();
}

void DebugWorker::syncBreakpoints(QList<int> breakpoints) {
  if (!session_) {
    return;
  }
  session_->clear_breakpoints();
  for (int line : breakpoints) {
    session_->set_breakpoint(static_cast<std::uint32_t>(line), true);
  }
}

void DebugWorker::requestPause() {
  if (session_) {
    session_->request_pause();
  }
}

void DebugWorker::reset() {
  if (session_) {
    session_->request_pause();
  }
  session_.reset();
  prev_output_.clear();
  Q_EMIT stateBusy(false);
}

void DebugWorker::continueRun() {
  if (!session_) {
    return;
  }
  Q_EMIT stateBusy(true);
  handleResult(session_->continue_run());
}

void DebugWorker::stepInto() {
  if (!session_) {
    return;
  }
  Q_EMIT stateBusy(true);
  handleResult(session_->step_line());
}

void DebugWorker::stepOver() {
  if (!session_) {
    return;
  }
  Q_EMIT stateBusy(true);
  handleResult(session_->step_over());
}

void DebugWorker::stepOut() {
  if (!session_) {
    return;
  }
  Q_EMIT stateBusy(true);
  handleResult(session_->step_out());
}

QString DebugWorker::takeOutputDelta() {
  if (!session_) {
    return {};
  }
  const std::string full = session_->output();
  QString delta;
  if (full.size() >= prev_output_.size() &&
      full.compare(0, prev_output_.size(), prev_output_) == 0) {
    delta = QString::fromStdString(full.substr(prev_output_.size()));
  } else {
    delta = QString::fromStdString(full);
  }
  prev_output_ = full;
  return delta;
}

void DebugWorker::emitPausedState() {
  if (!session_) {
    return;
  }
  handleResult(std::expected<void, calc::Diagnostic>{});
}

void DebugWorker::handleResult(const std::expected<void, calc::Diagnostic> &r) {
  const QString delta = takeOutputDelta();
  Q_EMIT stateBusy(false);
  if (!session_) {
    return;
  }
  if (!r) {
    const auto &d = r.error();
    const QString msg =
        QStringLiteral("помилка:%1:%2: %3")
            .arg(d.pos.line)
            .arg(d.pos.column)
            .arg(d.message.empty()
                     ? QString::fromStdString(calc::error_message(d.code))
                     : QString::fromStdString(d.message));
    Q_EMIT errored(msg, delta);
    return;
  }
  if (session_->state() == calc::DebugState::Finished) {
    Q_EMIT finished(delta);
    return;
  }

  QStringList names;
  QStringList values;
  QStringList types;
  const auto &locs = session_->locals();
  const auto &lnames = session_->local_names();
  for (std::size_t i = 0; i < locs.size(); ++i) {
    QString name = i < lnames.size() ? QString::fromStdString(lnames[i])
                                     : QStringLiteral("[%1]").arg(i);
    if (name.startsWith(QLatin1String("__"))) {
      continue;
    }
    names << name;
    values << QString::fromStdString(calc::format_value(locs[i]));
    const auto tn = calc::value_type_name(locs[i]);
    types << QString::fromUtf8(tn.data(), static_cast<int>(tn.size()));
  }
  std::ostringstream oss;
  calc::dump_chunk(session_->chunk(), oss, locs, false);
  const QString disasm = QStringLiteral(">>> ip=%1\n\n%2")
                             .arg(session_->ip())
                             .arg(QString::fromStdString(oss.str()));
  Q_EMIT paused(static_cast<int>(session_->current_line()),
              static_cast<quint64>(session_->ip()),
              static_cast<int>(session_->frame_count()), delta, names, values,
              types, disasm);
}

DebugController::DebugController(QObject *parent) : QObject(parent) {
  thread_ = new QThread(this);
  worker_ = new DebugWorker();
  worker_->moveToThread(thread_);
  connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);

  connect(worker_, &DebugWorker::paused, this, &DebugController::paused);
  connect(worker_, &DebugWorker::finished, this, &DebugController::finished);
  connect(worker_, &DebugWorker::errored, this, &DebugController::errored);
  connect(worker_, &DebugWorker::stateBusy, this, [this](bool b) {
    busy_ = b;
    Q_EMIT busyChanged(b);
  });

  thread_->start();
}

DebugController::~DebugController() {
  if (worker_) {
    QMetaObject::invokeMethod(worker_, &DebugWorker::reset,
                              Qt::BlockingQueuedConnection);
  }
  thread_->quit();
  thread_->wait(3000);
}

void DebugController::start(calc::Chunk chunk, const QSet<int> &breakpoints) {
  auto *w = worker_;
  const auto bps = toList(breakpoints);
  QMetaObject::invokeMethod(
      w,
      [w, chunk = std::move(chunk), bps]() mutable { w->load(std::move(chunk), bps); },
      Qt::QueuedConnection);
}

void DebugController::syncBreakpoints(const QSet<int> &breakpoints) {
  auto *w = worker_;
  const auto bps = toList(breakpoints);
  QMetaObject::invokeMethod(
      w, [w, bps]() { w->syncBreakpoints(bps); }, Qt::QueuedConnection);
}

void DebugController::continueRun() {
  QMetaObject::invokeMethod(worker_, &DebugWorker::continueRun,
                            Qt::QueuedConnection);
}

void DebugController::pause() {
  QMetaObject::invokeMethod(worker_, &DebugWorker::requestPause,
                            Qt::QueuedConnection);
}

void DebugController::stop() {
  QMetaObject::invokeMethod(worker_, &DebugWorker::reset, Qt::QueuedConnection);
}

void DebugController::stepInto() {
  QMetaObject::invokeMethod(worker_, &DebugWorker::stepInto,
                            Qt::QueuedConnection);
}

void DebugController::stepOver() {
  QMetaObject::invokeMethod(worker_, &DebugWorker::stepOver,
                            Qt::QueuedConnection);
}

void DebugController::stepOut() {
  QMetaObject::invokeMethod(worker_, &DebugWorker::stepOut,
                            Qt::QueuedConnection);
}
