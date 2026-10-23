#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("calx-ide"));
  QApplication::setApplicationVersion(QStringLiteral(CALX_IDE_VERSION_STRING));
  QApplication::setOrganizationName(QStringLiteral("calx"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("calx IDE — редактор і інтерактив для мови calx"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(QStringLiteral("file"),
                               QStringLiteral("Файл .calc для відкриття"),
                               QStringLiteral("[file]"));
  parser.process(app);

  MainWindow win;
  const QStringList args = parser.positionalArguments();
  if (!args.isEmpty() && QFile::exists(args.front())) {
    win.openPath(args.front());
  }
  win.show();
  return app.exec();
}
