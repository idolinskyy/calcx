#include "docks/ConstsDock.h"
#include "util/CalxAssist.h"

#include "calc/constants.hpp"
#include "calc/value.hpp"

ConstsDock::ConstsDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Константи"), parent) {
  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabels(
      {QStringLiteral("Імʼя"), QStringLiteral("Значення")});
  tree_->setRootIsDecorated(false);
  setWidget(tree_);
  setObjectName(QStringLiteral("ConstsDock"));
  refresh();
}

void ConstsDock::refresh() {
  tree_->clear();
  for (const QString &name : calx::constantNames()) {
    if (const auto v = calc::find_constant(name.toStdString())) {
      auto *item = new QTreeWidgetItem(tree_);
      item->setText(0, name);
      item->setText(1, QString::fromStdString(calc::format_value(*v)));
    }
  }
}
