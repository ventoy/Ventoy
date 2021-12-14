/********************************************************************************
** Form generated from reading UI file 'partcfgdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PARTCFGDIALOG_H
#define UI_PARTCFGDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_PartCfgDialog
{
public:
    QPushButton *pushButtonOK;
    QPushButton *pushButtonCancel;
    QGroupBox *groupBox;
    QCheckBox *checkBox;
    QGroupBox *groupBox_2;
    QLineEdit *lineEdit;
    QGroupBox *groupBox_3;
    QComboBox *comboBox;
    QGroupBox *groupBox_4;
    QCheckBox *checkBox_2;

    void setupUi(QDialog *PartCfgDialog)
    {
        if (PartCfgDialog->objectName().isEmpty())
            PartCfgDialog->setObjectName(QStringLiteral("PartCfgDialog"));
        PartCfgDialog->resize(420, 258);
        pushButtonOK = new QPushButton(PartCfgDialog);
        pushButtonOK->setObjectName(QStringLiteral("pushButtonOK"));
        pushButtonOK->setGeometry(QRect(90, 210, 90, 30));
        pushButtonCancel = new QPushButton(PartCfgDialog);
        pushButtonCancel->setObjectName(QStringLiteral("pushButtonCancel"));
        pushButtonCancel->setGeometry(QRect(230, 210, 90, 30));
        groupBox = new QGroupBox(PartCfgDialog);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        groupBox->setGeometry(QRect(10, 0, 400, 61));
        checkBox = new QCheckBox(groupBox);
        checkBox->setObjectName(QStringLiteral("checkBox"));
        checkBox->setGeometry(QRect(10, 20, 381, 41));
        checkBox->setAutoRepeatInterval(0);
        groupBox_2 = new QGroupBox(PartCfgDialog);
        groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
        groupBox_2->setGeometry(QRect(10, 60, 200, 61));
        lineEdit = new QLineEdit(groupBox_2);
        lineEdit->setObjectName(QStringLiteral("lineEdit"));
        lineEdit->setGeometry(QRect(10, 30, 181, 26));
        lineEdit->setMaxLength(14);
        groupBox_3 = new QGroupBox(PartCfgDialog);
        groupBox_3->setObjectName(QStringLiteral("groupBox_3"));
        groupBox_3->setGeometry(QRect(210, 60, 200, 60));
        comboBox = new QComboBox(groupBox_3);
        comboBox->setObjectName(QStringLiteral("comboBox"));
        comboBox->setGeometry(QRect(10, 30, 181, 26));
        groupBox_4 = new QGroupBox(PartCfgDialog);
        groupBox_4->setObjectName(QStringLiteral("groupBox_4"));
        groupBox_4->setGeometry(QRect(10, 120, 401, 61));
        checkBox_2 = new QCheckBox(groupBox_4);
        checkBox_2->setObjectName(QStringLiteral("checkBox_2"));
        checkBox_2->setGeometry(QRect(10, 30, 381, 24));
        checkBox_2->setChecked(true);

        retranslateUi(PartCfgDialog);

        comboBox->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(PartCfgDialog);
    } // setupUi

    void retranslateUi(QDialog *PartCfgDialog)
    {
        PartCfgDialog->setWindowTitle(QApplication::translate("PartCfgDialog", "Partition Configuration", Q_NULLPTR));
        pushButtonOK->setText(QApplication::translate("PartCfgDialog", "OK", Q_NULLPTR));
        pushButtonCancel->setText(QApplication::translate("PartCfgDialog", "Cancel", Q_NULLPTR));
        groupBox->setTitle(QString());
        checkBox->setText(QApplication::translate("PartCfgDialog", "Preserve some space at the end of the disk", Q_NULLPTR));
        groupBox_2->setTitle(QString());
        groupBox_3->setTitle(QString());
        comboBox->clear();
        comboBox->insertItems(0, QStringList()
         << QApplication::translate("PartCfgDialog", "MB", Q_NULLPTR)
         << QApplication::translate("PartCfgDialog", "GB", Q_NULLPTR)
        );
        groupBox_4->setTitle(QString());
        checkBox_2->setText(QApplication::translate("PartCfgDialog", "Align partitions with 4KB", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class PartCfgDialog: public Ui_PartCfgDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PARTCFGDIALOG_H
