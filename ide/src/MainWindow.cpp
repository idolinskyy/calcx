#include "MainWindow.h"

#include "debug/DebugController.h"
#include "docks/ConsoleDock.h"
#include "docks/ConstsDock.h"
#include "docks/DisasmDock.h"
#include "docks/FunsDock.h"
#include "docks/VarsDock.h"
#include "editor/CalxEditor.h"
#include "help/HelpDialog.h"
#include "repl/ReplWidget.h"

#include "calc/disasm.hpp"
#include "calc/engine.hpp"
#include "calc/lexer.hpp"
#include "calc/vm.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QToolBar>
#include <QVBoxLayout>
#include <sstream>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("calx IDE"));
  resize(1200, 800);

  QSettings settings;
  formatOnSave_ =
      settings.value(QStringLiteral("editor/formatOnSave"), false).toBool();
  darkTheme_ = settings.value(QStringLiteral("ui/darkTheme"), false).toBool();

  stack_ = new QStackedWidget(this);
  editor_ = new CalxEditor(stack_);
  repl_ = new ReplWidget(stack_);
  stack_->addWidget(editor_);
  stack_->addWidget(repl_);
  setCentralWidget(stack_);

  debugger_ = new DebugController(this);

  createDocks();
  createMenus();
  createToolbars();
  createShortcuts();

  statusMode_ = new QLabel(QStringLiteral("Редактор"), this);
  statusBar()->addPermanentWidget(statusMode_);
  statusBar()->showMessage(QStringLiteral("Готово"));

  connect(editor_, &CalxEditor::textChanged, this, [this] {
    dirty_ = true;
    updateWindowTitle();
  });
  connect(editor_, &CalxEditor::breakpointsChanged, this,
          &MainWindow::onBreakpointsChanged);
  connect(repl_, &ReplWidget::sessionUpdated, this,
          &MainWindow::onReplSessionUpdated);

  connect(debugger_, &DebugController::paused, this,
          &MainWindow::onDebugPaused);
  connect(debugger_, &DebugController::finished, this,
          &MainWindow::onDebugFinished);
  connect(debugger_, &DebugController::errored, this,
          &MainWindow::onDebugErrored);

  editor_->setPlainText(
      QStringLiteral("let x = 2 + 2;\nprint(\"x = {}\", x);\n"));
  dirty_ = false;
  updateWindowTitle();
  if (darkTheme_) {
    applyTheme(true);
  }
}

void MainWindow::createDocks() {
  console_ = new ConsoleDock(this);
  vars_ = new VarsDock(this);
  funs_ = new FunsDock(this);
  consts_ = new ConstsDock(this);
  disasm_ = new DisasmDock(this);

  addDockWidget(Qt::BottomDockWidgetArea, console_);
  addDockWidget(Qt::RightDockWidgetArea, vars_);
  addDockWidget(Qt::RightDockWidgetArea, funs_);
  addDockWidget(Qt::RightDockWidgetArea, consts_);
  addDockWidget(Qt::RightDockWidgetArea, disasm_);
  tabifyDockWidget(vars_, funs_);
  tabifyDockWidget(funs_, consts_);
  tabifyDockWidget(consts_, disasm_);
  vars_->raise();
}

