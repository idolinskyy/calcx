#include "docks/FunsDock.h"

FunsDock::FunsDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Функції"), parent) {
  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabels({QStringLiteral("Імʼя"), QStringLiteral("Арність"),
                          QStringLiteral("Entry")});
  tree_->setRootIsDecorated(false);
  setWidget(tree_);
  setObjectName(QStringLiteral("FunsDock"));
}

void FunsDock::setChunk(const calc::Chunk &chunk) {
  tree_->clear();
  for (const auto &fn : chunk.functions) {
    auto *item = new QTreeWidgetItem(tree_);
    item->setText(0, QString::fromStdString(fn.name));
    item->setText(1, QString::number(fn.arity));
    item->setText(2, QString::number(fn.entry));
  }
}

void FunsDock::clear() { tree_->clear(); }
