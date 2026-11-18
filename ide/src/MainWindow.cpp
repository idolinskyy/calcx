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
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTextCursor>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>
#include <sstream>

namespace {

enum class Glyph {
  Run,
  Debug,
  Continue,
  Pause,
  Stop,
  StepInto,
  StepOver,
  StepOut,
  Editor,
  Repl
};

void stroke(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(c);
}

void drawGlyph(QPainter &p, Glyph g, const QColor &c) {
  stroke(p, c);
  switch (g) {
  case Glyph::Run: {
    QPainterPath path;
    path.moveTo(6.5, 4.5);
    path.lineTo(15.5, 10);
    path.lineTo(6.5, 15.5);
    path.closeSubpath();
    p.drawPath(path);
    break;
  }
  case Glyph::Debug: {
    QPainterPath path;
    path.moveTo(5.5, 3.5);
    path.lineTo(13.5, 9);
    path.lineTo(5.5, 14.5);
    path.closeSubpath();
    p.drawPath(path);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(15.2, 15.0), 2.2, 2.2);
    break;
  }
  case Glyph::Continue: {
    QPainterPath a;
    a.moveTo(3.5, 5);
    a.lineTo(10, 10);
    a.lineTo(3.5, 15);
    a.closeSubpath();
    QPainterPath b;
    b.moveTo(10, 5);
    b.lineTo(16.5, 10);
    b.lineTo(10, 15);
    b.closeSubpath();
    p.drawPath(a);
    p.drawPath(b);
    break;
  }
  case Glyph::Pause:
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(5.5, 4.5, 3.2, 11), 0.8, 0.8);
    p.drawRoundedRect(QRectF(11.3, 4.5, 3.2, 11), 0.8, 0.8);
    break;
  case Glyph::Stop:
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(5.5, 5.5, 9, 9), 1.2, 1.2);
    break;
  case Glyph::StepInto: {
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(10, 3.5), QPointF(10, 12.5));
    QPainterPath arrow;
    arrow.moveTo(6.5, 10.5);
    arrow.lineTo(10, 15.5);
    arrow.lineTo(13.5, 10.5);
    p.setBrush(c);
    p.drawPath(arrow);
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(5, 16.5), QPointF(15, 16.5));
    break;
  }
  case Glyph::StepOver: {
    p.setBrush(Qt::NoBrush);
    QPainterPath arc;
    arc.moveTo(4.5, 13);
    arc.cubicTo(4.5, 5.5, 15.5, 5.5, 15.5, 11);
    p.drawPath(arc);
    QPainterPath arrow;
    arrow.moveTo(12.5, 8.5);
    arrow.lineTo(16.2, 12.2);
    arrow.lineTo(11.2, 13.2);
    p.setBrush(c);
    p.drawPath(arrow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(10, 15.5), 1.6, 1.6);
    break;
  }
  case Glyph::StepOut: {
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(5, 16.5), QPointF(15, 16.5));
    p.drawLine(QPointF(10, 15.5), QPointF(10, 6.5));
    QPainterPath arrow;
    arrow.moveTo(6.5, 9.5);
    arrow.lineTo(10, 4.5);
    arrow.lineTo(13.5, 9.5);
    p.setBrush(c);
    p.drawPath(arrow);
    break;
  }
  case Glyph::Editor:
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(4.5, 3.5, 11, 13), 1.2, 1.2);
    p.drawLine(QPointF(7, 7), QPointF(13, 7));
    p.drawLine(QPointF(7, 10), QPointF(13, 10));
    p.drawLine(QPointF(7, 13), QPointF(11, 13));
    break;
  case Glyph::Repl:
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(3.5, 4.5, 13, 11), 1.2, 1.2);
    p.drawLine(QPointF(6, 8), QPointF(9, 10.5));
    p.drawLine(QPointF(9, 10.5), QPointF(6, 13));
    p.drawLine(QPointF(10.5, 13), QPointF(14, 13));
    break;
  }
}

class CalxToolButton final : public QToolButton {
public:
  CalxToolButton(Glyph glyph, bool mode, QWidget *parent)
      : QToolButton(parent), glyph_(glyph), mode_(mode) {
    setAutoRaise(true);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(30, 28);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
  }