void MainWindow::createMenus() {
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("&Файл"));
  fileMenu->addAction(QStringLiteral("Новий"), this, &MainWindow::newFile,
                      QKeySequence::New);
  fileMenu->addAction(QStringLiteral("Відкрити…"), this, &MainWindow::openFile,
                      QKeySequence::Open);
  fileMenu->addAction(QStringLiteral("Зберегти"), this, &MainWindow::saveFile,
                      QKeySequence::Save);
  fileMenu->addAction(QStringLiteral("Зберегти як…"), this,
                      &MainWindow::saveFileAs, QKeySequence::SaveAs);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("Вийти"), this, &QWidget::close,
                      QKeySequence::Quit);

  auto *editMenu = menuBar()->addMenu(QStringLiteral("&Редагування"));
  editMenu->addAction(QStringLiteral("Форматувати"), this,
                      &MainWindow::formatSource,
                      QKeySequence(Qt::CTRL | Qt::Key_I));
  auto *formatOnSave =
      editMenu->addAction(QStringLiteral("Форматувати при збереженні"));
  formatOnSave->setCheckable(true);
  formatOnSave->setChecked(formatOnSave_);
  connect(formatOnSave, &QAction::toggled, this,
          &MainWindow::toggleFormatOnSave);

  auto *runMenu = menuBar()->addMenu(QStringLiteral("&Виконання"));
  runMenu->addAction(QStringLiteral("Запустити"), this, &MainWindow::runProgram,
                     QKeySequence(Qt::CTRL | Qt::Key_R));
  runMenu->addSeparator();
  runMenu->addAction(QStringLiteral("Дебаг"), this, &MainWindow::debugStart,
                     QKeySequence(Qt::SHIFT | Qt::Key_F5));
  runMenu->addAction(QStringLiteral("Продовжити / Запуск"), this,
                     &MainWindow::debugContinue, QKeySequence(Qt::Key_F5));
  runMenu->addAction(
      QStringLiteral("Точка зупину"), this,
      [this] {
        const int line = editor_->textCursor().blockNumber() + 1;
        editor_->toggleBreakpoint(line);
      },
      QKeySequence(Qt::Key_F9));
  runMenu->addAction(QStringLiteral("Пауза"), this, &MainWindow::debugPause);
  runMenu->addAction(QStringLiteral("Стоп"), this, &MainWindow::debugStop);
  runMenu->addAction(QStringLiteral("Крок усередину"), this,
                     &MainWindow::debugStepInto, QKeySequence(Qt::Key_F11));
  runMenu->addAction(QStringLiteral("Крок через"), this,
                     &MainWindow::debugStepOver, QKeySequence(Qt::Key_F10));
  runMenu->addAction(QStringLiteral("Крок назовні"), this,
                     &MainWindow::debugStepOut,
                     QKeySequence(Qt::SHIFT | Qt::Key_F11));

  auto *viewMenu = menuBar()->addMenu(QStringLiteral("&Вигляд"));
  viewMenu->addAction(QStringLiteral("Редактор"), this,
                      &MainWindow::switchEditorMode);
  viewMenu->addAction(QStringLiteral("Інтерактив"), this,
                      &MainWindow::switchReplMode);
  viewMenu->addSeparator();
  viewMenu->addAction(console_->toggleViewAction());
  viewMenu->addAction(vars_->toggleViewAction());
  viewMenu->addAction(funs_->toggleViewAction());
  viewMenu->addAction(consts_->toggleViewAction());
  viewMenu->addAction(disasm_->toggleViewAction());
  viewMenu->addSeparator();
  viewMenu->addAction(QStringLiteral("Світла / темна тема"), this,
                      &MainWindow::toggleTheme);

  auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Довідка"));
  auto *helpAction = helpMenu->addAction(QStringLiteral("Як користуватись…"),
                                         this, &MainWindow::openHelp);
  helpAction->setShortcut(QKeySequence::HelpContents);
  helpMenu->addSeparator();
  helpMenu->addAction(QStringLiteral("Про програму…"), this,
                      &MainWindow::openAbout);
  helpMenu->addAction(QStringLiteral("Про Qt…"), this,
                      &MainWindow::openAboutQt);
}

