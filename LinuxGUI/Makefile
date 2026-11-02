CC = gcc
UDISKS2 := $(shell pkg-config --libs -cflags udisks2)
GTK3 := $(shell pkg-config --libs -cflags gtk+-3.0)

civetweb.o:
	$(CC) -std=gnu99 -D_FILE_OFFSET_BITS=64 -c -Wall -Wextra -Wshadow -Wformat-security -Winit-self \
        -Wmissing-prototypes -O2 -DLINUX \
        -I./Ventoy2Disk/Lib/libhttp/include \
        -DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED \
        -DUSE_STACK_SIZE=102400 -DNDEBUG -fPIC \
        ./Ventoy2Disk/Lib/libhttp/include/civetweb.c \
        -o ./civetweb.o
    
gtk3: civetweb.o
	$(CC) -O2 -Wall -Wno-unused-function -DSTATIC=static -DINIT= \
        -I./Ventoy2Disk \
        -I./Ventoy2Disk/Core \
        -I./Ventoy2Disk/Web \
        -I./Ventoy2Disk/GTK \
        -I./Ventoy2Disk/Include \
        -I./Ventoy2Disk/Lib/libhttp/include \
        -I./Ventoy2Disk/Lib/fat_io_lib/include \
        -I./Ventoy2Disk/Lib/xz-embedded/linux/include \
        -I./Ventoy2Disk/Lib/xz-embedded/linux/include/linux \
        -I./Ventoy2Disk/Lib/xz-embedded/userspace \
        -I ./Ventoy2Disk/Lib/exfat/src/libexfat \
        -I ./Ventoy2Disk/Lib/exfat/src/mkfs \
        -I ./Ventoy2Disk/Lib/fat_io_lib \
        \
        -L ./Ventoy2Disk/Lib/fat_io_lib/lib \
        Ventoy2Disk/main_gtk.c \
        Ventoy2Disk/Core/*.c \
        Ventoy2Disk/Web/*.c \
        Ventoy2Disk/GTK/*.c \
        Ventoy2Disk/Lib/xz-embedded/linux/lib/decompress_unxz.c \
        Ventoy2Disk/Lib/exfat/src/libexfat/*.c \
        Ventoy2Disk/Lib/exfat/src/mkfs/*.c \
        Ventoy2Disk/Lib/fat_io_lib/*.c \
		-std=gnu99 -D_FILE_OFFSET_BITS=64 \
		$(GTK3) $(UDISKS2) \
        -l pthread \
        -s \
        ./civetweb.o \
        -o Ventoy2Disk.gtk3

clean:
	rm -f civetweb.o
	rm -f Ventoy2Disk.gtk3

