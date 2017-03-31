#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormBuilder>
#include <QGroupBox>
#include <QLineEdit>
#include <QSettings>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  loadSettings();
  connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSettings);
  ui->tabWidget->clear();
  connect(ui->action_select_marlin_location, &QAction::triggered, [=](bool){
    QSettings settings;
    QFileDialog directory_dialog(this);
    directory_dialog.setDirectory(settings.value("directory", QDir::homePath()).toString());
    directory_dialog.setFileMode(QFileDialog::Directory);
    if (directory_dialog.exec()) {
      analyzeConfigurationFiles(directory_dialog.selectedFiles().first());
      settings.setValue("directory", directory_dialog.directory().path());
    }
  });
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

  qDebug() << inputFile.fileName();

  if (!inputFile.open(QFile::ReadOnly)) {
    return;
  }

  QString file_data = inputFile.readAll();
  inputFile.close();

  QRegularExpression section_regexp{"@section (?<title>\\w+)", QRegularExpression::CaseInsensitiveOption};

  QRegularExpressionMatchIterator section_match_iterator = section_regexp.globalMatch(file_data);
//[\\s\\t]*//[\\s\\t]*?(?<comment_pre>.*?)?
  QRegularExpression define_regexp{""
                                   "\\n[\\s\\t]*?(?<disabled>[/]*)?"
                                   "[\\s\\t]*#define"
                                   "[\\s\\t]*(?<name>\\w+)"
                                   "[\\s\\t]*?(?<value>[^\\n/]*)?"
                                   "[\\s\\t/]*?(?<comment>[^\\n]*)?", QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};

  QMap<int, QString> sections;
  QList<int> section_addresses;

  QMap<QString, QWidget*> section_tabs;
  QMap<QString, QWidget*> elements;

  while (section_match_iterator.hasNext()) {
      QRegularExpressionMatch match = section_match_iterator.next();
      sections.insert(match.capturedStart("title"), match.captured("title"));
      section_addresses.append(match.capturedStart("title"));
      QString tab_title = match.captured("title");
      tab_title[0] = tab_title[0].toUpper();
      if (!section_tabs.contains(match.captured("title"))) {
        QWidget *new_tab = new QWidget(ui->tabWidget);
        QGridLayout *new_layout = new QGridLayout(new_tab);
        new_layout->setProperty("row", 0);
        new_layout->setProperty("column", 0);
        new_tab->setLayout(new_layout);
        ui->tabWidget->addTab(new_tab, tab_title);
        section_tabs.insert(match.captured("title"), new_tab);
      }
  }

  QRegularExpression if_regexp{"#if (?<condition>ENABLED|DISABLED)\\((?<title>\\w+)\\)", QRegularExpression::CaseInsensitiveOption};
  QRegularExpression if_value_regexp{"#if (?<title>\\w+)[\\s\\t]*(?<sign>[\\>\\<\\=]+)[\\s\\t]*(?<value>\\w+)", QRegularExpression::CaseInsensitiveOption};

  for (int i = 0; i < section_addresses.count(); ++i) {
    int begin = section_addresses[i];
    int end = (section_addresses.count() - 1 == i) ? file_data.size() : section_addresses[i + 1] ;
    qDebug() << "\nSECTION" << sections.value(section_addresses[i]);
    QWidget *current_tab = section_tabs.value(sections.value(section_addresses[i]));

    QStringRef section_data = file_data.midRef(begin, end - begin);

    QRegularExpressionMatchIterator if_match_iterator = if_regexp.globalMatch(section_data);

    QMap<int, QString> ifs;
    QList<int> if_addresses;

    while (if_match_iterator.hasNext()) {
        QRegularExpressionMatch match = if_match_iterator.next();
        int begin_if_match = match.capturedStart("title");
        qDebug() << match.captured("condition") + " " + match.captured("title") << begin_if_match << section_data.indexOf("#endif", begin_if_match);
        ifs.insert(begin_if_match, match.captured("condition") + " " + match.captured("title"));
        if_addresses.append(begin_if_match);
        if_addresses.append(section_data.indexOf("#endif", begin_if_match));
    }

    QRegularExpressionMatchIterator if_value_match_iterator = if_value_regexp.globalMatch(section_data);

    QMap<int, QString> if_values;
    QList<int> if_value_addresses;

    while (if_value_match_iterator.hasNext()) {
        QRegularExpressionMatch match = if_value_match_iterator.next();
        qDebug() << match.captured("title") << match.captured("sign") << match.captured("value");
        int begin_if_match = match.capturedStart("title");
        if_values.insert(begin_if_match, match.captured("title") + " " + match.captured("sign") + " " + match.captured("value"));
        if_value_addresses.append(begin_if_match);
        if_value_addresses.append(section_data.indexOf("#endif", begin_if_match));
    }

    QString found_if;

    QRegularExpressionMatchIterator define_match_iterator = define_regexp.globalMatch(section_data);
    while (define_match_iterator.hasNext()) {
        QRegularExpressionMatch match = define_match_iterator.next();
        int begin = match.capturedStart("name");
        QString tabs = "\t";
        QWidget *depends_on = nullptr;
        bool depend_type = true;
        bool found_if_b =  false;
        for (int j = 0; j < if_addresses.count(); j += 2) {
          int end_of_if = (j == if_addresses.count() - 1) ? section_data.size() : if_addresses[j + 1];
          if (if_addresses[j] < begin && end_of_if > begin) {
            tabs += "\t";
            found_if_b = true;
            if (found_if.isEmpty()) {
              found_if = ifs.value(if_addresses[j]);
              qDebug() << tabs.toStdString().data() << "#if" << found_if;

            }
            QStringList found_list = found_if.split(' ');
            if (found_list.count() == 2) {
              depends_on = elements.value(found_list[1], nullptr);
              if (depends_on != nullptr && !qobject_cast<QGroupBox*>(depends_on)) {
                QGroupBox *new_box = new QGroupBox(qobject_cast<QCheckBox*>(depends_on)->text(), current_tab);
                QGridLayout *new_layout = new QGridLayout(new_box);
                new_layout->setProperty("row", 0);
                new_layout->setProperty("column", 0);
                new_box->setLayout(new_layout);
                current_tab->layout()->replaceWidget(depends_on, new_box);
                new_box->setCheckable(true);
                new_box->setChecked(qobject_cast<QCheckBox*>(depends_on)->isChecked());
                new_box->setToolTip(depends_on->toolTip());
                elements.remove(found_list[1]);
                elements.insert(found_list[1], new_box);
                depends_on->deleteLater();
                depends_on = new_box;
              }
              depend_type = found_list[0].toUpper() == "ENABLED";
            }
            break;
          }
        }
        if (found_if_b == false) {
          found_if.clear();
        }
        for (int j = 0; j < if_value_addresses.count(); j += 2) {
          int end_of_if = (j == if_value_addresses.count() - 1) ? section_data.size() : if_value_addresses[j + 1];
          if (if_value_addresses[j] < begin && end_of_if > begin) {
            tabs += "\t";
            if (found_if.isEmpty()) {
              found_if = if_values.value(if_value_addresses[j]);
              qDebug() << tabs.toStdString().data() << "#if" << found_if;
            }
            break;
          }
          found_if.clear();
        }
        QCheckBox *new_checkbox = new QCheckBox(match.captured("name"), current_tab);
        new_checkbox->setChecked(match.captured("disabled") != "//");
        elements.insert(match.captured("name"), new_checkbox);
        if (depends_on) {
          int row = qobject_cast<QGridLayout*>(depends_on->layout())->property("row").toInt();
          int column = qobject_cast<QGridLayout*>(depends_on->layout())->property("column").toInt();
          if (row > 15) {
            qobject_cast<QGridLayout*>(depends_on->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), row, column);
            row = 0;
            column += 2;
            qobject_cast<QGridLayout*>(depends_on->layout())->setProperty("column", column);
          }
          new_checkbox->setParent(depends_on);
          qobject_cast<QGridLayout*>(depends_on->layout())->addWidget(new_checkbox, row, column);
          if (depend_type) {
            connect(qobject_cast<QGroupBox*>(depends_on), &QGroupBox::toggled, new_checkbox, &QCheckBox::setEnabled);
          }
          else {
            connect(qobject_cast<QGroupBox*>(depends_on), &QGroupBox::toggled, new_checkbox, &QCheckBox::setDisabled);
          }
          new_checkbox->setEnabled(qobject_cast<QGroupBox*>(depends_on)->isChecked());
          QString item_value = match.captured("value").trimmed();
          if (!item_value.isEmpty()) {
            QWidget *value_edit = nullptr;
            if (item_value.toLower() == "true" || item_value.toLower() == "false") {
              value_edit = new QCheckBox(depends_on);
              qobject_cast<QCheckBox*>(value_edit)->setChecked(item_value.toLower() == "true");
            }
            else {
              value_edit = new QLineEdit(item_value, depends_on);
            }
            if (value_edit) {
              value_edit->setEnabled(new_checkbox->isChecked());
              connect(new_checkbox, &QCheckBox::toggled, value_edit, &QLineEdit::setEnabled);
              qobject_cast<QGridLayout*>(depends_on->layout())->addWidget(value_edit, row, column + 1);
            }
          }
          qobject_cast<QGridLayout*>(depends_on->layout())->setProperty("row", ++row);
        }
        else {
          int row = qobject_cast<QGridLayout*>(current_tab->layout())->property("row").toInt();
          int column = qobject_cast<QGridLayout*>(current_tab->layout())->property("column").toInt();
          if (row > 15) {
            qobject_cast<QGridLayout*>(current_tab->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), row, column);
            row = 0;
            column += 2;
            qobject_cast<QGridLayout*>(current_tab->layout())->setProperty("column", column);
          }
          qobject_cast<QGridLayout*>(current_tab->layout())->addWidget(new_checkbox, row, column);
          QString item_value = match.captured("value").trimmed();
          if (!item_value.isEmpty()) {
            QWidget *value_edit = nullptr;
            if (item_value.toLower() == "true" || item_value.toLower() == "false") {
              value_edit = new QCheckBox(current_tab);
              qobject_cast<QCheckBox*>(value_edit)->setChecked(item_value.toLower() == "true");
            }
            else {
              value_edit = new QLineEdit(item_value, current_tab);
            }
            if (value_edit) {
              value_edit->setEnabled(new_checkbox->isChecked());
              connect(new_checkbox, &QCheckBox::toggled, value_edit, &QLineEdit::setEnabled);
              qobject_cast<QGridLayout*>(current_tab->layout())->addWidget(value_edit, row, column + 1);
            }
          }
          qobject_cast<QGridLayout*>(current_tab->layout())->setProperty("row", ++row);
        }
        new_checkbox->setToolTip(match.captured("comment").trimmed());
        if (new_checkbox->toolTip().isEmpty()) {
          new_checkbox->setToolTip(match.captured("comment_pre").trimmed());
        }
        if (!new_checkbox->toolTip().isEmpty()) {
          new_checkbox->setIcon(QIcon("/home/alexander/icons/oxygen/base/32x32/status/dialog-information.png"));
        }
        qDebug() << tabs.toStdString().data() << match.captured("disabled") << match.captured("name") << match.captured("value") << match.captured("comment") << match.captured("comment_pre"); //.remove(QRegularExpression("[/\\n]"))
    }
  }

  QMapIterator<QString, QWidget*> sections_iterator{section_tabs};
  while (sections_iterator.hasNext()) {
    sections_iterator.next();
    QWidget *section = sections_iterator.value();
    QList<QGroupBox*> groups = section->findChildren<QGroupBox*>();
    int row = qobject_cast<QGridLayout*>(section->layout())->property("row").toInt();
    int column = qobject_cast<QGridLayout*>(section->layout())->property("column").toInt();
    QWidget *container_widget = new QWidget(section);
    container_widget->setLayout(new QGridLayout(container_widget));
    qobject_cast<QGridLayout*>(section->layout())->addWidget(container_widget, 0, column + 2, 15, 2);
    row = 0;
    column = 0;
    foreach (QGroupBox *group, groups) {
//      if (row > 15) {
//        qobject_cast<QGridLayout*>(section->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), row, column);
//        row = 0;
//        column += 2;
//        qobject_cast<QGridLayout*>(section->layout())->setProperty("column", column);
//      }
      ++row;
      if (group->height() > container_widget->height()) {
        ++column;
        row = 0;
      }
      qobject_cast<QGridLayout*>(section->layout())->removeWidget(group);
      qobject_cast<QGridLayout*>(container_widget->layout())->addWidget(group, row, column);

//      qobject_cast<QGridLayout*>(section->layout())->addWidget(group, ++row, column);
    }
    QList<QCheckBox*> checks = section->findChildren<QCheckBox*>();
    row = qobject_cast<QGridLayout*>(section->layout())->property("row").toInt();
    column = qobject_cast<QGridLayout*>(section->layout())->property("column").toInt();
    QPoint place{-1, -1};
    QList<QPoint> places;
    for (int c = 0; c < column + 2; c += 2) {
      for (int r = 0; r < 16; ++r) {
        QLayoutItem *item = qobject_cast<QGridLayout*>(section->layout())->itemAtPosition(r, c);
        if (!item) {
          qDebug() << "No item at" << r << c;
          places.append(QPoint(c, r));
          continue;
        }
        QWidget *tem_widget = item->widget();
        if (!qobject_cast<QCheckBox*>(tem_widget)) {
          continue;
        }
        if (tem_widget) {
          if (!places.isEmpty()) {
            QPoint place = places.takeFirst();
            qobject_cast<QGridLayout*>(section->layout())->removeWidget(tem_widget);
            qobject_cast<QGridLayout*>(section->layout())->addWidget(tem_widget, place.y(), place.x());
            QLayoutItem *input_item = qobject_cast<QGridLayout*>(section->layout())->itemAtPosition(r, c + 1);
            if (input_item) {
              QWidget *tem_input_widget = input_item->widget();
              if (tem_input_widget) {
                qobject_cast<QGridLayout*>(section->layout())->removeWidget(tem_input_widget);
                qobject_cast<QGridLayout*>(section->layout())->addWidget(tem_input_widget, place.y(), place.x() + 1);
              }
            }
            qDebug() << "Moving" << qobject_cast<QCheckBox*>(tem_widget)->text() << place.y() << place.x();
            places.append(QPoint(c, r));
          }
        }
        else {
          places.append(QPoint(c, r));
        }
      }
    }
//    foreach (QCheckBox *check, checks) {
//      if (row > 15) {
//        row = 0;
//        column += 2;
//      }
//      QWidget *tem_widget = qobject_cast<QGridLayout*>(section->layout())->itemAtPosition(row, column)->widget();
//      ++row;
//    }
  }
}
