#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormBuilder>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
  connect(ui->action_save_configuration, &QAction::triggered, [=](bool){
    QSettings settings;
    QFileDialog directory_dialog(this);
    directory_dialog.setDirectory(settings.value("directory", QDir::homePath()).toString());
    directory_dialog.setFileMode(QFileDialog::Directory);
    if (directory_dialog.exec()) {
      saveConfigurationFile(directory_dialog.selectedFiles().first());
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

//  qDebug() << inputFile.fileName();

  if (!inputFile.open(QFile::ReadOnly)) {
    return;
  }

  QString file_data = inputFile.readAll();
  inputFile.close();

  original_file_data = file_data;

  QRegularExpression section_regexp{"@section (?<title>\\w+)", QRegularExpression::CaseInsensitiveOption};

  QRegularExpressionMatchIterator section_match_iterator = section_regexp.globalMatch(file_data);
//[\\s\\t]*//[\\s\\t]*?(?<comment_pre>.*?)?
//  QRegularExpression define_regexp{"(?<comment_pre>\\n{2}[\\s\\t]*//.*?)?"
//                                   "\\n[\\s\\t]*?(?<disabled>[/]*)?"
//                                   "[\\s\\t]*#define"
//                                   "[\\s\\t]*(?<name>\\w+)"
//                                   "[\\s\\t]*?(?<value>[^\\n]*)?"
//                                   "[\\s\\t]*?(?<comment>//[^\\n]*)?", QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};

  QRegularExpression define_regexp{"(?<comment_pre>\\n{2}[\\s\\t]*/[/*][^#].*?)?"
                                   "\\n[\\s\\t]*?(?<disabled>[/]*)?"
                                   "[\\s\\t]*#define"
                                   "[\\s\\t]*(?<name>\\w+)"
                                   "[\\s\\t]*?(?<value>[^\\n/]*)?"
                                   "[\\s\\t]*?(?<comment>[^\\n]*)?", QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};


  QMap<int, QString> sections;
  QList<int> section_addresses;
  QMap<QString, QLayout*> items_layouts;

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
//    qDebug() << "\nSECTION" << sections.value(section_addresses[i]);
    QWidget *current_tab = section_tabs.value(sections.value(section_addresses[i]));

    QStringRef section_data = file_data.midRef(begin, end - begin);

    QRegularExpressionMatchIterator if_match_iterator = if_regexp.globalMatch(section_data);

    QMap<int, QString> ifs;
    QList<int> if_addresses;

    while (if_match_iterator.hasNext()) {
        QRegularExpressionMatch match = if_match_iterator.next();
        int begin_if_match = match.capturedStart("title");
//        qDebug() << match.captured("condition") + " " + match.captured("title") << begin_if_match << section_data.indexOf("#endif", begin_if_match);
        ifs.insert(begin_if_match, match.captured("condition") + " " + match.captured("title"));
        if_addresses.append(begin_if_match);
        if_addresses.append(section_data.indexOf("#endif", begin_if_match));
    }

    QRegularExpressionMatchIterator if_value_match_iterator = if_value_regexp.globalMatch(section_data);

    QMap<int, QString> if_values;
    QList<int> if_value_addresses;

    while (if_value_match_iterator.hasNext()) {
        QRegularExpressionMatch match = if_value_match_iterator.next();
//        qDebug() << match.captured("title") << match.captured("sign") << match.captured("value");
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
//              qDebug() << tabs.toStdString().data() << "#if" << found_if;

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
//              qDebug() << tabs.toStdString().data() << "#if" << found_if;
            }
            break;
          }
          found_if.clear();
        }

        bool disabled = match.captured("disabled").trimmed() != "//";
        QString name = match.captured("name").trimmed();
        QString value = match.captured("value").trimmed();
        QString comment = match.captured("comment").trimmed();
        QString comment_pre = match.captured("comment_pre").replace(QRegularExpression{"\\n[\\s\\t/\\*]+"}, "\n").trimmed();//.remove(QRegularExpression{"[^:]//"})
        if (comment.left(2) != "//") {
          int index_of_backslash = comment.indexOf(QRegularExpression{"[^:]//"});
          if (index_of_backslash > 0) {
            value = match.captured("value") + comment.left(index_of_backslash);
            value = value.trimmed();
            comment = comment.right(comment.length() - (index_of_backslash + 3)).trimmed();
          }
          else {
            value = match.captured("value") + comment;
            value = value.trimmed();
            comment = "";
          }
        }
        else {
          comment = comment.remove(QRegularExpression{"^//"}).trimmed();
        }
        if (!comment.isEmpty()) {
          comment[0] = comment[0].toUpper();
        }
        QString json_data;
        if (!comment_pre.isEmpty()) {
          comment_pre[0] = comment_pre[0].toUpper();
          QRegularExpression json_expression{":[\\s\\t]*(?<json>[\\[\\{].*?[\\]\\}])"};
          QRegularExpressionMatch json_expression_match = json_expression.match(comment_pre);
          if (json_expression_match.hasMatch()) {
            json_data = json_expression_match.captured("json");
            comment_pre.remove(json_expression);
          }
        }
        comment += '\n' + comment_pre;
        comment = comment.trimmed();
        if (elements.contains(name)) {
          QWidget *value_edit = values.value(name, nullptr);
          QComboBox *value_combo_box = qobject_cast<QComboBox*>(value_edit);
          QLineEdit *value_line_edit = qobject_cast<QLineEdit*>(value_edit);
          if (value_combo_box != nullptr) {
            //if (value_combo_box->findText(value) == -1) {
              value_combo_box->addItem(value);
            //}
          }
          else if (value_line_edit != nullptr) {
            if (items_layouts.contains(name) && value_line_edit->text() != value) {
              value_combo_box = new QComboBox(items_layouts.value(name)->widget());
              value_combo_box->addItem(value_line_edit->text());
              value_combo_box->addItem(value);
              value_combo_box->setEditable(true);
              value_combo_box->setEnabled(value_line_edit->isEnabled());
              QCheckBox *depend_on_checkbox = static_cast<QCheckBox*>(value_line_edit->property("checkbox").value<void*>());
              if (depend_on_checkbox) {
                connect(depend_on_checkbox, &QCheckBox::toggled, value_combo_box, &QWidget::setEnabled);
              }
              items_layouts.value(name)->replaceWidget(value_line_edit, value_combo_box);
              value_line_edit->deleteLater();
              values.remove(name);
              values.insert(name, value_combo_box);
            }
          }
          if (value_edit && !value.isEmpty()) {
            value_edit->setToolTip((value_edit->toolTip().isEmpty() ? "" : value_edit->toolTip() + "\n") + comment + " - " + value);
          }
        }
        else {
          original_values.insert(name, value);
          QCheckBox *new_checkbox = new QCheckBox(name, current_tab);
          new_checkbox->setChecked(disabled);
          elements.insert(name, new_checkbox);
          if (depends_on) {
            int row = qobject_cast<QGridLayout*>(depends_on->layout())->property("row").toInt();
            int column = qobject_cast<QGridLayout*>(depends_on->layout())->property("column").toInt();
            if (row > MAX_ROWS) {
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
            QString item_value = value;
            if (!item_value.isEmpty()) {
              QWidget *value_edit = nullptr;
              if (item_value.toLower() == "true" || item_value.toLower() == "false") {
                value_edit = new QCheckBox(depends_on);
                qobject_cast<QCheckBox*>(value_edit)->setChecked(item_value.toLower() == "true");
              }
              else {
                if (json_data.isEmpty()) {
                  value_edit = new QLineEdit(item_value, depends_on);
                }
                else {
                  value_edit = new QComboBox(depends_on);
                  parseJson(json_data, qobject_cast<QComboBox*>(value_edit));
                }
              }
              if (value_edit) {
                values.insert(name, value_edit);
                value_edit->setEnabled(new_checkbox->isChecked());
                value_edit->setProperty("checkbox", QVariant::fromValue(static_cast<void*>(new_checkbox)));
                connect(new_checkbox, &QCheckBox::toggled, value_edit, &QWidget::setEnabled);
                qobject_cast<QGridLayout*>(depends_on->layout())->addWidget(value_edit, row, column + 1);
                items_layouts.insert(name, depends_on->layout());
              }
            }
            qobject_cast<QGridLayout*>(depends_on->layout())->setProperty("row", ++row);
          }
          else {
            int row = qobject_cast<QGridLayout*>(current_tab->layout())->property("row").toInt();
            int column = qobject_cast<QGridLayout*>(current_tab->layout())->property("column").toInt();
            if (row > MAX_ROWS) {
              qobject_cast<QGridLayout*>(current_tab->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), row, column);
              row = 0;
              column += 2;
              qobject_cast<QGridLayout*>(current_tab->layout())->setProperty("column", column);
            }
            qobject_cast<QGridLayout*>(current_tab->layout())->addWidget(new_checkbox, row, column);
            QString item_value = value;
            if (!item_value.isEmpty()) {
              QWidget *value_edit = nullptr;
              if (item_value.toLower() == "true" || item_value.toLower() == "false") {
                value_edit = new QCheckBox(current_tab);
                qobject_cast<QCheckBox*>(value_edit)->setChecked(item_value.toLower() == "true");
              }
              else {
                if (json_data.isEmpty()) {
                  value_edit = new QLineEdit(item_value, depends_on);
                }
                else {
                  value_edit = new QComboBox(depends_on);
                  parseJson(json_data, qobject_cast<QComboBox*>(value_edit));
                }
              }
              if (value_edit) {
                values.insert(name, value_edit);
                value_edit->setEnabled(new_checkbox->isChecked());
                value_edit->setProperty("checkbox", QVariant::fromValue(static_cast<void*>(new_checkbox)));
                connect(new_checkbox, &QCheckBox::toggled, value_edit, &QWidget::setEnabled);
                qobject_cast<QGridLayout*>(current_tab->layout())->addWidget(value_edit, row, column + 1);
                items_layouts.insert(name, current_tab->layout());
              }
            }
            qobject_cast<QGridLayout*>(current_tab->layout())->setProperty("row", ++row);
          }
          new_checkbox->setToolTip(comment);
          if (!comment.isEmpty()) {
            new_checkbox->setIcon(QIcon("/home/alexander/icons/oxygen/base/32x32/status/dialog-information.png"));
          }
        }
//        qDebug() << tabs.toStdString().data() << match.captured("disabled") << name << value << comment << comment_pre; //.remove(QRegularExpression("[/\\n]"))
    }
  }


  QMapIterator<QString, QWidget*> sections_iterator{section_tabs};
  while (sections_iterator.hasNext()) {
    sections_iterator.next();
    QWidget *section = sections_iterator.value();
    QList<QGroupBox*> groups = section->findChildren<QGroupBox*>();
    int column = qobject_cast<QGridLayout*>(section->layout())->property("column").toInt();
    QWidget *container_widget = new QWidget(section);
    container_widget->setLayout(new QHBoxLayout(container_widget));
    container_widget->layout()->setMargin(0);
    qobject_cast<QGridLayout*>(section->layout())->addWidget(container_widget, 0, column + 2, MAX_ROWS, 2);
    QWidget *container_groups_widget = new QWidget(container_widget);
    container_groups_widget->setLayout(new QVBoxLayout(container_groups_widget));
    container_groups_widget->layout()->setMargin(0);
    qobject_cast<QHBoxLayout*>(container_widget->layout())->addWidget(container_groups_widget);
    int temp_height = 0;
    foreach (QGroupBox *group, groups) {
//      if (row > MAX_ROWS) {
//        qobject_cast<QGridLayout*>(section->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), row, column);
//        row = 0;
//        column += 2;
//        qobject_cast<QGridLayout*>(section->layout())->setProperty("column", column);
//      }
      group->adjustSize();
      temp_height += group->height();
//      qDebug() << group->title() << group->size();
      if (temp_height > 512) {
        qobject_cast<QVBoxLayout*>(container_groups_widget->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
        container_groups_widget = new QWidget(container_widget);
        container_groups_widget->setLayout(new QVBoxLayout(container_groups_widget));
        container_groups_widget->layout()->setMargin(0);
        qobject_cast<QHBoxLayout*>(container_widget->layout())->addWidget(container_groups_widget);
        temp_height = group->height();

      }
      qobject_cast<QGridLayout*>(section->layout())->removeWidget(group);
      qobject_cast<QVBoxLayout*>(container_groups_widget->layout())->addWidget(group);

//      qobject_cast<QGridLayout*>(section->layout())->addWidget(group, ++row, column);
    }
    qobject_cast<QVBoxLayout*>(container_groups_widget->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    QList<QCheckBox*> checks = section->findChildren<QCheckBox*>();
    int row = row = qobject_cast<QGridLayout*>(section->layout())->property("row").toInt();
    column = qobject_cast<QGridLayout*>(section->layout())->property("column").toInt();
    QPoint place{-1, -1};
    QList<QPoint> places;
    for (int c = 0; c < column + 2; c += 2) {
      for (int r = 0; r <= MAX_ROWS; ++r) {
        QLayoutItem *item = qobject_cast<QGridLayout*>(section->layout())->itemAtPosition(r, c);
        if (!item) {
//          qDebug() << "No item at" << r << c;
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
//            qDebug() << "Moving" << qobject_cast<QCheckBox*>(tem_widget)->text() << place.y() << place.x();
            places.append(QPoint(c, r));
          }
        }
        else {
          places.append(QPoint(c, r));
        }
      }
    }
//    foreach (QCheckBox *check, checks) {
//      if (row > MAX_ROWS) {
//        row = 0;
//        column += 2;
//      }
//      QWidget *tem_widget = qobject_cast<QGridLayout*>(section->layout())->itemAtPosition(row, column)->widget();
//      ++row;
//    }
    qobject_cast<QGridLayout*>(section->layout())->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, column + 4);
  }
  adjustSize();
}

