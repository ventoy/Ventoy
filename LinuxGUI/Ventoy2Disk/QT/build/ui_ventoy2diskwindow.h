/********************************************************************************
** Form generated from reading UI file 'ventoy2diskwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VENTOY2DISKWINDOW_H
#define UI_VENTOY2DISKWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Ventoy2DiskWindow
{
public:
    QAction *actionSecure_Boot_Support;
    QAction *actionMBR;
    QAction *actionGPT;
    QAction *actionPartition_Configuration;
    QAction *actionClear_Ventoy;
    QAction *actionShow_All_Devices;
    QWidget *centralwidget;
    QGroupBox *groupBoxDevice;
    QComboBox *comboBoxDevice;
    QPushButton *ButtonRefresh;
    QGroupBox *groupBoxVentoyLocal;
    QLabel *labelVentoyLocalVer;
    QLabel *labelVentoyLocalPartStyle;
    QLabel *labelVentoyLocalSecure;
    QGroupBox *groupBoxVentoyDevice;
    QLabel *labelVentoyDeviceVer;
    QLabel *labelVentoyDevicePartStyle;
    QLabel *labelVentoyDeviceSecure;
    QGroupBox *groupBoxStatus;
    QProgressBar *progressBar;
    QPushButton *ButtonInstall;
    QPushButton *ButtonUpdate;
    QMenuBar *menubar;
    QMenu *menuOption;
    QMenu *menuPartition_Style;
    QMenu *menuLanguage;

    void setupUi(QMainWindow *Ventoy2DiskWindow)
    {
        if (Ventoy2DiskWindow->objectName().isEmpty())
            Ventoy2DiskWindow->setObjectName(QStringLiteral("Ventoy2DiskWindow"));
        Ventoy2DiskWindow->resize(441, 367);
        actionSecure_Boot_Support = new QAction(Ventoy2DiskWindow);
        actionSecure_Boot_Support->setObjectName(QStringLiteral("actionSecure_Boot_Support"));
        actionSecure_Boot_Support->setCheckable(true);
        actionMBR = new QAction(Ventoy2DiskWindow);
        actionMBR->setObjectName(QStringLiteral("actionMBR"));
        actionMBR->setCheckable(true);
        actionGPT = new QAction(Ventoy2DiskWindow);
        actionGPT->setObjectName(QStringLiteral("actionGPT"));
        actionGPT->setCheckable(true);
        actionPartition_Configuration = new QAction(Ventoy2DiskWindow);
        actionPartition_Configuration->setObjectName(QStringLiteral("actionPartition_Configuration"));
        actionClear_Ventoy = new QAction(Ventoy2DiskWindow);
        actionClear_Ventoy->setObjectName(QStringLiteral("actionClear_Ventoy"));
        actionShow_All_Devices = new QAction(Ventoy2DiskWindow);
        actionShow_All_Devices->setObjectName(QStringLiteral("actionShow_All_Devices"));
        actionShow_All_Devices->setCheckable(true);
        centralwidget = new QWidget(Ventoy2DiskWindow);
        centralwidget->setObjectName(QStringLiteral("centralwidget"));
        groupBoxDevice = new QGroupBox(centralwidget);
        groupBoxDevice->setObjectName(QStringLiteral("groupBoxDevice"));
        groupBoxDevice->setGeometry(QRect(10, 10, 421, 80));
        comboBoxDevice = new QComboBox(groupBoxDevice);
        comboBoxDevice->setObjectName(QStringLiteral("comboBoxDevice"));
        comboBoxDevice->setGeometry(QRect(10, 40, 361, 26));
        ButtonRefresh = new QPushButton(groupBoxDevice);
        ButtonRefresh->setObjectName(QStringLiteral("ButtonRefresh"));
        ButtonRefresh->setGeometry(QRect(380, 37, 30, 30));
        QIcon icon;
        icon.addFile(QStringLiteral("../refresh.ico"), QSize(), QIcon::Normal, QIcon::Off);
        ButtonRefresh->setIcon(icon);
        ButtonRefresh->setIconSize(QSize(24, 24));
        groupBoxVentoyLocal = new QGroupBox(centralwidget);
        groupBoxVentoyLocal->setObjectName(QStringLiteral("groupBoxVentoyLocal"));
        groupBoxVentoyLocal->setGeometry(QRect(10, 100, 205, 80));
        groupBoxVentoyLocal->setAlignment(Qt::AlignCenter);
        labelVentoyLocalVer = new QLabel(groupBoxVentoyLocal);
        labelVentoyLocalVer->setObjectName(QStringLiteral("labelVentoyLocalVer"));
        labelVentoyLocalVer->setGeometry(QRect(30, 30, 135, 41));
        labelVentoyLocalVer->setAlignment(Qt::AlignCenter);
        labelVentoyLocalPartStyle = new QLabel(groupBoxVentoyLocal);
        labelVentoyLocalPartStyle->setObjectName(QStringLiteral("labelVentoyLocalPartStyle"));
        labelVentoyLocalPartStyle->setGeometry(QRect(172, 60, 31, 18));
        labelVentoyLocalSecure = new QLabel(groupBoxVentoyLocal);
        labelVentoyLocalSecure->setObjectName(QStringLiteral("labelVentoyLocalSecure"));
        labelVentoyLocalSecure->setGeometry(QRect(12, 36, 21, 31));
        labelVentoyLocalSecure->setPixmap(QPixmap(QString::fromUtf8("../secure.ico")));
        groupBoxVentoyDevice = new QGroupBox(centralwidget);
        groupBoxVentoyDevice->setObjectName(QStringLiteral("groupBoxVentoyDevice"));
        groupBoxVentoyDevice->setGeometry(QRect(225, 100, 205, 80));
        groupBoxVentoyDevice->setAlignment(Qt::AlignCenter);
        labelVentoyDeviceVer = new QLabel(groupBoxVentoyDevice);
        labelVentoyDeviceVer->setObjectName(QStringLiteral("labelVentoyDeviceVer"));
        labelVentoyDeviceVer->setGeometry(QRect(30, 30, 135, 41));
        labelVentoyDeviceVer->setAlignment(Qt::AlignCenter);
        labelVentoyDevicePartStyle = new QLabel(groupBoxVentoyDevice);
        labelVentoyDevicePartStyle->setObjectName(QStringLiteral("labelVentoyDevicePartStyle"));
        labelVentoyDevicePartStyle->setGeometry(QRect(172, 60, 31, 18));
        labelVentoyDeviceSecure = new QLabel(groupBoxVentoyDevice);
        labelVentoyDeviceSecure->setObjectName(QStringLiteral("labelVentoyDeviceSecure"));
        labelVentoyDeviceSecure->setGeometry(QRect(12, 36, 21, 31));
        labelVentoyDeviceSecure->setPixmap(QPixmap(QString::fromUtf8("../secure.ico")));
        groupBoxStatus = new QGroupBox(centralwidget);
        groupBoxStatus->setObjectName(QStringLiteral("groupBoxStatus"));
        groupBoxStatus->setGeometry(QRect(10, 190, 421, 61));
        progressBar = new QProgressBar(groupBoxStatus);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setGeometry(QRect(10, 30, 401, 23));
        progressBar->setValue(0);
        progressBar->setTextVisible(false);
        ButtonInstall = new QPushButton(centralwidget);
        ButtonInstall->setObjectName(QStringLiteral("ButtonInstall"));
        ButtonInstall->setGeometry(QRect(90, 275, 101, 41));
        ButtonUpdate = new QPushButton(centralwidget);
        ButtonUpdate->setObjectName(QStringLiteral("ButtonUpdate"));
        ButtonUpdate->setGeometry(QRect(250, 275, 101, 41));
        Ventoy2DiskWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(Ventoy2DiskWindow);
        menubar->setObjectName(QStringLiteral("menubar"));
        menubar->setGeometry(QRect(0, 0, 441, 23));
        menuOption = new QMenu(menubar);
        menuOption->setObjectName(QStringLiteral("menuOption"));
        menuPartition_Style = new QMenu(menuOption);
        menuPartition_Style->setObjectName(QStringLiteral("menuPartition_Style"));
        menuLanguage = new QMenu(menubar);
        menuLanguage->setObjectName(QStringLiteral("menuLanguage"));
        Ventoy2DiskWindow->setMenuBar(menubar);

        menubar->addAction(menuOption->menuAction());
        menubar->addAction(menuLanguage->menuAction());
        menuOption->addAction(actionSecure_Boot_Support);
        menuOption->addAction(menuPartition_Style->menuAction());
        menuOption->addAction(actionPartition_Configuration);
        menuOption->addAction(actionClear_Ventoy);
        menuOption->addAction(actionShow_All_Devices);
        menuPartition_Style->addAction(actionMBR);
        menuPartition_Style->addAction(actionGPT);

        retranslateUi(Ventoy2DiskWindow);

        QMetaObject::connectSlotsByName(Ventoy2DiskWindow);
    } // setupUi

    void retranslateUi(QMainWindow *Ventoy2DiskWindow)
    {
        Ventoy2DiskWindow->setWindowTitle(QApplication::translate("Ventoy2DiskWindow", "Ventoy2Disk", Q_NULLPTR));
        actionSecure_Boot_Support->setText(QApplication::translate("Ventoy2DiskWindow", "Secure Boot Support", Q_NULLPTR));
        actionMBR->setText(QApplication::translate("Ventoy2DiskWindow", "MBR", Q_NULLPTR));
        actionGPT->setText(QApplication::translate("Ventoy2DiskWindow", "GPT", Q_NULLPTR));
        actionPartition_Configuration->setText(QApplication::translate("Ventoy2DiskWindow", "Partition Configuration", Q_NULLPTR));
        actionClear_Ventoy->setText(QApplication::translate("Ventoy2DiskWindow", "Clear Ventoy", Q_NULLPTR));
        actionShow_All_Devices->setText(QApplication::translate("Ventoy2DiskWindow", "Show All Devices", Q_NULLPTR));
        groupBoxDevice->setTitle(QApplication::translate("Ventoy2DiskWindow", "Device", Q_NULLPTR));
        ButtonRefresh->setText(QString());
        groupBoxVentoyLocal->setTitle(QApplication::translate("Ventoy2DiskWindow", "Ventoy In Package", Q_NULLPTR));
        labelVentoyLocalVer->setText(QApplication::translate("Ventoy2DiskWindow", "<html><head/><body><p><span style=\" font-size:20pt; font-weight:600; color:#ff0000;\">1.0.53</span></p></body></html>", Q_NULLPTR));
        labelVentoyLocalPartStyle->setText(QApplication::translate("Ventoy2DiskWindow", "<html><head/><body><p>MBR</p></body></html>", Q_NULLPTR));
        labelVentoyLocalSecure->setText(QString());
        groupBoxVentoyDevice->setTitle(QApplication::translate("Ventoy2DiskWindow", "Ventoy In Device", Q_NULLPTR));
        labelVentoyDeviceVer->setText(QApplication::translate("Ventoy2DiskWindow", "<html><head/><body><p><span style=\" font-size:20pt; font-weight:600; color:#ff0000;\">1.0.52</span></p></body></html>", Q_NULLPTR));
        labelVentoyDevicePartStyle->setText(QApplication::translate("Ventoy2DiskWindow", "<html><head/><body><p>GPT</p></body></html>", Q_NULLPTR));
        labelVentoyDeviceSecure->setText(QString());
        groupBoxStatus->setTitle(QApplication::translate("Ventoy2DiskWindow", "Status: REDAY", Q_NULLPTR));
        progressBar->setFormat(QString());
        ButtonInstall->setText(QApplication::translate("Ventoy2DiskWindow", "Install", Q_NULLPTR));
        ButtonUpdate->setText(QApplication::translate("Ventoy2DiskWindow", "Update", Q_NULLPTR));
        menuOption->setTitle(QApplication::translate("Ventoy2DiskWindow", "Option", Q_NULLPTR));
        menuPartition_Style->setTitle(QApplication::translate("Ventoy2DiskWindow", "Partition Style", Q_NULLPTR));
        menuLanguage->setTitle(QApplication::translate("Ventoy2DiskWindow", "Language", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class Ventoy2DiskWindow: public Ui_Ventoy2DiskWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VENTOY2DISKWINDOW_H
