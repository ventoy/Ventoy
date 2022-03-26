AUTOMAKE_OPTIONS = subdir-objects -Wno-portability

DEPDIR=.deps-core

include $(top_srcdir)/conf/Makefile.common

CC=$(TARGET_CC)
CPP=$(TARGET_CC)
CCAS=$(TARGET_CC)
RANLIB=$(TARGET_RANLIB)
STRIP=$(TARGET_STRIP)

MACHO2IMG=$(top_builddir)/grub-macho2img

AM_CFLAGS = $(TARGET_CFLAGS)
AM_LDFLAGS = $(TARGET_LDFLAGS)
AM_CPPFLAGS = $(TARGET_CPPFLAGS) $(CPPFLAGS_DEFAULT)
AM_CCASFLAGS = $(TARGET_CCASFLAGS) $(CCASFLAGS_DEFAULT)

CFLAGS_PROGRAM += $(CFLAGS_PLATFORM)
LDFLAGS_PROGRAM += $(LDFLAGS_PLATFORM)
CPPFLAGS_PROGRAM += $(CPPFLAGS_PLATFORM)
CCASFLAGS_PROGRAM += $(CCASFLAGS_PLATFORM)

CFLAGS_LIBRARY += $(CFLAGS_PLATFORM) -fno-builtin
CPPFLAGS_LIBRARY += $(CPPFLAGS_PLATFORM)
CCASFLAGS_LIBRARY += $(CCASFLAGS_PLATFORM)

build-grub-pep2elf$(BUILD_EXEEXT): $(top_srcdir)/util/grub-pe2elf.c $(top_srcdir)/grub-core/kern/emu/misc.c $(top_srcdir)/util/misc.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) -DGRUB_BUILD=1 -DGRUB_TARGET_WORDSIZE=64 -DGRUB_UTIL=1 -DGRUB_BUILD_PROGRAM_NAME=\"build-grub-pep2elf\" $^
CLEANFILES += build-grub-pep2elf$(BUILD_EXEEXT)

build-grub-pe2elf$(BUILD_EXEEXT): $(top_srcdir)/util/grub-pe2elf.c $(top_srcdir)/grub-core/kern/emu/misc.c $(top_srcdir)/util/misc.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) -DGRUB_BUILD=1 -DGRUB_TARGET_WORDSIZE=32 -DGRUB_UTIL=1 -DGRUB_BUILD_PROGRAM_NAME=\"build-grub-pe2elf\" $^
CLEANFILES += build-grub-pe2elf$(BUILD_EXEEXT)

# gentrigtables
gentrigtables$(BUILD_EXEEXT): gentrigtables.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) $< $(BUILD_LIBM)
CLEANFILES += gentrigtables$(BUILD_EXEEXT)

build-grub-module-verifier$(BUILD_EXEEXT): $(top_srcdir)/util/grub-module-verifier.c $(top_srcdir)/util/grub-module-verifier32.c $(top_srcdir)/util/grub-module-verifier64.c $(top_srcdir)/grub-core/kern/emu/misc.c $(top_srcdir)/util/misc.c
	$(BUILD_CC) -o $@ -I$(top_srcdir)/include $(BUILD_CFLAGS) $(BUILD_CPPFLAGS) $(BUILD_LDFLAGS) -DGRUB_BUILD=1 -DGRUB_UTIL=1 -DGRUB_BUILD_PROGRAM_NAME=\"build-grub-module-verifier\" $^
CLEANFILES += build-grub-module-verifier$(BUILD_EXEEXT)

# trigtables.c
trigtables.c: gentrigtables$(BUILD_EXEEXT) gentrigtables.c $(top_srcdir)/configure.ac
	./gentrigtables$(BUILD_EXEEXT) > $@
CLEANFILES += trigtables.c

# XXX Use Automake's LEX & YACC support
grub_script.tab.h: script/parser.y
	$(YACC) -d -p grub_script_yy -b grub_script $<
grub_script.tab.c: grub_script.tab.h
CLEANFILES += grub_script.tab.c grub_script.tab.h

# For the lexer.
grub_script.yy.h: script/yylex.l
	$(LEX) -o grub_script.yy.c --header-file=grub_script.yy.h $<