void MainWindow::saveConfigurationFile(QString &directory) {
  QMapIterator<QString, QWidget*> elements_iterator{elements};
  while (elements_iterator.hasNext()) {
    elements_iterator.next();
    if (values.contains(elements_iterator.key())) {
      QString value;
      if (qobject_cast<QLineEdit*>(values.value(elements_iterator.key()))) {
        value = qobject_cast<QLineEdit*>(values.value(elements_iterator.key()))->text();
      }
      else if (qobject_cast<QComboBox*>(values.value(elements_iterator.key()))) {
        if (qobject_cast<QComboBox*>(values.value(elements_iterator.key()))->currentData().toString().isEmpty()) {
          value = qobject_cast<QComboBox*>(values.value(elements_iterator.key()))->currentText();
        }
        else {
          value = qobject_cast<QComboBox*>(values.value(elements_iterator.key()))->currentData().toString();
        }
      }
      else if (qobject_cast<QCheckBox*>(values.value(elements_iterator.key()))) {
        value = qobject_cast<QCheckBox*>(values.value(elements_iterator.key()))->isChecked() ? "true" : "false";
      }

      QString enabled = qobject_cast<QCheckBox*>(elements_iterator.value())->isChecked() ? "" : "//";
      QRegularExpression value_expression{QString("(?<spaces>[\\s\\t]*)?(?<enabled>/{2,})?[\\s\\t]*#define[\\s\\t]*%1[\\s\\t]*%2").arg(elements_iterator.key()).arg(QRegularExpression::escape(original_values.value(elements_iterator.key())))};

      if (original_file_data.indexOf(value_expression) != original_file_data.lastIndexOf(value_expression)) {
        QRegularExpressionMatchIterator value_match_iterator = value_expression.globalMatch(original_file_data);

      }
      else {
        QRegularExpressionMatch match = value_expression.match(original_file_data);
        if (match.hasMatch()) {
          QString match_enabled = match.captured("enabled");
          if (value == original_values.value(elements_iterator.key()) && match_enabled == enabled) {
            continue;
          }

          if (!value.isEmpty()) {
            QString match_spaces = match.captured("spaces");
            original_file_data = original_file_data.replace(value_expression, match_spaces + enabled + "#define " + elements_iterator.key() + " " + value);
            qDebug() << value_expression.pattern() << "\n" << value << "=="
                     << original_values.value(elements_iterator.key())
                     << (value == original_values.value(elements_iterator.key()))
                     << match_enabled << "==" << enabled << (match_enabled == enabled);
          }
        }
      }
    }
  }
  QFile out_file{QDir::homePath() + "/Configuration.h"};
  if (!out_file.open(QFile::WriteOnly)) {
    return;
  }
  out_file.write(original_file_data.toStdString().data());
  out_file.flush();
  out_file.close();
}

