#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  loadSettings();
  connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSettings);
}

MainWindow::~MainWindow() {
  delete ui;
}

void MainWindow::changeEvent(QEvent *e) {
  QMainWindow::changeEvent(e);
  switch (e->type()) {
  case QEvent::LanguageChange:
    ui->retranslateUi(this);
    break;
  default:
    break;
  }
}

void MainWindow::loadSettings() {
  QSettings settings;
  restoreGeometry(settings.value("geometry", "").toByteArray());
  restoreState(settings.value("state", "").toByteArray());
}

void MainWindow::saveSettings() {
  QSettings settings;
  settings.setValue("geometry", saveGeometry());
  settings.setValue("state", saveState());
}

void MainWindow::analyzeConfigurationFiles(QString &directory) {
  QFile inputFile{directory + QDir::separator() + "Configuration.h"};

  if (!inputFile.open(QFile::ReadOnly)) {
    return;
  }

  QString file_data = inputFile.readAll();
  inputFile.close();

  QRegularExpression section_regexp{"@section (?<title>\\w+)", QRegularExpression::CaseInsensitiveOption};

  QRegularExpressionMatchIterator section_match_iterator = section_regexp.globalMatch(file_data);

  QRegularExpression define_regexp{"[\\s\\t]*(?<disabled>[/]*)"
                                   "[\\s\\t]*#define"
                                   "[\\s\\t]*(?<name>\\w+)"
                                   "[\\s\\t]*?(?<value>[^\\n/]*)?"
                                   "[\\s\\t/]*?(?<comment>[^\\n/]*)?\\n",
                                   QRegularExpression::CaseInsensitiveOption};

  QMap<int, QString> sections;
  QList<int> section_addresses;

  while (section_match_iterator.hasNext()) {
    QRegularExpressionMatch match = section_match_iterator.next();
    sections.insert(match.capturedStart("title"), match.captured("title"));
    section_addresses.append(match.capturedStart("title"));
  }

  for (int i = 0; i < section_addresses.count(); ++i) {
    int begin = section_addresses[i];
    int end = (section_addresses.count() - 1 == i) ? file_data.size() : section_addresses[i + 1];
    qDebug() << sections.value(section_addresses[i]);
    QRegularExpressionMatchIterator define_match_iterator = define_regexp.globalMatch(file_data, begin);
    while (define_match_iterator.hasNext()) {
      QRegularExpressionMatch match = define_match_iterator.next();
      if (match.capturedStart("name") < end) {
        qDebug() << "\t" << match.captured("disabled") << match.captured("name") << match.captured("value")
                 << match.captured("comment");
      }
    }
  }
}
