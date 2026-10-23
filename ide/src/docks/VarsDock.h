#pragma once

#include "calc/value.hpp"

#include <QDockWidget>
#include <QStringList>
#include <QTreeWidget>
#include <string>
#include <vector>

class VarsDock final : public QDockWidget {
  Q_OBJECT
public:
  explicit VarsDock(QWidget *parent = nullptr);

  void setLocals(const std::vector<std::string> &names,
                 const std::vector<calc::Value> &values);
  void setFormatted(const QStringList &names, const QStringList &types,
                    const QStringList &values);
  void clear();

private:
  QTreeWidget *tree_{nullptr};
};