grub_script.yy.c: grub_script.yy.h

rs_decoder.h: $(srcdir)/lib/reed_solomon.c
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -Os -I$(top_builddir) -S -DSTANDALONE -o $@ $< -g0 -mregparm=3 -ffreestanding

CLEANFILES += grub_script.yy.c grub_script.yy.h

include $(srcdir)/Makefile.core.am

KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/cache.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/command.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/device.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/dl.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/env.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/env_private.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/err.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/file.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/fs.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i18n.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/list.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/misc.h
if COND_emu
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/compiler-rt-emu.h
else
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/compiler-rt.h
endif
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/mm.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/parser.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/partition.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/term.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/time.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/mm_private.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/net.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/memory.h

if COND_i386_pc
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/pxe.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/int.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
endif

if COND_i386_xen_pvh
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/int.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/loader.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/xen.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/xen/hypercall.h
endif

if COND_i386_efi
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/pmtimer.h
endif

if COND_i386_coreboot
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/coreboot/lbio.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/video.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/video_fb.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/gfxterm.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/font.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/bufio.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_i386_multiboot
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_i386_qemu
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
endif

if COND_i386_ieee1275
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/ieee1275/ieee1275.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
endif

if COND_i386_xen
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/xen.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/xen/hypercall.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/loader.h
endif

if COND_x86_64_xen
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/xen.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/x86_64/xen/hypercall.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/loader.h
endif

if COND_x86_64_efi
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/tsc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/i386/pmtimer.h
endif

if COND_ia64_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_mips
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/cpu/kernel.h
endif

if COND_mips_arc
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/arc/arc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
endif

if COND_mips_qemu_mips
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/keyboard_layouts.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/serial.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/loader.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
endif

if COND_mips_loongson
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/keyboard_layouts.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/time.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/video.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/video_fb.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/gfxterm.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/font.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/bufio.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/cs5536.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/pci.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/serial.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/loader.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
endif

if COND_mips_qemu_mips
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/memory.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/kernel.h
endif

if COND_mips64_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/loongson.h
endif

if COND_powerpc_ieee1275
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/ieee1275/ieee1275.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
endif

if COND_sparc64_ieee1275
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/ieee1275/ieee1275.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/sparc64/ieee1275/ieee1275.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/terminfo.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
endif

if COND_arm_uboot
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/uboot/uboot.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/uboot/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/lib/arg.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/arm/system.h
endif

if COND_arm_coreboot
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/keyboard_layouts.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/arm/system.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/video.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/video_fb.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/gfxterm.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/font.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/bufio.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/fdt.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/dma.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/arm/coreboot/kernel.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/fdtbus.h
endif

if COND_arm_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/arm/system.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_arm64_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_riscv32_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_riscv64_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
endif

if COND_emu
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/datetime.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/emu/misc.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/emu/net.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/emu/hostdisk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/emu/hostfile.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/extcmd.h
if COND_GRUB_EMU_SDL
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/sdl.h
endif
if COND_GRUB_EMU_PCI
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/libpciaccess.h
endif
endif

symlist.h: $(top_builddir)/config.h $(KERNEL_HEADER_FILES)
	@list='$^'; \
	for p in $$list; do \
	  echo "#include <$$p>" >> $@ || (rm -f $@; exit 1); \
	done
CLEANFILES += symlist.h
BUILT_SOURCES += symlist.h

symlist.c: symlist.h gensymlist.sh
	$(TARGET_CPP) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS_KERNEL) $(CPPFLAGS) -DGRUB_SYMBOL_GENERATOR=1 symlist.h > symlist.p || (rm -f symlist.p; exit 1)
	cat symlist.p | $(SHELL) $(srcdir)/gensymlist.sh $(top_builddir)/config.h $(KERNEL_HEADER_FILES) >$@ || (rm -f $@; exit 1)
	rm -f symlist.p
CLEANFILES += symlist.c
BUILT_SOURCES += symlist.c

if COND_HAVE_ASM_USCORE
ASM_PREFIX=_
else
ASM_PREFIX=
endif

noinst_DATA += kernel_syms.lst

