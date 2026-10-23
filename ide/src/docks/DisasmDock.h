#pragma once

#include "calc/chunk.hpp"
#include "calc/value.hpp"

#include <QDockWidget>
#include <QPlainTextEdit>
#include <span>
#include <vector>

class DisasmDock final : public QDockWidget {
  Q_OBJECT
public:
  explicit DisasmDock(QWidget *parent = nullptr);

  void setChunk(const calc::Chunk &chunk,
                std::span<const calc::Value> locals = {},
                int highlightIp = -1);
  void setText(const QString &text);
  void clear();

private:
  QPlainTextEdit *view_{nullptr};
};