  void setDarkUi(bool dark) {
    dark_ = dark;
    update();
  }

protected:
  void enterEvent(QEnterEvent *event) override {
    hovered_ = true;
    QToolButton::enterEvent(event);
    update();
  }
  void leaveEvent(QEvent *event) override {
    hovered_ = false;
    QToolButton::leaveEvent(event);
    update();
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor ink =
        dark_ ? QColor(0xE8, 0xE8, 0xE8) : QColor(0x2A, 0x2A, 0x2A);
    const QColor accent =
        dark_ ? QColor(0x6C, 0xCB, 0xFF) : QColor(0x0B, 0x6A, 0xC9);
    if (isEnabled() && (hovered_ || isDown())) {
      const QColor wash =
          isDown()
              ? (dark_ ? QColor(0x4A, 0x4A, 0x50) : QColor(0xD2, 0xD2, 0xD2))
              : (dark_ ? QColor(0x3C, 0x3C, 0x42) : QColor(0xE2, 0xE2, 0xE2));
      p.setPen(Qt::NoPen);
      p.setBrush(wash);
      p.drawRoundedRect(QRectF(2, 2, width() - 4, height() - 4), 4, 4);
    }
    if (isChecked()) {
      p.fillRect(QRect(7, height() - 2, width() - 14, 2), accent);
    }
    p.save();
    p.translate((width() - 20) / 2.0, (height() - 20) / 2.0 - 1);
    const QColor glyphColor =
        !isEnabled() ? QColor(ink.red(), ink.green(), ink.blue(), 70)
                     : (mode_ && isChecked() ? accent : ink);
    drawGlyph(p, glyph_, glyphColor);
    p.restore();
  }

private:
  Glyph glyph_;
  bool mode_{false};
  bool dark_{false};
  bool hovered_{false};
};

std::vector<CalxToolButton *> toolbarButtons;