void MainWindow::parseJson(QString &json_string_parameter, QComboBox *combo_bos) {
  QString json_string = json_string_parameter.replace('\'', '"');
  if (json_string[0] != '[') {
    json_string = "[" + json_string + "]";
  }
  QRegularExpression incorrect_key{"[\\s\\t]*(?<comma>[,])?[\\s\\t]*(?<key>[^\\[\\{\"\\:]+):"};
  QRegularExpressionMatchIterator incorrect_key_iterator = incorrect_key.globalMatch(json_string);
  while (incorrect_key_iterator.hasNext()) {
    QRegularExpressionMatch match = incorrect_key_iterator.next();
    QRegularExpression current_incorrect_key{QString("[,]?[\\s\\t]*%1[\\s\\t]*:").arg(match.captured("key"))};
    json_string = json_string.replace(current_incorrect_key, match.captured("comma") + "\"" + match.captured("key").trimmed() + "\":");
  }
  QRegularExpression fix_expression{"\":[\\s\\t]*\"(?<text>.*?)\",[\\s\\t]*\""};
  QRegularExpressionMatchIterator fix_expression_iterator = fix_expression.globalMatch(json_string);
  while (fix_expression_iterator.hasNext()) {
    QRegularExpressionMatch match = fix_expression_iterator.next();
    QString temp_vcalue = match.captured("text");
    temp_vcalue = temp_vcalue.replace("\\", "\\\\");
    temp_vcalue = temp_vcalue.replace("\"", "\\\"");
    json_string = json_string.replace(match.captured("text"), temp_vcalue);
  }
  QJsonDocument jsonResponse = QJsonDocument::fromJson(json_string.toUtf8());
  QJsonArray jsonArray = jsonResponse.array();

  foreach (const QJsonValue & value, jsonArray) {
    if (value.isObject()) {
      QJsonObject obj = value.toObject();
      foreach (const QString key, obj.keys()) {
        combo_bos->addItem(key + " - " + obj[key].toVariant().toString(), key);
      }
    }
    else {
      combo_bos->addItem(value.toVariant().toString(), value.toVariant().toString());
    }
  }
}
