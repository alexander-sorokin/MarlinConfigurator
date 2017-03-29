#include "mainwindow.h"
#include "ui_mainwindow.h"

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