void addTool(QToolBar *tb, QAction *action, Glyph glyph, bool mode) {
  auto *btn = new CalxToolButton(glyph, mode, tb);
  btn->setDefaultAction(action);
  QObject::connect(action, &QAction::changed, btn,
                   qOverload<>(&QWidget::update));
  tb->addWidget(btn);
  toolbarButtons.push_back(btn);
}

} // namespace

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

  statusPos_ = new QLabel(QStringLiteral("Рядок 1, стовпець 1"), this);
  statusPos_->setMinimumWidth(statusPos_->fontMetrics().horizontalAdvance(
      QStringLiteral("Рядок 99999, стовпець 999  ·  виділено 99999")));
  statusBar()->addPermanentWidget(statusPos_);
  statusMode_ = new QLabel(QStringLiteral("Редактор"), this);
  statusBar()->addPermanentWidget(statusMode_);
  statusBar()->showMessage(QStringLiteral("Готово"));

  connect(editor_, &CalxEditor::textChanged, this, [this] {
    dirty_ = true;
    updateWindowTitle();
  });
  connect(editor_, &CalxEditor::cursorPositionChanged, this,
          &MainWindow::refreshCursorStatus);
  connect(editor_, &CalxEditor::selectionChanged, this,
          &MainWindow::refreshCursorStatus);
  connect(repl_, &ReplWidget::cursorMoved, this,
          &MainWindow::refreshCursorStatus);
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
  connect(debugger_, &DebugController::busyChanged, this,
          &MainWindow::onDebugBusyChanged);

  editor_->setPlainText(
      QStringLiteral("let x = 2 + 2;\nprint(\"x = {}\", x);\n"));
  dirty_ = false;
  updateWindowTitle();
  applyTheme(darkTheme_);
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

  actRun_ = new QAction(QStringLiteral("Запустити"), this);
  actRun_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
  actRun_->setToolTip(QStringLiteral("Запустити програму (Ctrl+R)"));
  connect(actRun_, &QAction::triggered, this, &MainWindow::runProgram);

  actDebug_ = new QAction(QStringLiteral("Дебаг"), this);
  actDebug_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  actDebug_->setToolTip(QStringLiteral("Почати дебаг (Shift+F5)"));
  connect(actDebug_, &QAction::triggered, this, &MainWindow::debugStart);

  actContinue_ = new QAction(QStringLiteral("Продовжити"), this);
  actContinue_->setShortcut(QKeySequence(Qt::Key_F5));
  actContinue_->setToolTip(QStringLiteral("Продовжити / запуск (F5)"));
  connect(actContinue_, &QAction::triggered, this, &MainWindow::debugContinue);

  actPause_ = new QAction(QStringLiteral("Пауза"), this);
  actPause_->setToolTip(QStringLiteral("Пауза"));
  connect(actPause_, &QAction::triggered, this, &MainWindow::debugPause);

  actStop_ = new QAction(QStringLiteral("Стоп"), this);
  actStop_->setToolTip(QStringLiteral("Зупинити дебаг"));
  connect(actStop_, &QAction::triggered, this, &MainWindow::debugStop);

  actStepInto_ = new QAction(QStringLiteral("Крок усередину"), this);
  actStepInto_->setShortcut(QKeySequence(Qt::Key_F11));
  actStepInto_->setToolTip(QStringLiteral("Крок усередину (F11)"));
  connect(actStepInto_, &QAction::triggered, this, &MainWindow::debugStepInto);

  actStepOver_ = new QAction(QStringLiteral("Крок через"), this);
  actStepOver_->setShortcut(QKeySequence(Qt::Key_F10));
  actStepOver_->setToolTip(QStringLiteral("Крок через (F10)"));
  connect(actStepOver_, &QAction::triggered, this, &MainWindow::debugStepOver);

  actStepOut_ = new QAction(QStringLiteral("Крок назовні"), this);
  actStepOut_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
  actStepOut_->setToolTip(QStringLiteral("Крок назовні (Shift+F11)"));
  connect(actStepOut_, &QAction::triggered, this, &MainWindow::debugStepOut);

  actEditorMode_ = new QAction(QStringLiteral("Редактор"), this);
  actEditorMode_->setToolTip(QStringLiteral("Режим редактора"));
  actEditorMode_->setCheckable(true);
  actEditorMode_->setChecked(true);
  connect(actEditorMode_, &QAction::triggered, this,
          &MainWindow::switchEditorMode);

  actReplMode_ = new QAction(QStringLiteral("Інтерактив"), this);
  actReplMode_->setToolTip(QStringLiteral("Інтерактивний калькулятор"));
  actReplMode_->setCheckable(true);
  connect(actReplMode_, &QAction::triggered, this, &MainWindow::switchReplMode);

  auto *modeGroup = new QActionGroup(this);
  modeGroup->setExclusive(true);
  modeGroup->addAction(actEditorMode_);
  modeGroup->addAction(actReplMode_);

  auto *runMenu = menuBar()->addMenu(QStringLiteral("&Виконання"));
  runMenu->addAction(actRun_);
  runMenu->addSeparator();
  runMenu->addAction(actDebug_);
  runMenu->addAction(actContinue_);
  runMenu->addAction(
      QStringLiteral("Точка зупину"), this,
      [this] {
        const int line = editor_->textCursor().blockNumber() + 1;
        editor_->toggleBreakpoint(line);
      },
      QKeySequence(Qt::Key_F9));
  runMenu->addAction(actPause_);
  runMenu->addAction(actStop_);
  runMenu->addSeparator();
  runMenu->addAction(actStepInto_);
  runMenu->addAction(actStepOver_);
  runMenu->addAction(actStepOut_);

  auto *viewMenu = menuBar()->addMenu(QStringLiteral("&Вигляд"));
  viewMenu->addAction(actEditorMode_);
  viewMenu->addAction(actReplMode_);
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

  updateToolbarIcons();
}

void MainWindow::createToolbars() {
  auto *tb = addToolBar(QStringLiteral("Основна"));
  tb->setObjectName(QStringLiteral("MainToolbar"));
  tb->setMovable(false);
  tb->setFloatable(false);
  tb->setIconSize(QSize(18, 18));

  addTool(tb, actRun_, Glyph::Run, false);
  addTool(tb, actDebug_, Glyph::Debug, false);
  tb->addSeparator();
  addTool(tb, actContinue_, Glyph::Continue, false);
  addTool(tb, actPause_, Glyph::Pause, false);
  addTool(tb, actStop_, Glyph::Stop, false);
  tb->addSeparator();
  addTool(tb, actStepInto_, Glyph::StepInto, false);
  addTool(tb, actStepOver_, Glyph::StepOver, false);
  addTool(tb, actStepOut_, Glyph::StepOut, false);
  tb->addSeparator();
  addTool(tb, actEditorMode_, Glyph::Editor, true);
  addTool(tb, actReplMode_, Glyph::Repl, true);
}

