#! /usr/bin/env python3

# -*- coding: utf-8 -*-
# made by: panickingkernel


from PyQt5 import QtCore, QtGui, QtWidgets
import subprocess
from subprocess import Popen, PIPE


class Ui_Dialog(object):
    def __init__(self, path, inst):
        self.usb_path = path
        self.installation_type = inst + " "

    def setupUi(self, Dialog):
        Dialog.setObjectName("Dialog")
        Dialog.resize(523, 142)
        self.gridLayout = QtWidgets.QGridLayout(Dialog)
        self.gridLayout.setObjectName("gridLayout")
        self.instLayout = QtWidgets.QVBoxLayout()
        self.instLayout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
        self.instLayout.setSpacing(0)
        self.instLayout.setObjectName("instLayout")
        self.GptCBox = QtWidgets.QCheckBox(Dialog)
        self.GptCBox.setObjectName("GptCBox")
        self.instLayout.addWidget(self.GptCBox)
        self.SecureCBox = QtWidgets.QCheckBox(Dialog)
        self.SecureCBox.setObjectName("SecureCBox")
        self.instLayout.addWidget(self.SecureCBox)
        self.PrvLayout = QtWidgets.QHBoxLayout()
        self.PrvLayout.setSizeConstraint(QtWidgets.QLayout.SetMinimumSize)
        self.PrvLayout.setObjectName("PrvLayout")
        self.PrvCBox = QtWidgets.QCheckBox(Dialog)
        self.PrvCBox.setObjectName("PrvCBox")
        self.PrvLayout.addWidget(self.PrvCBox)
        self.PrvBox = QtWidgets.QSpinBox(Dialog)
        self.PrvBox.setMaximum(99999999)
        self.PrvBox.setObjectName("PrvBox")
        self.PrvLayout.addWidget(self.PrvBox)
        spacerItem = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.PrvLayout.addItem(spacerItem)
        self.instLayout.addLayout(self.PrvLayout)
        spacerItem1 = QtWidgets.QSpacerItem(20, 60, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding)
        self.instLayout.addItem(spacerItem1)
        self.PrvBBox = QtWidgets.QDialogButtonBox(Dialog)
        sizePolicy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.PrvBBox.sizePolicy().hasHeightForWidth())
        self.PrvBBox.setSizePolicy(sizePolicy)
        self.PrvBBox.setOrientation(QtCore.Qt.Horizontal)
        self.PrvBBox.setStandardButtons(QtWidgets.QDialogButtonBox.Abort | QtWidgets.QDialogButtonBox.Yes)
        self.PrvBBox.setObjectName("PrvBBox")
        self.instLayout.addWidget(self.PrvBBox)
        self.gridLayout.addLayout(self.instLayout, 0, 0, 1, 1)

        self.retranslateUi(Dialog)
        self.PrvBBox.accepted.connect(Dialog.accept)
        self.PrvBBox.rejected.connect(Dialog.reject)
        QtCore.QMetaObject.connectSlotsByName(Dialog)

    def retranslateUi(self, Dialog):
        _translate = QtCore.QCoreApplication.translate
        Dialog.setWindowTitle(_translate("Dialog", "Dialog"))
        self.GptCBox.setText(_translate("Dialog", "Use GPT"))
        self.SecureCBox.setText(_translate("Dialog", "Secure Boot"))
        self.PrvCBox.setText(_translate("Dialog", "Preserve space(mb)"))
        self.PrvBBox.accepted.connect(lambda: self.install())

    def install(self):
        self.install_prms = []
        if self.SecureCBox.isChecked():
            self.install_prms.append(" -s")
        if self.GptCBox.isChecked():
            self.install_prms.append(" -g")
        if self.PrvCBox.isChecked():
            self.space_to_preserve = self.PrvBox.text()
            self.install_prms.append(f" -r {self.space_to_preserve}")
        self.install_command = str("bash Ventoy2Disk.sh " + self.installation_type + self.usb_path + "".join(
            self.install_prms)).split(" ")
        installation = subprocess.Popen(self.install_command, stdin=PIPE, stdout=PIPE)
        installation.stdin.write(b"y\n")
        if self.installation_type == "-I":
            installation.stdin.write(b"y\n")
        outputlog, errorlog = installation.communicate(b"y")
        installation.stdin.close()


if __name__ == "__main__":
    import sys

    app = QtWidgets.QApplication(sys.argv)
    Dialog = QtWidgets.QDialog()
    ui = Ui_Dialog()
    ui.setupUi(Dialog)
    Dialog.show()
    sys.exit(app.exec_())
