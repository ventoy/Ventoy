#! /usr/bin/env python3

# -*- coding: utf-8 -*-
# made by: panickingkernel


from PyQt5 import QtCore, QtGui, QtWidgets

import subprocess
from QtVentoyInstall import Ui_Dialog as Ui_Dialog


class Ui_MainWindow(object):
    def __init__(self):
        self.usb_path = ""
        self.dialogs = list()

    def setupUi(self, MainWindow):
        MainWindow.setObjectName("MainWindow")
        MainWindow.resize(664, 701)
        self.centralwidget = QtWidgets.QWidget(MainWindow)
        self.centralwidget.setObjectName("centralwidget")
        self.gridLayout_2 = QtWidgets.QGridLayout(self.centralwidget)
        self.gridLayout_2.setObjectName("gridLayout_2")
        self.gridLayout = QtWidgets.QGridLayout()
        self.gridLayout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
        self.gridLayout.setObjectName("gridLayout")
        self.workLayout = QtWidgets.QVBoxLayout()
        self.workLayout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
        self.workLayout.setObjectName("workLayout")
        self.versionLabel = QtWidgets.QLabel(self.centralwidget)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.versionLabel.sizePolicy().hasHeightForWidth())
        self.versionLabel.setSizePolicy(sizePolicy)
        self.versionLabel.setLayoutDirection(QtCore.Qt.LeftToRight)
        self.versionLabel.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignTop | QtCore.Qt.AlignTrailing)
        self.versionLabel.setObjectName("versionLabel")
        self.workLayout.addWidget(self.versionLabel)
        self.BtnLayout = QtWidgets.QHBoxLayout()
        self.BtnLayout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
        self.BtnLayout.setObjectName("BtnLayout")
        self.usbLayout_big = QtWidgets.QVBoxLayout()
        self.usbLayout_big.setObjectName("usbLayout_big")
        self.usbLayout = QtWidgets.QVBoxLayout()
        self.usbLayout.setSizeConstraint(QtWidgets.QLayout.SetMinimumSize)
        self.usbLayout.setSpacing(0)
        self.usbLayout.setObjectName("usbLayout")
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Minimum)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        spacerItem = QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum,
                                           QtWidgets.QSizePolicy.MinimumExpanding)
        self.usbLayout.addItem(spacerItem)
        self.refreshLayout = QtWidgets.QVBoxLayout()
        self.refreshLayout.setObjectName("refreshLayout")
        self.refreshBtn = QtWidgets.QPushButton(self.centralwidget)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.refreshBtn.sizePolicy().hasHeightForWidth())
        self.refreshBtn.setSizePolicy(sizePolicy)
        self.refreshBtn.setContextMenuPolicy(QtCore.Qt.NoContextMenu)
        self.refreshBtn.setLayoutDirection(QtCore.Qt.LeftToRight)
        self.refreshBtn.setCheckable(False)
        self.refreshBtn.setObjectName("refreshBtn")
        self.refreshLayout.addWidget(self.refreshBtn)
        self.usbLayout_big.addLayout(self.usbLayout)
        self.usbLayout_big.addLayout(self.refreshLayout)
        self.BtnLayout.addLayout(self.usbLayout_big)
        self.installBtn = QtWidgets.QPushButton(self.centralwidget)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.installBtn.sizePolicy().hasHeightForWidth())
        self.installBtn.setSizePolicy(sizePolicy)
        self.installBtn.setContextMenuPolicy(QtCore.Qt.DefaultContextMenu)
        self.installBtn.setAcceptDrops(False)
        self.installBtn.setObjectName("installBtn")
        self.BtnLayout.addWidget(self.installBtn, 0, QtCore.Qt.AlignBottom)
        self.forceinstallBtn = QtWidgets.QPushButton(self.centralwidget)
        self.forceinstallBtn.setFlat(False)
        self.forceinstallBtn.setObjectName("forceinstallBtn")
        self.BtnLayout.addWidget(self.forceinstallBtn, 0, QtCore.Qt.AlignBottom)
        self.updateBtn = QtWidgets.QPushButton(self.centralwidget)
        self.updateBtn.setObjectName("updateBtn")
        self.BtnLayout.addWidget(self.updateBtn, 0, QtCore.Qt.AlignBottom)
        self.workLayout.addLayout(self.BtnLayout)
        self.gridLayout.addLayout(self.workLayout, 0, 1, 1, 1)
        self.gridLayout_2.addLayout(self.gridLayout, 0, 0, 1, 1)
        MainWindow.setCentralWidget(self.centralwidget)
        self.menubar = QtWidgets.QMenuBar(MainWindow)
        self.menubar.setGeometry(QtCore.QRect(0, 0, 664, 26))
        self.menubar.setObjectName("menubar")
        MainWindow.setMenuBar(self.menubar)
        self.statusbar = QtWidgets.QStatusBar(MainWindow)
        self.statusbar.setObjectName("statusbar")
        MainWindow.setStatusBar(self.statusbar)
        self.usb_btns()
        self.retranslateUi(MainWindow)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)

    def install_dialog(self, title, inst_type):
        _translate = QtCore.QCoreApplication.translate
        self.Dialog = QtWidgets.QDialog()
        self.ui = Ui_Dialog(self.usb_path, inst_type)
        self.ui.setupUi(self.Dialog)
        self.Dialog.setWindowTitle(_translate("Dialog", title))
        self.Dialog.show()


    def retranslateUi(self, MainWindow):
        _translate = QtCore.QCoreApplication.translate
        MainWindow.setWindowTitle(_translate("MainWindow", "MainWindow"))
        self.versionLabel.setText(_translate("MainWindow", "Ventoy Version: VER"))
        self.refreshBtn.setText(_translate("MainWindow", "Refresh"))
        self.installBtn.setText(_translate("MainWindow", "Install"))
        self.forceinstallBtn.setText(_translate("MainWindow", "Force Install"))
        self.updateBtn.setText(_translate("MainWindow", "Update"))
        self.installBtn.clicked.connect(lambda: self.install_dialog("Install", "-i"))
        self.forceinstallBtn.clicked.connect(lambda: self.install_dialog("Force Install", "-I"))
        self.updateBtn.clicked.connect(lambda: self.install_dialog("Force Install", "-u"))
        self.refreshBtn.clicked.connect(lambda: self.usb_btns())
        # set version
        self.label_text = self.versionLabel.text().replace("VER", f"{subprocess.getoutput('cat ./ventoy/version')}")
        self.versionLabel.setText(self.label_text)

    def usb_btns(self):
        #delete all buttons in usbLayout
        self.delete_this = self.usbLayout
        for i in reversed(range(self.delete_this.count())):
            try:
                self.widgetToRemove = self.delete_this.itemAt(i).widget()

                # remove it from the layout list
                self.delete_this.removeWidget(self.widgetToRemove)
                # remove it from the gui
                self.widgetToRemove.setParent(None)
            except:
                pass
        #define variables
        self.usb_path = ""
        self.get_usbs_count = len(subprocess.getoutput("lsblk -l -o tran| grep 'usb'").split("\n"))
        self.get_usbs_path = subprocess.getoutput("lsblk -l -o tran,path| grep 'usb'")
        self.get_usbs_path = self.get_usbs_path.replace("usb", "").replace(" ", "").split("\n")
        self.get_usbs_name = subprocess.getoutput("lsblk -l -o tran,model| grep 'usb'")
        self.get_usbs_name = self.get_usbs_name.replace("usb", "").replace(" ", "").split("\n")

        def set_usb_path(path):
            self.usb_path = self.get_usbs_path[path]

        #put buttons back in
        for button in range(self.get_usbs_count):
            if button == 0:
                self.usb_path = self.get_usbs_path[button]
            self.btn = QtWidgets.QPushButton()
            # self.btn.setObjectName(f"Button {button + 1}")
            self.btn.setText(f"Usb {button + 1}:{self.get_usbs_name[button]}({self.get_usbs_path[button]})")
            self.btn.clicked.connect(lambda location_set, number=button: set_usb_path(number))
            self.usbLayout.addWidget(self.btn)
            sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Minimum)
            sizePolicy.setHorizontalStretch(0)
            sizePolicy.setVerticalStretch(0)
        UsbSpacer = QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.MinimumExpanding)
        self.usbLayout.addItem(UsbSpacer)



if __name__ == "__main__":
    import sys

    app = QtWidgets.QApplication(sys.argv)
    MainWindow = QtWidgets.QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow)
    MainWindow.show()
    sys.exit(app.exec_())
