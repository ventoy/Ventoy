
Ventoy LiveCD is  Tinycore distro + Ventoy linux install package.


vmlinuz and core.gz are downloaded from:
http://www.tinycorelinux.net/11.x/x86_64/release/distribution_files/

MD5SUM:
598db33adff41155f26e244e492916d0  corepure64.gz
858a5b2524cc541efe2d22b11b271e52  modules64.gz
ab3a8196df5a84889f16494fde188799  vmlinuz64


VTOY/ventoy/tcz/*/tcz are downloaded from:
http://distro.ibiblio.org/tinycorelinux/11.x/x86_64/tcz/

MD5SUM:
803ac92b15e2ba58cddc58e1ff66446c  dosfstools.tcz
eaa8aafb285b3f3bdf89187a964436db  glib2.tcz
bbf81e97259faa73cbaf42b7e76c8685  libffi.tcz
2cb278ef278a6b8819df52ec2e6bedc3  liblvm2.tcz
0345a267ab49e711c596e21eaf721e3b  ncursesw.tcz
65e226c963e78a0174baf99bc9cafcfc  parted.tcz
ae78bbe0c5b7d79382cd1aeb08dc97bd  readline.tcz
dff3775dea468c31e517f5ec5f403ce0  udev-lib.tcz


VTOY/ventoy/drivers/*.ko
build kernel
http://www.tinycorelinux.net/11.x/x86_64/release/src/kernel/
config-5.4.3-tinycore64
linux-5.4.3-patched.txz        
disable a wireless lan driver to avoid compile error