void MainWindow::createToolbars() {
  auto *tb = addToolBar(QStringLiteral("Основна"));
  tb->setObjectName(QStringLiteral("MainToolbar"));
  tb->addAction(QStringLiteral("▶ Запуск"), this, &MainWindow::runProgram);
  tb->addAction(QStringLiteral("🐞 Дебаг"), this, &MainWindow::debugStart);
  tb->addAction(QStringLiteral("▶▶"), this, &MainWindow::debugContinue);
  tb->addAction(QStringLiteral("⏸"), this, &MainWindow::debugPause);
  tb->addAction(QStringLiteral("⏹"), this, &MainWindow::debugStop);
  tb->addAction(QStringLiteral("F11"), this, &MainWindow::debugStepInto);
  tb->addAction(QStringLiteral("F10"), this, &MainWindow::debugStepOver);
  tb->addSeparator();
  tb->addAction(QStringLiteral("Редактор"), this,
                &MainWindow::switchEditorMode);
  tb->addAction(QStringLiteral("Інтерактив"), this,
                &MainWindow::switchReplMode);
}

void MainWindow::createShortcuts() {
  // F9 toggles breakpoint on current line when not starting debug from menu —
  // already used for debug start; gutter click for BP.
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (maybeSave()) {
    event->accept();
  } else {
    event->ignore();
  }
}

bool MainWindow::maybeSave() {
  if (!dirty_) {
    return true;
  }
  const auto r = QMessageBox::warning(
      this, QStringLiteral("calx IDE"),
      QStringLiteral("Документ змінено. Зберегти?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  if (r == QMessageBox::Save) {
    return saveFile();
  }
  if (r == QMessageBox::Cancel) {
    return false;
  }
  return true;
}

void MainWindow::setCurrentFile(const QString &path) {
  currentFile_ = path;
  dirty_ = false;
  updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
  QString name = currentFile_.isEmpty() ? QStringLiteral("безіменний.calc")
                                        : QFileInfo(currentFile_).fileName();
  if (dirty_) {
    name.prepend(QLatin1Char('*'));
  }
  setWindowTitle(name + QStringLiteral(" — calx IDE"));
}

void MainWindow::newFile() {
  if (!maybeSave()) {
    return;
  }
  editor_->clear();
  editor_->clearBreakpoints();
  editor_->clearDiagnostics();
  setCurrentFile({});
  switchEditorMode();
}

void MainWindow::openPath(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, QStringLiteral("Помилка"),
                         QStringLiteral("Не вдалося відкрити файл."));
    return;
  }
  editor_->setPlainText(QString::fromUtf8(f.readAll()));
  editor_->clearBreakpoints();
  editor_->clearDiagnostics();
  setCurrentFile(path);
  switchEditorMode();
}

void MainWindow::openFile() {
  if (!maybeSave()) {
    return;
  }
  const QString path =
      QFileDialog::getOpenFileName(this, QStringLiteral("Відкрити"), {},
                                   QStringLiteral("calx (*.calc);;Усі (*)"));
  if (path.isEmpty()) {
    return;
  }
  openPath(path);
}

bool MainWindow::formatBeforeSave() {
  if (!formatOnSave_ || stack_->currentWidget() != editor_) {
    return true;
  }
  QString err;
  if (!editor_->formatDocument(&err)) {
    console_->appendError(err);
    const auto ans = QMessageBox::warning(
        this, QStringLiteral("Форматування"),
        QStringLiteral(
            "Автоформат не вдався (помилка розбору).\nЗберегти як є?\n\n%1")
            .arg(err),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return ans == QMessageBox::Yes;
  }
  return true;
}

bool MainWindow::saveFile() {
  if (currentFile_.isEmpty()) {
    return saveFileAs();
  }
  if (!formatBeforeSave()) {
    return false;
  }
  QFile f(currentFile_);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, QStringLiteral("Помилка"),
                         QStringLiteral("Не вдалося зберегти файл."));
    return false;
  }
  f.write(editor_->toPlainText().toUtf8());
  dirty_ = false;
  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("Збережено"), 2000);
  return true;
}

