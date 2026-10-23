#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

class CalxEditor;
class ReplWidget;
class ConsoleDock;
class VarsDock;
class FunsDock;
class ConstsDock;
class DisasmDock;
class DebugController;
class QStackedWidget;
class QLabel;
class QCloseEvent;

class MainWindow final : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  void openPath(const QString &path);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void newFile();
  void openFile();
  bool saveFile();
  bool saveFileAs();
  void runProgram();
  void debugStart();
  void debugContinue();
  void debugPause();
  void debugStop();
  void debugStepInto();
  void debugStepOver();
  void debugStepOut();
  void formatSource();
  void toggleFormatOnSave(bool enabled);
  void switchEditorMode();
  void switchReplMode();
  void toggleTheme();
  void openHelp();
  void openAbout();
  void openAboutQt();
  void onDebugFinished(const QString &outputDelta);
  void onDebugErrored(const QString &message, const QString &outputDelta);
  void onDebugPaused(int line, quint64 ip, int frames,
                     const QString &outputDelta, const QStringList &localNames,
                     const QStringList &localValues,
                     const QStringList &localTypes, const QString &disasm);
  void onReplSessionUpdated(const QString &source, const QString &delta);
  void onBreakpointsChanged();
  void updateWindowTitle();

private:
  void createMenus();
  void createToolbars();
  void createDocks();
  void createShortcuts();
  [[nodiscard]] bool maybeSave();
  void setCurrentFile(const QString &path);
  void applyTheme(bool dark);
  [[nodiscard]] bool formatBeforeSave();

  QStackedWidget *stack_{nullptr};
  CalxEditor *editor_{nullptr};
  ReplWidget *repl_{nullptr};

  ConsoleDock *console_{nullptr};
  VarsDock *vars_{nullptr};
  FunsDock *funs_{nullptr};
  ConstsDock *consts_{nullptr};
  DisasmDock *disasm_{nullptr};

  DebugController *debugger_{nullptr};
  QLabel *statusMode_{nullptr};

  QString currentFile_;
  bool dirty_{false};
  bool debugging_{false};
  bool darkTheme_{false};
  bool formatOnSave_{false};
};