kernel_syms.lst: $(KERNEL_HEADER_FILES) $(top_builddir)/config.h
	$(TARGET_CPP) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS_KERNEL) $(CPPFLAGS) $(CFLAGS) -DGRUB_SYMBOL_GENERATOR=1 $^ >kernel_syms.input
	cat kernel_syms.input | grep -v '^#' | sed -n \
	  -e '/EXPORT_FUNC *([a-zA-Z0-9_]*)/{s/.*EXPORT_FUNC *(\([a-zA-Z0-9_]*\)).*/defined kernel '"$(ASM_PREFIX)"'\1/;p;}' \
	  -e '/EXPORT_VAR *([a-zA-Z0-9_]*)/{s/.*EXPORT_VAR *(\([a-zA-Z0-9_]*\)).*/defined kernel '"$(ASM_PREFIX)"'\1/;p;}' \
	  | sort -u >$@
	rm -f kernel_syms.input
CLEANFILES += kernel_syms.lst

if COND_emu
kern/emu/grub_emu-main.$(OBJEXT):grub_emu_init.h
grub_emu-grub_emu_init.$(OBJEXT):grub_emu_init.h
kern/emu/grub_emu_dyn-main.$(OBJEXT):grub_emu_init.h
grub_emu_dyn-grub_emu_init.$(OBJEXT):grub_emu_init.h

grub_emu_init.h: genemuinitheader.sh $(MODULE_FILES)
	rm -f $@; echo $(MODULE_FILES) | sh $(srcdir)/genemuinitheader.sh $(TARGET_NM) > $@
CLEANFILES += grub_emu_init.h

grub_emu_init.c: grub_emu_init.h genemuinit.sh $(MODULE_FILES)
	rm -f $@; echo $(MODULE_FILES) | sh $(srcdir)/genemuinit.sh $(TARGET_NM) > $@
CLEANFILES += grub_emu_init.c
endif

# List files

fs.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  if grep 'FS_LIST_MARKER' $$pp >/dev/null 2>&1; then \
	    echo $$b; \
	  fi; \
	done) | sort -u > $@
platform_DATA += fs.lst
CLEANFILES += fs.lst

command.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  sed -n \
	    -e "/EXTCOMMAND_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/*\1: $$b/;p;}" \
	    -e "/P1COMMAND_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/*\1: $$b/;p;}" \
	    -e "/COMMAND_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/\1: $$b/;p;}" $$pp; \
	done) | sort -u > $@
platform_DATA += command.lst
CLEANFILES += command.lst

partmap.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  if grep 'PARTMAP_LIST_MARKER' $$pp >/dev/null 2>&1; then \
	    echo $$b; \
	  fi; \
	done) | sort -u > $@
platform_DATA += partmap.lst
CLEANFILES += partmap.lst

terminal.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  sed -n \
	    -e "/INPUT_TERMINAL_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/i\1: $$b/;p;}" \
	    -e "/OUTPUT_TERMINAL_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/o\1: $$b/;p;}" $$pp; \
	done) | sort -u > $@
platform_DATA += terminal.lst
CLEANFILES += terminal.lst

fdt.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  sed -n \
	    -e "/FDT_DRIVER_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/i\1: $$b/;p;}" \
	    -e "/FDT_DRIVER_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/o\1: $$b/;p;}" $$pp; \
	done) | sort -u > $@
platform_DATA += fdt.lst
CLEANFILES += fdt.lst

parttool.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  sed -n \
	    -e "/PARTTOOL_LIST_MARKER *( *\"/{s/.*( *\"\([^\"]*\)\".*/\1: $$b/;p;}" $$pp; \
	done) | sort -u > $@
platform_DATA += parttool.lst
CLEANFILES += parttool.lst

video.lst: $(MARKER_FILES)
	(for pp in $^; do \
	  b=`basename $$pp .marker`; \
	  if grep 'VIDEO_LIST_MARKER' $$pp >/dev/null 2>&1; then \
	    echo $$b; \
	  fi; \
	done) | sort -u > $@
platform_DATA += video.lst
CLEANFILES += video.lst