bool MainWindow::saveFileAs() {
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Зберегти як"), QStringLiteral("program.calc"),
      QStringLiteral("calx (*.calc);;Усі (*)"));
  if (path.isEmpty()) {
    return false;
  }
  setCurrentFile(path);
  return saveFile();
}

void MainWindow::formatSource() {
  if (stack_->currentWidget() != editor_) {
    return;
  }
  QString err;
  if (!editor_->formatDocument(&err)) {
    console_->appendError(err);
    statusBar()->showMessage(QStringLiteral("Форматування: помилка розбору"),
                             5000);
    return;
  }
  statusBar()->showMessage(QStringLiteral("Відформатовано"), 2000);
}

void MainWindow::toggleFormatOnSave(bool enabled) {
  formatOnSave_ = enabled;
  QSettings settings;
  settings.setValue(QStringLiteral("editor/formatOnSave"), formatOnSave_);
  statusBar()->showMessage(
      formatOnSave_ ? QStringLiteral("Автоформат при збереженні: увімкнено")
                    : QStringLiteral("Автоформат при збереженні: вимкнено"),
      3000);
}

void MainWindow::switchEditorMode() {
  stack_->setCurrentWidget(editor_);
  statusMode_->setText(QStringLiteral("Редактор"));
}

void MainWindow::switchReplMode() {
  stack_->setCurrentWidget(repl_);
  statusMode_->setText(QStringLiteral("Інтерактив"));
}

void MainWindow::toggleTheme() {
  darkTheme_ = !darkTheme_;
  QSettings settings;
  settings.setValue(QStringLiteral("ui/darkTheme"), darkTheme_);
  applyTheme(darkTheme_);
}

void MainWindow::openHelp() {
  HelpDialog dialog(this);
  dialog.exec();
}

void MainWindow::openAbout() {
  const QString text =
      QStringLiteral(
          "<h3>calx IDE %1</h3>"
          "<p>Редактор і інтерактивне середовище для мови <b>calx</b>.</p>"
          "<p>"
          "Версія програми: <b>%1</b><br/>"
          "Час останньої збірки: <b>%2</b>"
          "</p>"
          "<p>© 2026 Ihor Dolinskyi<br/>"
          "Ліцензія: GNU GPL версії 3 або пізнішої.<br/>"
          "Подробиці про сторонні компоненти — у файлі NOTICE.</p>")
          .arg(QStringLiteral(CALX_IDE_VERSION_STRING),
               QStringLiteral(CALX_IDE_BUILD_TIME_READABLE));
  QMessageBox::about(this, QStringLiteral("Про програму"), text);
}

void MainWindow::openAboutQt() {
  QMessageBox::aboutQt(this, QStringLiteral("Про Qt"));
}

