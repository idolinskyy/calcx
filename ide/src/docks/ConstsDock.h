#pragma once

#include <QDockWidget>
#include <QTreeWidget>

class ConstsDock final : public QDockWidget {
  Q_OBJECT
public:
  explicit ConstsDock(QWidget *parent = nullptr);

  void refresh();

private:
  QTreeWidget *tree_{nullptr};
};
