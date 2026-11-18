#include "examples/ExamplesDialog.h"

#include "editor/CalxHighlighter.h"

#include <QCloseEvent>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

struct Example {
  QString category;
  QString title;
  QString blurb;
  QString code;
};

[[nodiscard]] QString takeField(QStringList &lines, const QString &key) {
  const QString prefix = QStringLiteral("// ") + key + QStringLiteral(": ");
  if (lines.isEmpty() || !lines.first().startsWith(prefix)) {
    return {};
  }
  return lines.takeFirst().mid(prefix.size()).trimmed();
}

[[nodiscard]] Example parseExample(const QString &fileName, const QString &text) {
  QStringList lines = text.split(QLatin1Char('\n'));
  Example ex;
  ex.category = takeField(lines, QStringLiteral("category"));
  ex.title = takeField(lines, QStringLiteral("title"));
  ex.blurb = takeField(lines, QStringLiteral("blurb"));
  if (ex.title.isEmpty()) {
    ex.title = fileName;
  }
  if (ex.category.isEmpty()) {
    ex.category = QStringLiteral("Інше");
  }
  while (!lines.isEmpty() && lines.first().isEmpty()) {
    lines.removeFirst();
  }
  ex.code = lines.join(QLatin1Char('\n'));
  if (!ex.code.endsWith(QLatin1Char('\n'))) {
    ex.code.append(QLatin1Char('\n'));
  }
  return ex;
}

} // namespace

ExamplesDialog::ExamplesDialog(QWidget *parent) : QWidget(parent, Qt::Window) {
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowFlag(Qt::Window, true);
  setWindowTitle(QStringLiteral("Приклади — calx"));
  resize(920, 560);

  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  tree_->setMinimumWidth(220);
  tree_->setMaximumWidth(280);

  blurb_ = new QLabel(this);
  blurb_->setWordWrap(true);

  code_ = new QPlainTextEdit(this);
  code_->setReadOnly(true);
  code_->setLineWrapMode(QPlainTextEdit::NoWrap);
  code_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  highlighter_ = new CalxHighlighter(code_->document());

  auto *openBtn = new QPushButton(QStringLiteral("Відкрити в редакторі"), this);
  openBtn->setDefault(true);
  auto *closeBtn = new QPushButton(QStringLiteral("Закрити"), this);

  auto *right = new QVBoxLayout;
  right->addWidget(blurb_);
  right->addWidget(code_, 1);
  auto *buttons = new QHBoxLayout;
  buttons->addStretch(1);
  buttons->addWidget(openBtn);
  buttons->addWidget(closeBtn);
  right->addLayout(buttons);

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->addWidget(tree_);
  root->addLayout(right, 1);

  connect(tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current, QTreeWidgetItem *) { showItem(current); });
  connect(tree_, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item, int) {
            showItem(item);
            emitCurrent();
          });
  connect(openBtn, &QPushButton::clicked, this, &ExamplesDialog::emitCurrent);
  connect(closeBtn, &QPushButton::clicked, this, &QWidget::hide);

  loadCatalog();
  syncHighlight();
}

void ExamplesDialog::loadCatalog() {
  QDir dir(QStringLiteral(":/examples"));
  const QStringList names = dir.entryList(QStringList{QStringLiteral("*.calc")},
                                           QDir::Files, QDir::Name);
  QTreeWidgetItem *first = nullptr;
  for (const QString &name : names) {
    QFile file(dir.filePath(name));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    const Example ex =
        parseExample(name, QString::fromUtf8(file.readAll()));
    QTreeWidgetItem *group = nullptr;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      if (tree_->topLevelItem(i)->text(0) == ex.category) {
        group = tree_->topLevelItem(i);
        break;
      }
    }
    if (group == nullptr) {
      group = new QTreeWidgetItem(tree_, {ex.category});
      group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
      group->setExpanded(true);
    }
    auto *leaf = new QTreeWidgetItem(group, {ex.title});
    leaf->setData(0, Qt::UserRole, ex.code);
    leaf->setData(0, Qt::UserRole + 1, ex.blurb);
    leaf->setToolTip(0, ex.blurb);
    if (first == nullptr) {
      first = leaf;
    }
  }
  if (first != nullptr) {
    tree_->setCurrentItem(first);
  } else {
    blurb_->setText(QStringLiteral("У каталозі немає прикладів."));
  }
  setWindowTitle(QStringLiteral("Приклади — calx (%1)")
                     .arg(names.size()));
}

void ExamplesDialog::showItem(QTreeWidgetItem *item) {
  if (item == nullptr || item->parent() == nullptr) {
    return;
  }
  blurb_->setText(item->data(0, Qt::UserRole + 1).toString());
  code_->setPlainText(item->data(0, Qt::UserRole).toString());
}

void ExamplesDialog::syncHighlight() {
  const bool dark = palette().color(QPalette::Base).lightness() < 128;
  highlighter_->setDark(dark);
}

void ExamplesDialog::emitCurrent() {
  auto *item = tree_->currentItem();
  if (item == nullptr || item->parent() == nullptr) {
    return;
  }
  openInEditor(item->data(0, Qt::UserRole).toString());
}

void ExamplesDialog::closeEvent(QCloseEvent *event) {
  hide();
  event->ignore();
}

void ExamplesDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange) {
    syncHighlight();
  }
  QWidget::changeEvent(event);
}