void MainWindow::applyTheme(bool dark) {
  qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  if (dark) {
    QPalette p;
    const QColor window(45, 45, 48);
    const QColor base(30, 30, 32);
    const QColor alt(38, 38, 42);
    const QColor text(220, 220, 220);
    const QColor disabled(140, 140, 140);
    const QColor button(55, 55, 58);
    const QColor highlight(42, 130, 218);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alt);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, QColor(255, 80, 80));
    p.setColor(QPalette::ToolTipBase, QColor(50, 50, 54));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, highlight);
    p.setColor(QPalette::LinkVisited, QColor(160, 120, 200));
    p.setColor(QPalette::Light, QColor(70, 70, 74));
    p.setColor(QPalette::Midlight, QColor(60, 60, 64));
    p.setColor(QPalette::Dark, QColor(25, 25, 28));
    p.setColor(QPalette::Mid, QColor(48, 48, 52));
    p.setColor(QPalette::Shadow, Qt::black);

    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(70, 70, 70));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);

    qApp->setPalette(p);
    qApp->setStyleSheet(
        QStringLiteral("QToolBar { spacing: 4px; }"
                       "QToolButton { color: #dcdcdc; }"
                       "QHeaderView::section {"
                       "  background-color: #37373a; color: #dcdcdc;"
                       "  padding: 4px; border: 1px solid #2a2a2d;"
                       "}"
                       "QDockWidget::title {"
                       "  background: #37373a; color: #dcdcdc;"
                       "  padding: 4px; text-align: left;"
                       "}"
                       "QDockWidget { color: #dcdcdc; }"
                       "QTabBar::tab { color: #dcdcdc; background: #2e2e32;"
                       "  padding: 4px 10px; }"
                       "QTabBar::tab:selected { background: #3d3d42; }"
                       "QStatusBar { color: #dcdcdc; }"
                       "QMenuBar { color: #dcdcdc; background: #2d2d30; }"
                       "QMenuBar::item:selected { background: #3e3e42; }"
                       "QMenu { color: #dcdcdc; background: #2d2d30; }"
                       "QMenu::item:selected { background: #094771; }"
                       "QTreeWidget, QPlainTextEdit, QTextEdit {"
                       "  background-color: #1e1e20; color: #dcdcdc;"
                       "  selection-background-color: #2a82da;"
                       "}"));
  } else {
    qApp->setStyleSheet({});
    qApp->setPalette(
        QStyleFactory::create(QStringLiteral("Fusion"))->standardPalette());
  }
  // Оновити підсвітку редакторів після зміни палітри.
  editor_->syncThemeFromPalette();
  // ReplWidget містить CalxEditor — він отримає PaletteChange сам.
}

void MainWindow::runProgram() {
  switchEditorMode();
  debugger_->stop();
  debugging_ = false;
  editor_->setCurrentDebugLine(0);
  editor_->clearDiagnostics();
  console_->clear();

  const auto src = editor_->toPlainText().toStdString();
  auto compiled = calc::compile_source(src);
  if (!compiled) {
    const auto &d = compiled.error();
    editor_->setDiagnosticUnderline(static_cast<int>(d.pos.line),
                                    static_cast<int>(d.pos.column), 1);
    console_->appendError(
        QStringLiteral("помилка:%1:%2: %3")
            .arg(d.pos.line)
            .arg(d.pos.column)
            .arg(d.message.empty()
                     ? QString::fromStdString(calc::error_message(d.code))
                     : QString::fromStdString(d.message)));
    statusBar()->showMessage(QStringLiteral("Помилка компіляції"), 5000);
    return;
  }

  // Імена fun для completer.
  QStringList funNames;
  for (const auto &fn : compiled->functions) {
    funNames << QString::fromStdString(fn.name);
  }
  auto lexed = calc::tokenize(src);
  for (const auto &tok : lexed.tokens) {
    if (tok.kind == calc::TokenKind::Identifier) {
      funNames << QString::fromUtf8(tok.text.data(),
                                    static_cast<int>(tok.text.size()));
    }
  }
  editor_->setExtraCompletions(funNames);

  std::ostringstream captured;
  auto ran = calc::execute_with_locals(*compiled, captured);
  if (!ran) {
    const auto &d = ran.error();
    console_->appendError(
        QStringLiteral("помилка:%1:%2: %3")
            .arg(d.pos.line)
            .arg(d.pos.column)
            .arg(d.message.empty()
                     ? QString::fromStdString(calc::error_message(d.code))
                     : QString::fromStdString(d.message)));
    statusBar()->showMessage(QStringLiteral("Помилка виконання"), 5000);
    funs_->setChunk(*compiled);
    disasm_->setChunk(*compiled);
    return;
  }

  const QString out = QString::fromStdString(captured.str());
  if (!out.isEmpty()) {
    console_->appendText(out);
  }
  vars_->setLocals(compiled->local_names, ran->locals);
  funs_->setChunk(*compiled);
  disasm_->setChunk(*compiled, ran->locals);
  statusBar()->showMessage(QStringLiteral("Виконано"), 3000);
}

