#include "mainwindow.h"
#include "version.h"
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QTranslator>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc == 2) {
    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "-V") == 0) {
      // Printing application name and version to console and closing application.
      std::cout << VER_PRODUCTNAME_STR << " " << VER_FILEVERSION_STR << std::endl
                << VER_COMPANYNAME_STR << " " << "(" << VER_COMPANYEMAIL_STR << ")" << std::endl;
      exit(1);
    }
  }

  QApplication a(argc, argv);

  // Setting application properties. Required for settings storage.
  // Changing name, organization or domain will cause settings storage location change.
  a.setApplicationName(VER_PRODUCTNAME_STR);
  a.setOrganizationName(VER_COMPANYNAME_STR);
  a.setOrganizationDomain(VER_COMPANYDOMAIN_STR);
  a.setApplicationVersion(VER_FILEVERSION_STR);

  // Loading translation. By default local language will be used if it not set in settings.
  // If translation file not found all text will be diplayed as it is in code and UI files.
  QSettings settings;
  QString language = settings.value("language", QLocale().name()).toString();
  QTranslator translator;
  if (translator.load(QString(":/language/marlin-configurator_%1").arg(language))) {
    a.installTranslator(&translator);
  }

  MainWindow w;
  w.setWindowTitle(QString("%1 %2").arg(VER_PRODUCTNAME_STR).arg(VER_FILEVERSION_STR));
  w.setWindowIcon(QIcon(":/images/main-icon.png"));
  w.show();

  return a.exec();
}
