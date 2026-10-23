#pragma once

#include <QDialog>
#include <QString>

class QListWidget;
class QTextBrowser;

class HelpDialog final : public QDialog {
  Q_OBJECT
public:
  explicit HelpDialog(QWidget *parent = nullptr);
  explicit HelpDialog(const QString &section_title, QWidget *parent = nullptr);

private:
  void finishSetup(int initial_row);
  void showSection(int index);
  [[nodiscard]] static QString pageChrome(const QString &body_html);
  [[nodiscard]] static int sectionIndexByTitle(const QString &title);

  QListWidget *nav_{nullptr};
  QTextBrowser *browser_{nullptr};
};
