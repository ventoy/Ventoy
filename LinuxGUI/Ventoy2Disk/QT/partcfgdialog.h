#ifndef PARTCFGDIALOG_H
#define PARTCFGDIALOG_H

#include <QDialog>
#include <QJsonObject>

namespace Ui {
class PartCfgDialog;
}

class PartCfgDialog : public QDialog
{
    Q_OBJECT

public:

    bool reserve;
    int unit;
    bool align;
    QString valuestr;
    qint64 resvalue;

    QString invalid_value;
    QString err_title;
    void update_ui_status();
    void update_language_ui(QJsonObject &obj);

    explicit PartCfgDialog(QWidget *parent = nullptr);
    ~PartCfgDialog();

private slots:
    void on_pushButtonOK_clicked();

    void on_pushButtonCancel_clicked();

    void on_checkBox_stateChanged(int arg1);

private:
    Ui::PartCfgDialog *ui;
};

#define _LANG_STR(id) obj.value(id).toString()

#endif // PARTCFGDIALOG_H
