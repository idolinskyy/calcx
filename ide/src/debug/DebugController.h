#pragma once

#include "calc/chunk.hpp"
#include "calc/debug.hpp"
#include "calc/error.hpp"

#include <QObject>
#include <QSet>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>

class QThread;

class DebugWorker final : public QObject {
  Q_OBJECT
public:
  explicit DebugWorker(QObject *parent = nullptr);

public slots:
  void load(calc::Chunk chunk, QList<int> breakpoints);
  void syncBreakpoints(QList<int> breakpoints);
  void continueRun();
  void stepInto();
  void stepOver();
  void stepOut();
  void requestPause();
  void reset();

signals:
  void paused(int line, quint64 ip, int frames, QString outputDelta,
              QStringList localNames, QStringList localValues,
              QStringList localTypes, QString disasm);
  void finished(QString outputDelta);
  void errored(QString message, QString outputDelta);
  void stateBusy(bool busy);

private:
  void handleResult(const std::expected<void, calc::Diagnostic> &r);
  QString takeOutputDelta();
  void emitPausedState();

  std::unique_ptr<calc::DebugSession> session_;
  std::string prev_output_;
};

/// Фасад UI → worker у QThread.
class DebugController final : public QObject {
  Q_OBJECT
public:
  explicit DebugController(QObject *parent = nullptr);
  ~DebugController() override;

  void start(calc::Chunk chunk, const QSet<int> &breakpoints);
  void continueRun();
  void pause();
  void stop();
  void stepInto();
  void stepOver();
  void stepOut();
  void syncBreakpoints(const QSet<int> &breakpoints);

  [[nodiscard]] bool isBusy() const { return busy_; }

signals:
  void paused(int line, quint64 ip, int frames, QString outputDelta,
              QStringList localNames, QStringList localValues,
              QStringList localTypes, QString disasm);
  void finished(QString outputDelta);
  void errored(QString message, QString outputDelta);
  void busyChanged(bool busy);

private:
  QThread *thread_{nullptr};
  DebugWorker *worker_{nullptr};
  bool busy_{false};
};
