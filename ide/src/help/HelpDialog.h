#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QTextBrowser>
#include <QVector>
#include <QWidget>

class QCloseEvent;
class QLabel;
class QListWidget;
class QUrl;

/// QTextBrowser з ресурсами codeblock:N (відмальовані картки коду).
class HelpBrowser final : public QTextBrowser {
  Q_OBJECT
public:
  explicit HelpBrowser(QWidget *parent = nullptr);

  void setCodeBlockImages(QVector<QImage> images);

protected:
  [[nodiscard]] QVariant loadResource(int type, const QUrl &name) override;

private:
  QVector<QImage> codeImages_;
};

/// Немодальне вікно довідки (можна тримати відкритим поряд з IDE).
class HelpDialog final : public QWidget {
  Q_OBJECT
public:
  explicit HelpDialog(QWidget *parent = nullptr);
  /// Відкрити розділ за id (`overview`) або заголовком (`Огляд`).
  void openSection(const QString &section_title_or_id);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onAnchorClicked(const QUrl &url);

private:
  void finishSetup(int initial_row);
  void showSection(int index);
  [[nodiscard]] QString enrichHtml(const QString &body_html);
  [[nodiscard]] static QString pageChrome(const QString &body_html);
  [[nodiscard]] static int sectionIndexByTitle(const QString &title);
  [[nodiscard]] QImage renderCodeCard(const QString &plain_code,
                                      qreal dpr) const;

  QListWidget *nav_{nullptr};
  HelpBrowser *browser_{nullptr};
  QLabel *copyHint_{nullptr};
  QString pendingAnchor_;
  QStringList copySnippets_;
  QVector<QImage> codeImages_;
};
