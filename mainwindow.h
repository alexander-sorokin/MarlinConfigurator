#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void changeEvent(QEvent *e);

private:
  Ui::MainWindow *ui;
  void loadSettings();
  void saveSettings();
};

#endif // MAINWINDOW_H
