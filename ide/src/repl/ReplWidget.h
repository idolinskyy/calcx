#pragma once

#include <QWidget>
#include <QString>
#include <string>

class CalxEditor;
class QPlainTextEdit;
class QLineEdit;
class QLabel;

class ReplWidget final : public QWidget {
  Q_OBJECT
public:
  explicit ReplWidget(QWidget *parent = nullptr);
  ~ReplWidget() override;

  [[nodiscard]] QString sessionSource() const;
  void clearSession();

signals:
  void sessionUpdated(const QString &source, const QString &outputDelta);

private slots:
  void onSubmit();

private:
  QPlainTextEdit *output_{nullptr};
  CalxEditor *input_{nullptr};
  QLabel *prompt_{nullptr};

  std::string session_;
  std::string prev_output_;
  std::string pending_;
  QStringList history_;
  int history_index_{-1};

  void appendOut(const QString &text);
  void appendErr(const QString &text);
  void loadHistory();
  void saveHistory();
  void setPrompt(bool continuation);
};
