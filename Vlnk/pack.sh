#!/bin/sh

rm -f ../INSTALL/VentoyVlnk.sh
cp -a ./VentoyVlnk.sh ../INSTALL/VentoyVlnk.sh

rm -f ../INSTALL/VentoyVlnk.exe
cp -a ./vs/VentoyVlnk/Release/VentoyVlnk.exe   ../INSTALL/VentoyVlnk.exe
