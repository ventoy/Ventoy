#!/bin/bash

FTPIP=192.168.44.1
FTPUSR='a:a'

rm -f dmpatch.c Makefile Makefile_IBT

for f in dmpatch.c Makefile Makefile_IBT; do
	curl -s -u $FTPUSR ftp://$FTPIP/$f -o $f
	if [ -f $f ]; then
		echo "download $f OK ..."
	else
		echo "download $f FAILED ..."
		exit 1
	fi
done



rm -f *.ko


echo "build dm_patch.ko ..."
rm -rf ./aa
mkdir ./aa

cp -a *.c aa/
cp -a Makefile aa/

cd /home/panda/linux-source-5.15.0
make modules M=/home/panda/build/aa/
strip --strip-debug /home/panda/build/aa/dm_patch.ko
cd -

cp -a aa/dm_patch.ko  ./



echo "build dm_patch_ibt.ko ..."
rm -rf ./aa
mkdir ./aa

cp -a *.c aa/
cp -a Makefile_IBT aa/Makefile

cd /home/panda/linux-source-5.15.0
make modules M=/home/panda/build/aa/
strip --strip-debug /home/panda/build/aa/dm_patch_ibt.ko
cd -

cp -a aa/dm_patch_ibt.ko ./

rm -rf ./aa


curl -s -T dm_patch.ko -u $FTPUSR ftp://$FTPIP/dm_patch_64.ko || exit 1
curl -s -T dm_patch_ibt.ko -u $FTPUSR ftp://$FTPIP/dm_patch_ibt_64.ko || exit 1


if [ -f ./dm_patch.ko -a -f ./dm_patch_ibt.ko ]; then
	echo -e "\n\n=============== SUCCESS =============\n\n"
else
	echo -e "\n\n=============== FAILED ==============\n\n"
fi

