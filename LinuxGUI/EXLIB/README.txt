
====== ARM64 ====== 
1. Download CentOS-7-aarch64-Everything-2009.iso from internet.
2. mount CentOS-7-aarch64-Everything-2009.iso  /mnt
3. sh prepare_lib_aarch64.sh /mnt/Packages/


====== MIPS64EL ====== 
1. build dpkg for CentOS7
download dpkg_1.18.25.tar.xz from internet.
cd dpkg-1.18.25
./configure
make
cp -a ./src/dpkg /sbin/
cp -a ./dpkg-deb/dpkg-deb /sbin/

2. download debian 10.x mips64el DVD iso (e.g. debian-10.9.0-mips64el-DVD-1.iso) form internet
3. mount debian-10.9.0-mips64el-DVD-1.iso /mnt
4. sh prepare_gtk_lib_mips64el.sh /mnt/
5. download the following packages from internet and dpkg -x each of them
pool/main/g/gtk+3.0/libgtk-3-dev_3.24.5-1_mips64el.deb
pool/main/b/brotli/libbrotli1_1.0.9-2+b2_mips64el.deb
pool/main/d/double-conversion/libdouble-conversion3_3.1.5-6.1_mips64el.deb
pool/main/d/double-conversion/libdouble-conversion1_3.1.0-3_mips64el.deb
pool/main/libg/libglvnd/libgl1_1.3.2-1~bpo10+2_mips64el.deb
pool/main/libg/libglvnd/libglvnd0_1.3.2-1~bpo10+2_mips64el.deb
pool/main/libg/libglvnd/libglx0_1.3.2-1~bpo10+2_mips64el.deb
pool/main/q/qtbase-opensource-src/libqt5concurrent5_5.11.3+dfsg1-1+deb10u4_mips64el.deb
pool/main/q/qtbase-opensource-src/libqt5core5a_5.11.3+dfsg1-1+deb10u4_mips64el.deb
pool/main/q/qtbase-opensource-src/libqt5dbus5_5.11.3+dfsg1-1+deb10u4_mips64el.deb 
pool/main/q/qtbase-opensource-src/libqt5gui5_5.11.3+dfsg1-1+deb10u4_mips64el.deb 
pool/main/q/qtbase-opensource-src/libqt5network5_5.11.3+dfsg1-1+deb10u4_mips64el.deb
pool/main/q/qtbase-opensource-src/libqt5opengl5_5.11.3+dfsg1-1+deb10u4_mips64el.deb 
pool/main/q/qtbase-opensource-src/libqt5opengl5-dev_5.11.3+dfsg1-1+deb10u4_mips64el.deb
pool/main/q/qtbase-opensource-src/libqt5widgets5_5.11.3+dfsg1-1+deb10u4_mips64el.deb 
pool/main/q/qtbase-opensource-src/libqt5xml5_5.11.3+dfsg1-1+deb10u4_mips64el.deb  
pool/main/q/qtbase-opensource-src/qt5-qmake_5.11.3+dfsg1-1+deb10u4_mips64el.deb    
pool/main/q/qtbase-opensource-src/qtbase5-dev_5.11.3+dfsg1-1+deb10u4_mips64el.deb 
pool/main/q/qtbase-opensource-src/qtbase5-dev-tools_5.11.3+dfsg1-1+deb10u4_mips64el.deb
pool/main/q/qtbase-opensource-src/qtbase5-examples_5.11.3+dfsg1-1+deb10u4_mips64el.deb
pool/main/q/qtbase-opensource-src/qtbase5-private-dev_5.11.3+dfsg1-1+deb10u4_mips64el.deb


==== I386 ===
We need a CentOS8 environment
Install CentOS 8 x86_64 system.
yum update
yum install gcc
yum install gcc-c++
yum install gcc.i686
yum install glibc.i686 glibc-devel.i686 glibc-headers.i686 glibc-static.i686 glibc-nss-devel.i686
yum install libstdc++.i686
yum install harfbuzz.i686
yum install zlib.i686
yum install mesa-libGL.i686

pack all /usr /etc directories and extract to /opt/CentOS8/



