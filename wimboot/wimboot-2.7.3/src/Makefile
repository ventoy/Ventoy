VERSION := v2.7.3

OBJECTS := prefix.o startup.o callback.o main.o vsprintf.o string.o peloader.o
OBJECTS += int13.o vdisk.o cpio.o stdio.o lznt1.o xca.o die.o efi.o efimain.o
OBJECTS += efiguid.o efifile.o efipath.o efiboot.o efiblock.o cmdline.o
OBJECTS += wimpatch.o huffman.o lzx.o wim.o wimfile.o pause.o sha1.o cookie.o
OBJECTS += paging.o memmap.o

OBJECTS_i386 := $(patsubst %.o,%.i386.o,$(OBJECTS))
OBJECTS_x86_64 := $(patsubst %.o,%.x86_64.o,$(OBJECTS))
OBJECTS_i386_x86_64 := $(patsubst %.o,%.i386.x86_64.o,$(OBJECTS))

HEADERS := $(wildcard *.h)

HOST_CC		:= $(CC)
AS		:= $(AS)
ECHO		:= echo
OBJCOPY 	:= objcopy
AR		:= ar
RANLIB		:= ranlib
CP		:= cp
RM		:= rm
GCAB		:= gcab
PESIGN		:= pesign
DIFF		:= diff
CUT		:= cut
BINUTILS_DIR	:= /usr
BFD_DIR		:= $(BINUTILS_DIR)
ZLIB_DIR	:= /usr

HOST_CFLAGS += -Wall -W -Werror

CFLAGS += -Os -ffreestanding -Wall -W -Werror -nostdinc -I. -fshort-wchar
CFLAGS += -DVERSION="\"$(VERSION)\""

CFLAGS_i386 += -m32 -march=i386 -malign-double -fno-pic
CFLAGS_x86_64 += -m64 -mno-red-zone -fpie

# Enable stack protection if available
#
SPG_TEST = $(CC) -fstack-protector-strong -mstack-protector-guard=global \
		 -x c -c /dev/null -o /dev/null >/dev/null 2>&1
SPG_FLAGS := $(shell $(SPG_TEST) && $(ECHO) '-fstack-protector-strong ' \
					    '-mstack-protector-guard=global')
CFLAGS += $(SPG_FLAGS)

# Inhibit unwanted debugging information
CFI_TEST = $(CC) -fno-dwarf2-cfi-asm -fno-exceptions -fno-unwind-tables \
		 -fno-asynchronous-unwind-tables -x c -c /dev/null \
		 -o /dev/null >/dev/null 2>&1
CFI_FLAGS := $(shell $(CFI_TEST) && \
	       $(ECHO) '-fno-dwarf2-cfi-asm -fno-exceptions ' \
		    '-fno-unwind-tables -fno-asynchronous-unwind-tables')
WORKAROUND_CFLAGS += $(CFI_FLAGS)

# Add -maccumulate-outgoing-args if required by this version of gcc
MS_ABI_TEST_CODE := extern void __attribute__ (( ms_abi )) ms_abi(); \
		    void sysv_abi ( void ) { ms_abi(); }
MS_ABI_TEST = $(ECHO) '$(MS_ABI_TEST_CODE)' | \
	      $(CC) -m64 -mno-accumulate-outgoing-args -x c -c - -o /dev/null \
		    >/dev/null 2>&1
MS_ABI_FLAGS := $(shell $(MS_ABI_TEST) || $(ECHO) '-maccumulate-outgoing-args')
WORKAROUND_CFLAGS += $(MS_ABI_FLAGS)

# Inhibit warnings from taking address of packed struct members
WNAPM_TEST = $(CC) -Wno-address-of-packed-member -x c -c /dev/null \
		   -o /dev/null >/dev/null 2>&1
WNAPM_FLAGS := $(shell $(WNAPM_TEST) && \
		 $(ECHO) '-Wno-address-of-packed-member')
WORKAROUND_CFLAGS += $(WNAPM_FLAGS)

# Inhibit LTO
LTO_TEST = $(CC) -fno-lto -x c -c /dev/null -o /dev/null >/dev/null 2>&1
LTO_FLAGS := $(shell $(LTO_TEST) && $(ECHO) '-fno-lto')
WORKAROUND_CFLAGS += $(LTO_FLAGS)