# but, crypto.lst is simply copied
crypto.lst: $(srcdir)/lib/libgcrypt-grub/cipher/crypto.lst
	cp $^ $@
platform_DATA += crypto.lst
CLEANFILES += crypto.lst

syminfo.lst: gensyminfo.sh kernel_syms.lst $(MODULE_FILES)
	cat kernel_syms.lst > $@.new
	for m in $(MODULE_FILES); do \
	  sh $< $$m >> $@.new || exit 1; \
	done
	mv $@.new $@

# generate global module dependencies list
moddep.lst: syminfo.lst genmoddep.awk video.lst
	cat $< | sort | $(AWK) -f $(srcdir)/genmoddep.awk > $@ || (rm -f $@; exit 1)
platform_DATA += moddep.lst
CLEANFILES += config.log syminfo.lst moddep.lst

$(MOD_FILES): %.mod : genmod.sh moddep.lst %.module$(EXEEXT) build-grub-module-verifier$(BUILD_EXEEXT)
	TARGET_OBJ2ELF=@TARGET_OBJ2ELF@ sh $^ $@
platform_DATA += $(MOD_FILES)
platform_DATA += modinfo.sh
CLEANFILES += $(MOD_FILES)

if COND_ENABLE_EFIEMU
efiemu32.o: efiemu/runtime/efiemu.c $(TARGET_OBJ2ELF)
	-rm -f $@
	-rm -f $@.bin
	$(TARGET_CC) $(DEFS) $(INCLUDES) $(CPPFLAGS_EFIEMU) $(CPPFLAGS_DEFAULT) -m32 -Wall -Werror -nostdlib -static -O2 -c -o $@.bin $<
	if test "x$(TARGET_APPLE_LINKER)" = x1; then \
	  $(TARGET_OBJCONV) -felf32 -nu -nd $@.bin $@ || exit 1; \
	  rm -f $@.bin ; \
	elif test ! -z "$(TARGET_OBJ2ELF)"; then \
	  $(TARGET_OBJ2ELF) $@.bin || (rm -f $@.bin; exit 1); \
	  mv $@.bin $@ ; \
	else \
	  mv $@.bin $@ ; \
	fi

# Link format -arch,x86_64 means Apple linker
efiemu64_c.o: efiemu/runtime/efiemu.c
	$(TARGET_CC) $(DEFS) $(INCLUDES) $(CPPFLAGS_EFIEMU) $(CPPFLAGS_DEFAULT) -m64 -nostdlib -Wall -Werror -O2 -mcmodel=large -mno-red-zone -c -o $@ $<

efiemu64_s.o: efiemu/runtime/efiemu.S
	$(TARGET_CC) $(DEFS) $(INCLUDES) $(CPPFLAGS_EFIEMU) $(CPPFLAGS_DEFAULT) -m64 -Wall -Werror -nostdlib -O2 -mcmodel=large -mno-red-zone -c -o $@ $<

efiemu64.o: efiemu64_c.o efiemu64_s.o $(TARGET_OBJ2ELEF)
	-rm -f $@
	-rm -f $@.bin
	$(TARGET_CC) -m64 $(EFIEMU64_LINK_FORMAT) -nostdlib -static -Wl,-r -o $@.bin $^
	if test "x$(EFIEMU64_LINK_FORMAT)" = x-arch,x86_64; then \
	  $(TARGET_OBJCONV) -felf64 -nu -nd $@.bin $@ || exit 1; \
	  rm -f $@.bin; \
	else \
	  mv $@.bin $@ ; \
	fi

platform_DATA += efiemu32.o efiemu64.o
CLEANFILES += efiemu32.o efiemu64.o efiemu64_c.o efiemu64_s.o
endif

windowsdir=$(top_builddir)/$(PACKAGE)-$(VERSION)-for-windows
windowsdir: $(PROGRAMS) $(starfield_DATA) $(platform_DATA)
	test -d $(windowsdir)/$(target_cpu)-$(platform) || mkdir $(windowsdir)/$(target_cpu)-$(platform)
	for x in $(platform_DATA); do \
		cp -fp $$x $(windowsdir)/$(target_cpu)-$(platform)/$$x; \
	done
