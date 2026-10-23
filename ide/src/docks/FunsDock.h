#pragma once

#include "calc/chunk.hpp"

#include <QDockWidget>
#include <QTreeWidget>

class FunsDock final : public QDockWidget {
  Q_OBJECT
public:
  explicit FunsDock(QWidget *parent = nullptr);

  void setChunk(const calc::Chunk &chunk);
  void clear();

private:
  QTreeWidget *tree_{nullptr};
};