void MainWindow::debugStart() {
  switchEditorMode();
  editor_->clearDiagnostics();
  console_->clear();
  const auto src = editor_->toPlainText().toStdString();
  auto compiled = calc::compile_source(src);
  if (!compiled) {
    const auto &d = compiled.error();
    editor_->setDiagnosticUnderline(static_cast<int>(d.pos.line),
                                    static_cast<int>(d.pos.column), 1);
    console_->appendError(
        QStringLiteral("помилка:%1:%2: %3")
            .arg(d.pos.line)
            .arg(d.pos.column)
            .arg(QString::fromStdString(
                d.message.empty() ? calc::error_message(d.code) : d.message)));
    return;
  }
  funs_->setChunk(*compiled);
  debugging_ = true;
  debugger_->start(*compiled, editor_->breakpoints());
  statusBar()->showMessage(QStringLiteral("Дебаг: на паузі"));
}

void MainWindow::debugContinue() {
  if (!debugging_) {
    runProgram();
    return;
  }
  debugger_->continueRun();
}

void MainWindow::debugPause() { debugger_->pause(); }

void MainWindow::debugStop() {
  debugger_->stop();
  debugging_ = false;
  editor_->setCurrentDebugLine(0);
  statusBar()->showMessage(QStringLiteral("Дебаг зупинено"), 3000);
}

void MainWindow::debugStepInto() {
  if (!debugging_) {
    debugStart();
  }
  debugger_->stepInto();
}

void MainWindow::debugStepOver() {
  if (!debugging_) {
    debugStart();
  }
  debugger_->stepOver();
}

void MainWindow::debugStepOut() {
  if (!debugging_) {
    debugStart();
  }
  debugger_->stepOut();
}

void MainWindow::onBreakpointsChanged() {
  if (debugging_) {
    debugger_->syncBreakpoints(editor_->breakpoints());
  }
}

void MainWindow::onDebugPaused(int line, quint64 ip, int frames,
                               const QString &outputDelta,
                               const QStringList &localNames,
                               const QStringList &localValues,
                               const QStringList &localTypes,
                               const QString &disasmText) {
  Q_UNUSED(ip);
  if (!outputDelta.isEmpty()) {
    console_->appendText(outputDelta);
  }
  editor_->setCurrentDebugLine(line);
  vars_->setFormatted(localNames, localTypes, localValues);
  disasm_->setText(disasmText);
  statusBar()->showMessage(
      QStringLiteral("Пауза: рядок %1, кадрів %2").arg(line).arg(frames));
}

void MainWindow::onDebugFinished(const QString &outputDelta) {
  if (!outputDelta.isEmpty()) {
    console_->appendText(outputDelta);
  }
  editor_->setCurrentDebugLine(0);
  debugging_ = false;
  statusBar()->showMessage(QStringLiteral("Дебаг завершено"), 3000);
}

void MainWindow::onDebugErrored(const QString &message,
                                const QString &outputDelta) {
  if (!outputDelta.isEmpty()) {
    console_->appendText(outputDelta);
  }
  console_->appendError(message);
  editor_->setCurrentDebugLine(0);
  debugging_ = false;
  statusBar()->showMessage(QStringLiteral("Помилка дебагу"), 5000);
}

void MainWindow::onReplSessionUpdated(const QString &source,
                                      const QString &delta) {
  Q_UNUSED(delta);
  if (source.isEmpty()) {
    vars_->clear();
    funs_->clear();
    disasm_->clear();
    return;
  }
  auto compiled = calc::compile_source(source.toStdString());
  if (!compiled) {
    return;
  }
  std::ostringstream discarded;
  auto ran = calc::execute_with_locals(*compiled, discarded);
  if (!ran) {
    funs_->setChunk(*compiled);
    return;
  }
  vars_->setLocals(compiled->local_names, ran->locals);
  funs_->setChunk(*compiled);
  disasm_->setChunk(*compiled, ran->locals);
}
