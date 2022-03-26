#include "partcfgdialog.h"
#include "ui_partcfgdialog.h"
#include <QDebug>
#include <QMessageBox>

PartCfgDialog::PartCfgDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PartCfgDialog)
{
    reserve = false;
    unit = 1;
    align = true;
    valuestr = "";
    resvalue = 0;

    ui->setupUi(this);

    ui->lineEdit->setEnabled(false);
    ui->comboBox->setEnabled(false);
}

PartCfgDialog::~PartCfgDialog()
{
    delete ui;
}

void PartCfgDialog::update_ui_status()
{
    ui->checkBox->setChecked(reserve);
    ui->lineEdit->setEnabled(reserve);
    ui->comboBox->setEnabled(reserve);
    ui->checkBox_2->setChecked(align);
}

void PartCfgDialog::update_language_ui(QJsonObject &obj)
{
    ui->checkBox->setText(_LANG_STR("STR_PRESERVE_SPACE"));
    ui->checkBox_2->setText(_LANG_STR("STR_PART_ALIGN_4KB"));
    ui->pushButtonOK->setText(_LANG_STR("STR_BTN_OK"));
    ui->pushButtonCancel->setText(_LANG_STR("STR_BTN_CANCEL"));

    invalid_value = _LANG_STR("STR_SPACE_VAL_INVALID");
    err_title = _LANG_STR("STR_ERROR");
}

void PartCfgDialog::on_pushButtonOK_clicked()
{
    if (ui->checkBox->isChecked())
    {
        QString str = ui->lineEdit->text();

        if (str.isEmpty())
        {
            QMessageBox::critical(NULL, err_title, invalid_value);
            return;
        }

        for (int i = 0; i < str.size(); i++)
        {
            if (str[i] < '0' || str[i] > '9')
            {
                QMessageBox::critical(NULL, err_title, invalid_value);
                return;
            }
        }

        valuestr = str;
        resvalue = str.toLongLong();
        reserve = true;
    }
    else
    {
        reserve = false;
    }

    align = ui->checkBox_2->isChecked();
    unit = ui->comboBox->currentIndex();

    accept();
}

void PartCfgDialog::on_pushButtonCancel_clicked()
{
    reject();
}

void PartCfgDialog::on_checkBox_stateChanged(int arg1)
{
    (void)arg1;

    if (ui->checkBox->isChecked())
    {
        ui->lineEdit->setEnabled(true);
        ui->comboBox->setEnabled(true);
    }
    else
    {
        ui->lineEdit->setEnabled(false);
        ui->comboBox->setEnabled(false);
    }
}
