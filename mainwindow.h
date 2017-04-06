#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QMainWindow>
#include <QMap>

namespace Ui {
  class MainWindow;
}

#define MAX_ROWS 20

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
  void analyzeConfigurationFiles(QString &directory);
  void saveConfigurationFile(QString &directory);
  void parseJson(QString &json_string_parameter, QComboBox *combo_bos);

  QMap<QString, QWidget*> section_tabs;
  QMap<QString, QWidget*> elements;
  QMap<QString, QWidget*> values;
  QMap<QString, QString> original_values;
  QString original_file_data;

  QMap<QString, QWidget*> section_tabs_adv;
  QMap<QString, QWidget*> elements_adv;
  QMap<QString, QWidget*> values_adv;
  QMap<QString, QString> original_values_adv;
  QString original_file_data_adv;
};

#endif // MAINWINDOW_H