void MainWindow::updateToolbarIcons() {
  for (CalxToolButton *btn : toolbarButtons) {
    btn->setDarkUi(darkTheme_);
  }
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

void MainWindow::refreshCursorStatus() {
  if (!statusPos_) {
    return;
  }
  int line = 1;
  int column = 1;
  int selected = 0;
  if (stack_ && repl_ && stack_->currentWidget() == repl_) {
    repl_->cursorPlace(line, column, selected);
  } else if (editor_) {
    const QTextCursor c = editor_->textCursor();
    line = c.blockNumber() + 1;
    column = c.positionInBlock() + 1;
    selected = c.selectionEnd() - c.selectionStart();
  }
  QString text = QStringLiteral("Рядок %1, стовпець %2").arg(line).arg(column);
  if (selected > 0) {
    text += QStringLiteral("  ·  виділено %1").arg(selected);
  }
  statusPos_->setText(text);
}

void MainWindow::switchEditorMode() {
  stack_->setCurrentWidget(editor_);
  statusMode_->setText(QStringLiteral("Редактор"));
  if (actEditorMode_ && !actEditorMode_->isChecked()) {
    actEditorMode_->setChecked(true);
  }
  updateToolbarIcons();
  refreshCursorStatus();
}

void MainWindow::switchReplMode() {
  stack_->setCurrentWidget(repl_);
  statusMode_->setText(QStringLiteral("Інтерактив"));
  if (actReplMode_ && !actReplMode_->isChecked()) {
    actReplMode_->setChecked(true);
  }
  updateToolbarIcons();
  refreshCursorStatus();
}

void MainWindow::toggleTheme() {
  darkTheme_ = !darkTheme_;
  QSettings settings;
  settings.setValue(QStringLiteral("ui/darkTheme"), darkTheme_);
  applyTheme(darkTheme_);
}

void MainWindow::openHelp() {
  if (!help_) {
    help_ = new HelpDialog(this);
  }
  help_->openSection(QStringLiteral("overview"));
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
        QStringLiteral("QToolBar {"
                       "  spacing: 2px; padding: 2px 4px;"
                       "  background: #2d2d30; border: none;"
                       "  border-bottom: 1px solid #3c3c40;"
                       "}"
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
  updateToolbarIcons();
  editor_->syncThemeFromPalette();
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

  std::ostringstream captured;
  auto ran = calc::execute_with_locals(*compiled, captured);
  if (!ran) {
    const auto &d = ran.error();
    editor_->setDiagnosticUnderline(static_cast<int>(d.pos.line),
                                    static_cast<int>(d.pos.column), 1);
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

void MainWindow::onDebugBusyChanged(bool busy) {
  const bool enable = !busy;
  if (actContinue_) {
    actContinue_->setEnabled(enable);
  }
  if (actStepInto_) {
    actStepInto_->setEnabled(enable);
  }
  if (actStepOver_) {
    actStepOver_->setEnabled(enable);
  }
  if (actStepOut_) {
    actStepOut_->setEnabled(enable);
  }
}

void MainWindow::setDiagnosticFromMessage(const QString &message) {
  // "помилка:LINE:COL: …"
  const QString prefix = QStringLiteral("помилка:");
  if (!message.startsWith(prefix)) {
    return;
  }
  const QString rest = message.mid(prefix.size());
  const auto colon1 = rest.indexOf(QLatin1Char(':'));
  if (colon1 <= 0) {
    return;
  }
  const auto colon2 = rest.indexOf(QLatin1Char(':'), colon1 + 1);
  if (colon2 <= colon1) {
    return;
  }
  bool ok_line = false;
  bool ok_col = false;
  const int line = rest.left(colon1).toInt(&ok_line);
  const int col = rest.mid(colon1 + 1, colon2 - colon1 - 1).toInt(&ok_col);
  if (ok_line && ok_col && line > 0) {
    editor_->setDiagnosticUnderline(line, col > 0 ? col : 1, 1);
  }
}

void MainWindow::onDebugErrored(const QString &message,
                                const QString &outputDelta) {
  if (!outputDelta.isEmpty()) {
    console_->appendText(outputDelta);
  }
  console_->appendError(message);
  setDiagnosticFromMessage(message);
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
