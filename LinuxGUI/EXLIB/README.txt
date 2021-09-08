
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

2. download debian 9.x mips64el DVD iso (e.g. debian-9.9.0-mips64el-DVD-1.iso) form internet
3. download libgtk-3-dev_3.22.11-1_mips64el.deb from internet
4. mount debian-9.9.0-mips64el-DVD-1.iso /mnt
5. sh prepare_lib_mips64el.sh /mnt/