CFLAGS += $(WORKAROUND_CFLAGS)
CFLAGS += $(EXTRA_CFLAGS)

ifneq ($(DEBUG),)
CFLAGS += -DDEBUG=$(DEBUG)
endif

CFLAGS += -include compiler.h

###############################################################################
#
# Final targets

all : wimboot wimboot.i386 wimboot.x86_64 wimboot.cab

wimboot : wimboot.x86_64 Makefile
	$(CP) $< $@
	$(CP) $@ ../$@

wimboot.%.elf : prefix.%.o lib.%.a script.lds Makefile
	$(LD) -m elf_$* -T script.lds -o $@ -q -Map wimboot.$*.map \
		prefix.$*.o lib.$*.a

wimboot.%.unsigned : wimboot.%.elf efireloc Makefile
	$(OBJCOPY) -Obinary $< $@
	./efireloc $< $@

wimboot.%.unsigned.hash : wimboot.%.unsigned Makefile
	$(PESIGN) -h -i $< | $(CUT) -d" " -f2- > $@

wimboot.%.efi : wimboot.%.unsigned Makefile
	$(CP) $< $@

wimboot.%.efi.hash : wimboot.%.efi Makefile
	$(PESIGN) -h -i $< | $(CUT) -d" " -f2- > $@

wimboot.% : wimboot.%.efi wimboot.%.efi.hash wimboot.%.unsigned.hash Makefile
	$(DIFF) wimboot.$*.efi.hash wimboot.$*.unsigned.hash
	$(CP) $< $@

wimboot.cab : wimboot.i386.efi wimboot.x86_64.efi Makefile
	$(GCAB) -n -c $@ wimboot.i386.efi wimboot.x86_64.efi

###############################################################################
#
# i386 objects

%.i386.s : %.S $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CFLAGS_i386) -DASSEMBLY -Ui386 -E $< -o $@

%.i386.s : %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CFLAGS_i386) -S $< -o $@

%.i386.o : %.i386.s i386.i Makefile
	$(AS) --32 i386.i $< -o $@

lib.i386.a : $(OBJECTS_i386) Makefile
	$(RM) -f $@
	$(AR) r $@ $(OBJECTS_i386)
	$(RANLIB) $@

###############################################################################
#
# i386 objects to be linked into an x86_64 binary

%.i386.x86_64.raw.o : %.i386.s i386.i Makefile
	$(AS) --64 i386.i $< -o $@

%.i386.x86_64.o : %.i386.x86_64.raw.o Makefile
	$(OBJCOPY) --prefix-symbols=__i386_ $< $@

###############################################################################
#
# x86_64 objects

%.x86_64.s : %.S $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CFLAGS_x86_64) -DASSEMBLY -Ui386 -E $< -o $@

%.x86_64.s : %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CFLAGS_x86_64) -S $< -o $@

%.x86_64.o : %.x86_64.s x86_64.i Makefile
	$(AS) --64 x86_64.i $< -o $@

lib.x86_64.a : $(OBJECTS_x86_64) $(OBJECTS_i386_x86_64) Makefile
	$(RM) -f $@
	$(AR) r $@ $(OBJECTS_x86_64) $(OBJECTS_i386_x86_64)
	$(RANLIB) $@

###############################################################################
#
# EFI relocator

EFIRELOC_CFLAGS := -I$(BINUTILS_DIR)/include -I$(BFD_DIR)/include \
		   -I$(ZLIB_DIR)/include -idirafter .
EFIRELOC_LDFLAGS := -L$(BINUTILS_DIR)/lib -L$(BFD_DIR)/lib -L$(ZLIB_DIR)/lib \
		    -lbfd -ldl -liberty -lz -Wl,--no-warn-search-mismatch

efireloc : efireloc.c Makefile
	$(CC) $(HOST_CFLAGS) $(EFIRELOC_CFLAGS) $< $(EFIRELOC_LDFLAGS) -o $@

###############################################################################
#
# Cleanup

clean :
	$(RM) -f *.s *.o *.a *.elf *.map
	$(RM) -f efireloc
	$(RM) -f wimboot wimboot.i386 wimboot.x86_64 ../wimboot
	$(RM) -f wimboot.i386.unsigned wimboot.x86_64.unsigned
	$(RM) -f wimboot.i386.efi wimboot.x86_64.efi wimboot.cab
