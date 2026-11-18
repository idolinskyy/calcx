#pragma once

#include <QString>
#include <QWidget>

class QCloseEvent;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class CalxHighlighter;

/// Каталог готових програм: список ліворуч, текст праворуч.
class ExamplesDialog final : public QWidget {
  Q_OBJECT
public:
  explicit ExamplesDialog(QWidget *parent = nullptr);

signals:
  void openInEditor(const QString &source);

protected:
  void closeEvent(QCloseEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  void loadCatalog();
  void showItem(QTreeWidgetItem *item);
  void syncHighlight();
  void emitCurrent();

  QTreeWidget *tree_{nullptr};
  QLabel *blurb_{nullptr};
  QPlainTextEdit *code_{nullptr};
  CalxHighlighter *highlighter_{nullptr};
};
