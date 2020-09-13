
Ventoy LiveCD is  Tinycore distro + Ventoy linux install package.


vmlinuz and core.gz are downloaded from:
http://www.tinycorelinux.net/11.x/x86/release/distribution_files/

MD5SUM:
0fd08c73e84b26aabbd0d12006d64855  core.gz
a9c2e2abbf464517e681234fb4687aa1  vmlinuz



VTOY/ventoy/tcz/*/tcz are downloaded from:
http://distro.ibiblio.org/tinycorelinux/11.x/x86/tcz/

MD5SUM:
b6153a469d1d56e1e6895c6812a344cd  dosfstools.tcz
29a4585d38b34ad58f8a7cb2d09e065f  glib2.tcz
6812067a60165aee3cbcc07a75b6b1f4  libffi.tcz
5120e0c9ee65f936dea8cb6a0a0a1ddd  liblvm2.tcz
92909db8fb3c4333a2a4a325ffbf4b50  ncursesw.tcz
e2bb47c9da2abab62fa794d69aba97c0  parted.tcz
0e6dfbebe816062a81aff6d3e5e7719b  readline.tcz
3cf996373ab01be269ea0efaf17ce0cd  udev-lib.tcz


VTOY/ventoy/drivers/*.ko
build kernel
http://www.tinycorelinux.net/11.x/x86/release/src/kernel/
config-5.4.3-tinycore
linux-5.4.3-patched.txz           
disable a wireless lan driver to avoid compile error

