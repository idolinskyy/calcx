#include "docks/VarsDock.h"

#include "calc/value.hpp"

VarsDock::VarsDock(QWidget *parent)
    : QDockWidget(QStringLiteral("Змінні"), parent) {
  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabels(
      {QStringLiteral("Імʼя"), QStringLiteral("Тип"), QStringLiteral("Значення")});
  tree_->setRootIsDecorated(false);
  setWidget(tree_);
  setObjectName(QStringLiteral("VarsDock"));
}

void VarsDock::setLocals(const std::vector<std::string> &names,
                         const std::vector<calc::Value> &values) {
  tree_->clear();
  const std::size_t n = values.size();
  for (std::size_t i = 0; i < n; ++i) {
    QString name;
    if (i < names.size() && !names[i].empty()) {
      name = QString::fromStdString(names[i]);
      if (name.startsWith(QLatin1String("__"))) {
        continue; // службові for-in
      }
    } else {
      name = QStringLiteral("[%1]").arg(i);
    }
    auto *item = new QTreeWidgetItem(tree_);
    item->setText(0, name);
    item->setText(1, QString::fromUtf8(calc::value_type_name(values[i]).data(),
                                       static_cast<int>(
                                           calc::value_type_name(values[i]).size())));
    item->setText(2, QString::fromStdString(calc::format_value(values[i])));
  }
}

void VarsDock::setFormatted(const QStringList &names, const QStringList &types,
                            const QStringList &values) {
  tree_->clear();
  const int n = names.size();
  for (int i = 0; i < n; ++i) {
    auto *item = new QTreeWidgetItem(tree_);
    item->setText(0, names[i]);
    item->setText(1, i < types.size() ? types[i] : QString());
    item->setText(2, i < values.size() ? values[i] : QString());
  }
}

void VarsDock::clear() { tree_->clear(); }
