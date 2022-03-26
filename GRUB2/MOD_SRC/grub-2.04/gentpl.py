#! /usr/bin/python
#  GRUB  --  GRand Unified Bootloader
#  Copyright (C) 2010,2011,2012,2013  Free Software Foundation, Inc.
#
#  GRUB is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  GRUB is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function

__metaclass__ = type

from optparse import OptionParser
import re

#
# This is the python script used to generate Makefile.*.am
#

GRUB_PLATFORMS = [ "emu", "i386_pc", "i386_efi", "i386_qemu", "i386_coreboot",
                   "i386_multiboot", "i386_ieee1275", "x86_64_efi",
                   "i386_xen", "x86_64_xen", "i386_xen_pvh",
                   "mips_loongson", "mips64_efi", "sparc64_ieee1275",
                   "powerpc_ieee1275", "mips_arc", "ia64_efi",
                   "mips_qemu_mips", "arm_uboot", "arm_efi", "arm64_efi",
                   "arm_coreboot", "riscv32_efi", "riscv64_efi" ]

GROUPS = {}

GROUPS["common"]   = GRUB_PLATFORMS[:]

# Groups based on CPU
GROUPS["i386"]     = [ "i386_pc", "i386_efi", "i386_qemu", "i386_coreboot", "i386_multiboot", "i386_ieee1275" ]
GROUPS["x86_64"]   = [ "x86_64_efi" ]
GROUPS["x86"]      = GROUPS["i386"] + GROUPS["x86_64"]
GROUPS["mips"]     = [ "mips_loongson", "mips_qemu_mips", "mips_arc" ]
GROUPS["mips64"]   = [ "mips64_efi" ]
GROUPS["sparc64"]  = [ "sparc64_ieee1275" ]
GROUPS["powerpc"]  = [ "powerpc_ieee1275" ]
GROUPS["arm"]      = [ "arm_uboot", "arm_efi", "arm_coreboot" ]
GROUPS["arm64"]    = [ "arm64_efi" ]
GROUPS["riscv32"]  = [ "riscv32_efi" ]
GROUPS["riscv64"]  = [ "riscv64_efi" ]

# Groups based on firmware
GROUPS["efi"]  = [ "i386_efi", "x86_64_efi", "ia64_efi", "arm_efi", "arm64_efi", "mips64_efi",
		   "riscv32_efi", "riscv64_efi" ]
GROUPS["ieee1275"]   = [ "i386_ieee1275", "sparc64_ieee1275", "powerpc_ieee1275" ]
GROUPS["uboot"] = [ "arm_uboot" ]
GROUPS["xen"]  = [ "i386_xen", "x86_64_xen" ]
GROUPS["coreboot"]  = [ "i386_coreboot", "arm_coreboot" ]

# emu is a special case so many core functionality isn't needed on this platform
GROUPS["noemu"]   = GRUB_PLATFORMS[:]; GROUPS["noemu"].remove("emu")

# Groups based on hardware features
GROUPS["cmos"] = GROUPS["x86"][:] + ["mips_loongson", "mips_qemu_mips",
                                     "sparc64_ieee1275", "powerpc_ieee1275"]
GROUPS["cmos"].remove("i386_efi"); GROUPS["cmos"].remove("x86_64_efi");
GROUPS["pci"]      = GROUPS["x86"] + ["mips_loongson"]
GROUPS["usb"]      = GROUPS["pci"] + ["arm_coreboot"]

# If gfxterm is main output console integrate it into kernel
GROUPS["videoinkernel"] = ["mips_loongson", "i386_coreboot", "arm_coreboot" ]
GROUPS["videomodules"]   = GRUB_PLATFORMS[:];
for i in GROUPS["videoinkernel"]: GROUPS["videomodules"].remove(i)

# Similar for terminfo
GROUPS["terminfoinkernel"] = [ "emu", "mips_loongson", "mips_arc", "mips_qemu_mips", "i386_xen_pvh" ] + GROUPS["xen"] + GROUPS["ieee1275"] + GROUPS["uboot"];
GROUPS["terminfomodule"]   = GRUB_PLATFORMS[:];
for i in GROUPS["terminfoinkernel"]: GROUPS["terminfomodule"].remove(i)

# Flattened Device Trees (FDT)
GROUPS["fdt"] = [ "arm64_efi", "arm_uboot", "arm_efi", "riscv32_efi", "riscv64_efi" ]

# Needs software helpers for division
# Must match GRUB_DIVISION_IN_SOFTWARE in misc.h
GROUPS["softdiv"] = GROUPS["arm"] + ["ia64_efi"] + GROUPS["riscv32"]
GROUPS["no_softdiv"]   = GRUB_PLATFORMS[:]
for i in GROUPS["softdiv"]: GROUPS["no_softdiv"].remove(i)

# Miscellaneous groups scheduled to disappear in future
GROUPS["i386_coreboot_multiboot_qemu"] = ["i386_coreboot", "i386_multiboot", "i386_qemu"]
GROUPS["nopc"] = GRUB_PLATFORMS[:]; GROUPS["nopc"].remove("i386_pc")

#
# Create platform => groups reverse map, where groups covering that
# platform are ordered by their sizes
#
RMAP = {}
for platform in GRUB_PLATFORMS:
    # initialize with platform itself as a group
    RMAP[platform] = [ platform ]

    for k in GROUPS.keys():
        v = GROUPS[k]
        # skip groups that don't cover this platform
        if platform not in v: continue

        bigger = []
        smaller = []
        # partition currently known groups based on their size
        for group in RMAP[platform]:
            if group in GRUB_PLATFORMS: smaller.append(group)
            elif len(GROUPS[group]) < len(v): smaller.append(group)
            else: bigger.append(group)
        # insert in the middle
        RMAP[platform] = smaller + [ k ] + bigger

#
# Input
#

# We support a subset of the AutoGen definitions file syntax.  Specifically,
# compound names are disallowed; some preprocessing directives are
# disallowed (though #if/#endif are allowed; note that, like AutoGen, #if
# skips everything to the next #endif regardless of the value of the
# conditional); and shell-generated strings, Scheme-generated strings, and
# here strings are disallowed.

class AutogenToken:
    (autogen, definitions, eof, var_name, other_name, string, number,
     semicolon, equals, comma, lbrace, rbrace, lbracket, rbracket) = range(14)

class AutogenState:
    (init, need_def, need_tpl, need_semi, need_name, have_name, need_value,
     need_idx, need_rbracket, indx_name, have_value, done) = range(12)

class AutogenParseError(Exception):
    def __init__(self, message, path, line):
        super(AutogenParseError, self).__init__(message)
        self.path = path
        self.line = line

    def __str__(self):
        return (
            super(AutogenParseError, self).__str__() +
            " at file %s line %d" % (self.path, self.line))

class AutogenDefinition(list):
    def __getitem__(self, key):
        try:
            return super(AutogenDefinition, self).__getitem__(key)
        except TypeError:
            for name, value in self:
                if name == key:
                    return value

    def __contains__(self, key):
        for name, value in self:
            if name == key:
                return True
        return False

    def get(self, key, default):
        for name, value in self:
            if name == key:
                return value
        else:
            return default

    def find_all(self, key):
        for name, value in self:
            if name == key:
                yield value

class AutogenParser:
    def __init__(self):
        self.definitions = AutogenDefinition()
        self.def_stack = [("", self.definitions)]
        self.curdef = None
        self.new_name = None
        self.cur_path = None
        self.cur_line = 0

    @staticmethod
    def is_unquotable_char(c):
        return (ord(c) in range(ord("!"), ord("~") + 1) and
                c not in "#,;<=>[\\]`{}?*'\"()")

    @staticmethod
    def is_value_name_char(c):
        return c in ":^-_" or c.isalnum()

    def error(self, message):
        raise AutogenParseError(message, self.cur_file, self.cur_line)

    def read_tokens(self, f):
        data = f.read()
        end = len(data)
        offset = 0
        while offset < end:
            while offset < end and data[offset].isspace():
                if data[offset] == "\n":
                    self.cur_line += 1
                offset += 1
            if offset >= end:
                break
            c = data[offset]
            if c == "#":
                offset += 1
                try:
                    end_directive = data.index("\n", offset)
                    directive = data[offset:end_directive]
                    offset = end_directive
                except ValueError:
                    directive = data[offset:]
                    offset = end
                name, value = directive.split(None, 1)
                if name == "if":
                    try:
                        end_if = data.index("\n#endif", offset)
                        new_offset = end_if + len("\n#endif")
                        self.cur_line += data[offset:new_offset].count("\n")
                        offset = new_offset
                    except ValueError:
                        self.error("#if without matching #endif")
                else:
                    self.error("Unhandled directive '#%s'" % name)
            elif c == "{":
                yield AutogenToken.lbrace, c
                offset += 1
            elif c == "=":
                yield AutogenToken.equals, c
                offset += 1
            elif c == "}":
                yield AutogenToken.rbrace, c
                offset += 1
            elif c == "[":
                yield AutogenToken.lbracket, c
                offset += 1
            elif c == "]":
                yield AutogenToken.rbracket, c
                offset += 1
            elif c == ";":
                yield AutogenToken.semicolon, c
                offset += 1
            elif c == ",":
                yield AutogenToken.comma, c
                offset += 1
            elif c in ("'", '"'):
                s = []
                while True:
                    offset += 1
                    if offset >= end:
                        self.error("EOF in quoted string")
                    if data[offset] == "\n":
                        self.cur_line += 1
                    if data[offset] == "\\":
                        offset += 1
                        if offset >= end:
                            self.error("EOF in quoted string")
                        if data[offset] == "\n":
                            self.cur_line += 1
                        # Proper escaping unimplemented; this can be filled
                        # out if needed.
                        s.append("\\")
                        s.append(data[offset])
                    elif data[offset] == c:
                        offset += 1
                        break
                    else:
                        s.append(data[offset])
                yield AutogenToken.string, "".join(s)
            elif c == "/":
                offset += 1
                if data[offset] == "*":
                    offset += 1
                    try:
                        end_comment = data.index("*/", offset)
                        new_offset = end_comment + len("*/")
                        self.cur_line += data[offset:new_offset].count("\n")
                        offset = new_offset
                    except ValueError:
                        self.error("/* without matching */")
                elif data[offset] == "/":
                    try:
                        offset = data.index("\n", offset)
                    except ValueError:
                        pass
            elif (c.isdigit() or
                  (c == "-" and offset < end - 1 and
                   data[offset + 1].isdigit())):
                end_number = offset + 1
                while end_number < end and data[end_number].isdigit():
                    end_number += 1
                yield AutogenToken.number, data[offset:end_number]
                offset = end_number
            elif self.is_unquotable_char(c):
                end_name = offset
                while (end_name < end and
                       self.is_value_name_char(data[end_name])):
                    end_name += 1
                if end_name < end and self.is_unquotable_char(data[end_name]):
                    while (end_name < end and
                           self.is_unquotable_char(data[end_name])):
                        end_name += 1
                    yield AutogenToken.other_name, data[offset:end_name]
                    offset = end_name
                else:
                    s = data[offset:end_name]
                    if s.lower() == "autogen":
                        yield AutogenToken.autogen, s
                    elif s.lower() == "definitions":
                        yield AutogenToken.definitions, s
                    else:
                        yield AutogenToken.var_name, s
                    offset = end_name
            else:
                self.error("Invalid input character '%s'" % c)
        yield AutogenToken.eof, None

    def do_need_name_end(self, token):
        if len(self.def_stack) > 1:
            self.error("Definition blocks were left open")

    def do_need_name_var_name(self, token):
        self.new_name = token

    def do_end_block(self, token):
        if len(self.def_stack) <= 1:
            self.error("Too many close braces")
        new_name, parent_def = self.def_stack.pop()
        parent_def.append((new_name, self.curdef))
        self.curdef = parent_def

    def do_empty_val(self, token):
        self.curdef.append((self.new_name, ""))

    def do_str_value(self, token):
        self.curdef.append((self.new_name, token))

    def do_start_block(self, token):
        self.def_stack.append((self.new_name, self.curdef))
        self.curdef = AutogenDefinition()

    def do_indexed_name(self, token):
        self.new_name = token

    def read_definitions_file(self, f):
        self.curdef = self.definitions
        self.cur_line = 0
        state = AutogenState.init

        # The following transition table was reduced from the Autogen
        # documentation:
        #   info -f autogen -n 'Full Syntax'
        transitions = {
            AutogenState.init: {
                AutogenToken.autogen: (AutogenState.need_def, None),
            },
            AutogenState.need_def: {
                AutogenToken.definitions: (AutogenState.need_tpl, None),
            },
            AutogenState.need_tpl: {
                AutogenToken.var_name: (AutogenState.need_semi, None),
                AutogenToken.other_name: (AutogenState.need_semi, None),
                AutogenToken.string: (AutogenState.need_semi, None),
            },
            AutogenState.need_semi: {
                AutogenToken.semicolon: (AutogenState.need_name, None),
            },
            AutogenState.need_name: {
                AutogenToken.autogen: (AutogenState.need_def, None),
                AutogenToken.eof: (AutogenState.done, self.do_need_name_end),
                AutogenToken.var_name: (
                    AutogenState.have_name, self.do_need_name_var_name),
                AutogenToken.rbrace: (
                    AutogenState.have_value, self.do_end_block),
            },
            AutogenState.have_name: {
                AutogenToken.semicolon: (
                    AutogenState.need_name, self.do_empty_val),
                AutogenToken.equals: (AutogenState.need_value, None),
                AutogenToken.lbracket: (AutogenState.need_idx, None),
            },
            AutogenState.need_value: {
                AutogenToken.var_name: (
                    AutogenState.have_value, self.do_str_value),
                AutogenToken.other_name: (
                    AutogenState.have_value, self.do_str_value),
                AutogenToken.string: (
                    AutogenState.have_value, self.do_str_value),
                AutogenToken.number: (
                    AutogenState.have_value, self.do_str_value),
                AutogenToken.lbrace: (
                    AutogenState.need_name, self.do_start_block),
            },
            AutogenState.need_idx: {
                AutogenToken.var_name: (
                    AutogenState.need_rbracket, self.do_indexed_name),
                AutogenToken.number: (
                    AutogenState.need_rbracket, self.do_indexed_name),
            },
            AutogenState.need_rbracket: {
                AutogenToken.rbracket: (AutogenState.indx_name, None),
            },
            AutogenState.indx_name: {
                AutogenToken.semicolon: (
                    AutogenState.need_name, self.do_empty_val),
                AutogenToken.equals: (AutogenState.need_value, None),
            },
            AutogenState.have_value: {
                AutogenToken.semicolon: (AutogenState.need_name, None),
                AutogenToken.comma: (AutogenState.need_value, None),
            },
        }

        for code, token in self.read_tokens(f):
            if code in transitions[state]:
                state, handler = transitions[state][code]
                if handler is not None:
                    handler(token)
            else:
                self.error(
                    "Parse error in state %s: unexpected token '%s'" % (
                        state, token))
            if state == AutogenState.done:
                break

    def read_definitions(self, path):
        self.cur_file = path
        with open(path) as f:
            self.read_definitions_file(f)

defparser = AutogenParser()

#
# Output
#

outputs = {}

def output(s, section=''):
    if s == "":
        return
    outputs.setdefault(section, [])
    outputs[section].append(s)

def write_output(section=''):
    for s in outputs.get(section, []):
        print(s, end='')

#
# Global variables
#

def gvar_add(var, value):
    output(var + " += " + value + "\n")

#
# Per PROGRAM/SCRIPT variables 
#

seen_vars = set()

def vars_init(defn, *var_list):
    name = defn['name']

    if name not in seen_target and name not in seen_vars:
        for var in var_list:
            output(var + "  = \n", section='decl')
        seen_vars.add(name)

def var_set(var, value):
    output(var + "  = " + value + "\n")

def var_add(var, value):
    output(var + " += " + value + "\n")

#
# Variable names and rules
#

canonical_name_re = re.compile(r'[^0-9A-Za-z@_]')
canonical_name_suffix = ""

def set_canonical_name_suffix(suffix):
    global canonical_name_suffix
    canonical_name_suffix = suffix

def cname(defn):
    return canonical_name_re.sub('_', defn['name'] + canonical_name_suffix)

def rule(target, source, cmd):
    if cmd[0] == "\n":
        output("\n" + target + ": " + source + cmd.replace("\n", "\n\t") + "\n")
    else:
        output("\n" + target + ": " + source + "\n\t" + cmd.replace("\n", "\n\t") + "\n")

#
# Handle keys with platform names as values, for example:
#
# kernel = {
#   nostrip = emu;
#   ...
# }
#
def platform_tagged(defn, platform, tag):
    for value in defn.find_all(tag):
        for group in RMAP[platform]:
            if value == group:
                return True
    return False

def if_platform_tagged(defn, platform, tag, snippet_if, snippet_else=None):
    if platform_tagged(defn, platform, tag):
        return snippet_if
    elif snippet_else is not None:
        return snippet_else

#
# Handle tagged values
#
# module = {
#   extra_dist = ...
#   extra_dist = ...
#   ...
# };
#
def foreach_value(defn, tag, closure):
    r = []
    for value in defn.find_all(tag):
        r.append(closure(value))
    return ''.join(r)

#
# Handle best matched values for a platform, for example:
#
# module = {
#   cflags = '-Wall';
#   emu_cflags = '-Wall -DGRUB_EMU=1';
#   ...
# }
#
def foreach_platform_specific_value(defn, platform, suffix, nonetag, closure):
    r = []
    for group in RMAP[platform]:
        values = list(defn.find_all(group + suffix))
        if values:
            for value in values:
                r.append(closure(value))
            break
    else:
        for value in defn.find_all(nonetag):
            r.append(closure(value))
    return ''.join(r)

#
# Handle values from sum of all groups for a platform, for example:
#
# module = {
#   common = kern/misc.c;
#   emu = kern/emu/misc.c;
#   ...
# }
#
def foreach_platform_value(defn, platform, suffix, closure):
    r = []
    for group in RMAP[platform]:
        for value in defn.find_all(group + suffix):
            r.append(closure(value))
    return ''.join(r)

def platform_conditional(platform, closure):
    output("\nif COND_" + platform + "\n")
    closure(platform)
    output("endif\n")

#
# Handle guarding with platform-specific "enable" keys, for example:
#
#  module = {
#    name = pci;
#    noemu = bus/pci.c;
#    emu = bus/emu/pci.c;
#    emu = commands/lspci.c;
#
#    enable = emu;
#    enable = i386_pc;
#    enable = x86_efi;
#    enable = i386_ieee1275;
#    enable = i386_coreboot;
#  };
#
def foreach_enabled_platform(defn, closure):
    if 'enable' in defn:
        for platform in GRUB_PLATFORMS:
            if platform_tagged(defn, platform, "enable"):
               platform_conditional(platform, closure)
    else:
        for platform in GRUB_PLATFORMS:
            platform_conditional(platform, closure)

#
# Handle guarding with platform-specific automake conditionals, for example:
#
#  module = {
#    name = usb;
#    common = bus/usb/usb.c;
#    noemu = bus/usb/usbtrans.c;
#    noemu = bus/usb/usbhub.c;
#    enable = emu;
#    enable = i386;
#    enable = mips_loongson;
#    emu_condition = COND_GRUB_EMU_SDL;
#  };
#
def under_platform_specific_conditionals(defn, platform, closure):
    output(foreach_platform_specific_value(defn, platform, "_condition", "condition", lambda cond: "if " + cond + "\n"))
    closure(defn, platform)
    output(foreach_platform_specific_value(defn, platform, "_condition", "condition", lambda cond: "endif " + cond + "\n"))

def platform_specific_values(defn, platform, suffix, nonetag):
    return foreach_platform_specific_value(defn, platform, suffix, nonetag,
                                           lambda value: value + " ")

def platform_values(defn, platform, suffix):
    return foreach_platform_value(defn, platform, suffix, lambda value: value + " ")

def extra_dist(defn):
    return foreach_value(defn, "extra_dist", lambda value: value + " ")

def platform_sources(defn, p): return platform_values(defn, p, "")
def platform_nodist_sources(defn, p): return platform_values(defn, p, "_nodist")

def platform_startup(defn, p): return platform_specific_values(defn, p, "_startup", "startup")
def platform_ldadd(defn, p): return platform_specific_values(defn, p, "_ldadd", "ldadd")
def platform_dependencies(defn, p): return platform_specific_values(defn, p, "_dependencies", "dependencies")
def platform_cflags(defn, p): return platform_specific_values(defn, p, "_cflags", "cflags")
def platform_ldflags(defn, p): return platform_specific_values(defn, p, "_ldflags", "ldflags")
def platform_cppflags(defn, p): return platform_specific_values(defn, p, "_cppflags", "cppflags")
def platform_ccasflags(defn, p): return platform_specific_values(defn, p, "_ccasflags", "ccasflags")
def platform_stripflags(defn, p): return platform_specific_values(defn, p, "_stripflags", "stripflags")
def platform_objcopyflags(defn, p): return platform_specific_values(defn, p, "_objcopyflags", "objcopyflags")

#
# Emit snippet only the first time through for the current name.
#
seen_target = set()

def first_time(defn, snippet):
    if defn['name'] not in seen_target:
        return snippet
    return ''

def is_platform_independent(defn):
    if 'enable' in defn:
        return False
    for suffix in [ "", "_nodist" ]:
        template = platform_values(defn, GRUB_PLATFORMS[0], suffix)
        for platform in GRUB_PLATFORMS[1:]:
            if template != platform_values(defn, platform, suffix):
                return False

    for suffix in [ "startup", "ldadd", "dependencies", "cflags", "ldflags", "cppflags", "ccasflags", "stripflags", "objcopyflags", "condition" ]:
        template = platform_specific_values(defn, GRUB_PLATFORMS[0], "_" + suffix, suffix)
        for platform in GRUB_PLATFORMS[1:]:
            if template != platform_specific_values(defn, platform, "_" + suffix, suffix):
                return False
    for tag in [ "nostrip" ]:
        template = platform_tagged(defn, GRUB_PLATFORMS[0], tag)
        for platform in GRUB_PLATFORMS[1:]:
            if template != platform_tagged(defn, platform, tag):
                return False

    return True

def module(defn, platform):
    name = defn['name']
    set_canonical_name_suffix(".module")

    gvar_add("platform_PROGRAMS", name + ".module")
    gvar_add("MODULE_FILES", name + ".module$(EXEEXT)")

    var_set(cname(defn) + "_SOURCES", platform_sources(defn, platform) + " ## platform sources")
    var_set("nodist_" + cname(defn) + "_SOURCES", platform_nodist_sources(defn, platform) + " ## platform nodist sources")
    var_set(cname(defn) + "_LDADD", platform_ldadd(defn, platform))
    var_set(cname(defn) + "_CFLAGS", "$(AM_CFLAGS) $(CFLAGS_MODULE) " + platform_cflags(defn, platform))
    var_set(cname(defn) + "_LDFLAGS", "$(AM_LDFLAGS) $(LDFLAGS_MODULE) " + platform_ldflags(defn, platform))
    var_set(cname(defn) + "_CPPFLAGS", "$(AM_CPPFLAGS) $(CPPFLAGS_MODULE) " + platform_cppflags(defn, platform))
    var_set(cname(defn) + "_CCASFLAGS", "$(AM_CCASFLAGS) $(CCASFLAGS_MODULE) " + platform_ccasflags(defn, platform))
    var_set(cname(defn) + "_DEPENDENCIES", "$(TARGET_OBJ2ELF) " + platform_dependencies(defn, platform))

    gvar_add("dist_noinst_DATA", extra_dist(defn))
    gvar_add("BUILT_SOURCES", "$(nodist_" + cname(defn) + "_SOURCES)")
    gvar_add("CLEANFILES", "$(nodist_" + cname(defn) + "_SOURCES)")

    gvar_add("MOD_FILES", name + ".mod")
    gvar_add("MARKER_FILES", name + ".marker")
    gvar_add("CLEANFILES", name + ".marker")
    output("""
""" + name + """.marker: $(""" + cname(defn) + """_SOURCES) $(nodist_""" + cname(defn) + """_SOURCES)
	$(TARGET_CPP) -DGRUB_LST_GENERATOR $(CPPFLAGS_MARKER) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(""" + cname(defn) + """_CPPFLAGS) $(CPPFLAGS) $^ > $@.new || (rm -f $@; exit 1)
	grep 'MARKER' $@.new > $@; rm -f $@.new
""")

def kernel(defn, platform):
    name = defn['name']
    set_canonical_name_suffix(".exec")
    gvar_add("platform_PROGRAMS", name + ".exec")
    var_set(cname(defn) + "_SOURCES", platform_startup(defn, platform))
    var_add(cname(defn) + "_SOURCES", platform_sources(defn, platform))
    var_set("nodist_" + cname(defn) + "_SOURCES", platform_nodist_sources(defn, platform) + " ## platform nodist sources")
    var_set(cname(defn) + "_LDADD", platform_ldadd(defn, platform))
    var_set(cname(defn) + "_CFLAGS", "$(AM_CFLAGS) $(CFLAGS_KERNEL) " + platform_cflags(defn, platform))
    var_set(cname(defn) + "_LDFLAGS", "$(AM_LDFLAGS) $(LDFLAGS_KERNEL) " + platform_ldflags(defn, platform))
    var_set(cname(defn) + "_CPPFLAGS", "$(AM_CPPFLAGS) $(CPPFLAGS_KERNEL) " + platform_cppflags(defn, platform))
    var_set(cname(defn) + "_CCASFLAGS", "$(AM_CCASFLAGS) $(CCASFLAGS_KERNEL) " + platform_ccasflags(defn, platform))
    var_set(cname(defn) + "_STRIPFLAGS", "$(AM_STRIPFLAGS) $(STRIPFLAGS_KERNEL) " + platform_stripflags(defn, platform))
    var_set(cname(defn) + "_DEPENDENCIES", "$(TARGET_OBJ2ELF)")

    gvar_add("dist_noinst_DATA", extra_dist(defn))
    gvar_add("BUILT_SOURCES", "$(nodist_" + cname(defn) + "_SOURCES)")
    gvar_add("CLEANFILES", "$(nodist_" + cname(defn) + "_SOURCES)")

    gvar_add("platform_DATA", name + ".img")
    gvar_add("CLEANFILES", name + ".img")
    rule(name + ".img", name + ".exec$(EXEEXT)",
         if_platform_tagged(defn, platform, "nostrip",
"""if test x$(TARGET_APPLE_LINKER) = x1; then \
     $(TARGET_OBJCONV) -f$(TARGET_MODULE_FORMAT) -nr:_grub_mod_init:grub_mod_init -nr:_grub_mod_fini:grub_mod_fini -ed2022 -wd1106 -nu -nd $< $@; \
   elif test ! -z '$(TARGET_OBJ2ELF)'; then \
     $(TARGET_OBJ2ELF) $< $@ || (rm -f $@; exit 1); \
   else cp $< $@; fi""",
"""if test x$(TARGET_APPLE_LINKER) = x1; then \
  $(TARGET_STRIP) -S -x $(""" + cname(defn) + """) -o $@.bin $<; \
  $(TARGET_OBJCONV) -f$(TARGET_MODULE_FORMAT) -nr:_grub_mod_init:grub_mod_init -nr:_grub_mod_fini:grub_mod_fini -ed2022 -ed2016 -wd1106 -nu -nd $@.bin $@; \
  rm -f $@.bin; \
   elif test ! -z '$(TARGET_OBJ2ELF)'; then \
     """  + "$(TARGET_STRIP) $(" + cname(defn) + "_STRIPFLAGS) -o $@.bin $< && \
     $(TARGET_OBJ2ELF) $@.bin $@ || (rm -f $@; rm -f $@.bin; exit 1); \
     rm -f $@.bin; \
else """  + "$(TARGET_STRIP) $(" + cname(defn) + "_STRIPFLAGS) -o $@ $<; \
fi"""))

def image(defn, platform):
    name = defn['name']
    set_canonical_name_suffix(".image")
    gvar_add("platform_PROGRAMS", name + ".image")
    var_set(cname(defn) + "_SOURCES", platform_sources(defn, platform))
    var_set("nodist_" + cname(defn) + "_SOURCES", platform_nodist_sources(defn, platform) + "## platform nodist sources")
    var_set(cname(defn) + "_LDADD", platform_ldadd(defn, platform))
    var_set(cname(defn) + "_CFLAGS", "$(AM_CFLAGS) $(CFLAGS_IMAGE) " + platform_cflags(defn, platform))
    var_set(cname(defn) + "_LDFLAGS", "$(AM_LDFLAGS) $(LDFLAGS_IMAGE) " + platform_ldflags(defn, platform))
    var_set(cname(defn) + "_CPPFLAGS", "$(AM_CPPFLAGS) $(CPPFLAGS_IMAGE) " + platform_cppflags(defn, platform))
    var_set(cname(defn) + "_CCASFLAGS", "$(AM_CCASFLAGS) $(CCASFLAGS_IMAGE) " + platform_ccasflags(defn, platform))
    var_set(cname(defn) + "_OBJCOPYFLAGS", "$(OBJCOPYFLAGS_IMAGE) " + platform_objcopyflags(defn, platform))
    # var_set(cname(defn) + "_DEPENDENCIES", platform_dependencies(defn, platform) + " " + platform_ldadd(defn, platform))

    gvar_add("dist_noinst_DATA", extra_dist(defn))
    gvar_add("BUILT_SOURCES", "$(nodist_" + cname(defn) + "_SOURCES)")
    gvar_add("CLEANFILES", "$(nodist_" + cname(defn) + "_SOURCES)")

    gvar_add("platform_DATA", name + ".img")
    gvar_add("CLEANFILES", name + ".img")
    rule(name + ".img", name + ".image$(EXEEXT)", """
if test x$(TARGET_APPLE_LINKER) = x1; then \
  $(MACHO2IMG) $< $@; \
else \
  $(TARGET_OBJCOPY) $(""" + cname(defn) + """_OBJCOPYFLAGS) --strip-unneeded -R .note -R .comment -R .note.gnu.build-id -R .MIPS.abiflags -R .reginfo -R .rel.dyn -R .note.gnu.gold-version -R .ARM.exidx $< $@; \
fi
""")

def library(defn, platform):
    name = defn['name']
    set_canonical_name_suffix("")

    vars_init(defn,
              cname(defn) + "_SOURCES",
              "nodist_" + cname(defn) + "_SOURCES",
              cname(defn) + "_CFLAGS",
              cname(defn) + "_CPPFLAGS",
              cname(defn) + "_CCASFLAGS")
    #         cname(defn) + "_DEPENDENCIES")

    if name not in seen_target:
        gvar_add("noinst_LIBRARIES", name)
    var_add(cname(defn) + "_SOURCES", platform_sources(defn, platform))
    var_add("nodist_" + cname(defn) + "_SOURCES", platform_nodist_sources(defn, platform))
    var_add(cname(defn) + "_CFLAGS", first_time(defn, "$(AM_CFLAGS) $(CFLAGS_LIBRARY) ") + platform_cflags(defn, platform))
    var_add(cname(defn) + "_CPPFLAGS", first_time(defn, "$(AM_CPPFLAGS) $(CPPFLAGS_LIBRARY) ") + platform_cppflags(defn, platform))
    var_add(cname(defn) + "_CCASFLAGS", first_time(defn, "$(AM_CCASFLAGS) $(CCASFLAGS_LIBRARY) ") + platform_ccasflags(defn, platform))
    # var_add(cname(defn) + "_DEPENDENCIES", platform_dependencies(defn, platform) + " " + platform_ldadd(defn, platform))

    gvar_add("dist_noinst_DATA", extra_dist(defn))
    if name not in seen_target:
        gvar_add("BUILT_SOURCES", "$(nodist_" + cname(defn) + "_SOURCES)")
        gvar_add("CLEANFILES", "$(nodist_" + cname(defn) + "_SOURCES)")

def installdir(defn, default="bin"):
    return defn.get('installdir', default)

def manpage(defn, adddeps):
    name = defn['name']
    mansection = defn['mansection']

    output("if COND_MAN_PAGES\n")
    gvar_add("man_MANS", name + "." + mansection)
    rule(name + "." + mansection, name + " " + adddeps, """
chmod a+x """ + name + """
PATH=$(builddir):$$PATH pkgdatadir=$(builddir) $(HELP2MAN) --section=""" + mansection + """ -i $(top_srcdir)/docs/man/""" + name + """.h2m -o $@ """ + name + """
""")
    gvar_add("CLEANFILES", name + "." + mansection)
    output("endif\n")

def program(defn, platform, test=False):
    name = defn['name']
    set_canonical_name_suffix("")

    if 'testcase' in defn:
        gvar_add("check_PROGRAMS", name)
        gvar_add("TESTS", name)
    else:
        var_add(installdir(defn) + "_PROGRAMS", name)
        if 'mansection' in defn:
            manpage(defn, "")

    var_set(cname(defn) + "_SOURCES", platform_sources(defn, platform))
    var_set("nodist_" + cname(defn) + "_SOURCES", platform_nodist_sources(defn, platform))
    var_set(cname(defn) + "_LDADD", platform_ldadd(defn, platform))
    var_set(cname(defn) + "_CFLAGS", "$(AM_CFLAGS) $(CFLAGS_PROGRAM) " + platform_cflags(defn, platform))
    var_set(cname(defn) + "_LDFLAGS", "$(AM_LDFLAGS) $(LDFLAGS_PROGRAM) " + platform_ldflags(defn, platform))
    var_set(cname(defn) + "_CPPFLAGS", "$(AM_CPPFLAGS) $(CPPFLAGS_PROGRAM) " + platform_cppflags(defn, platform))
    var_set(cname(defn) + "_CCASFLAGS", "$(AM_CCASFLAGS) $(CCASFLAGS_PROGRAM) " + platform_ccasflags(defn, platform))
    # var_set(cname(defn) + "_DEPENDENCIES", platform_dependencies(defn, platform) + " " + platform_ldadd(defn, platform))

    gvar_add("dist_noinst_DATA", extra_dist(defn))
    gvar_add("BUILT_SOURCES", "$(nodist_" + cname(defn) + "_SOURCES)")
    gvar_add("CLEANFILES", "$(nodist_" + cname(defn) + "_SOURCES)")

def data(defn, platform):
    var_add("dist_" + installdir(defn) + "_DATA", platform_sources(defn, platform))
    gvar_add("dist_noinst_DATA", extra_dist(defn))

def transform_data(defn, platform):
    name = defn['name']

    var_add(installdir(defn) + "_DATA", name)

    rule(name, "$(top_builddir)/config.status " + platform_sources(defn, platform) + platform_dependencies(defn, platform), """
(for x in """ + platform_sources(defn, platform) + """; do cat $(srcdir)/"$$x"; done) | $(top_builddir)/config.status --file=$@:-
chmod a+x """ + name + """
""")

    gvar_add("CLEANFILES", name)
    gvar_add("EXTRA_DIST", extra_dist(defn))
    gvar_add("dist_noinst_DATA", platform_sources(defn, platform))

def script(defn, platform):
    name = defn['name']

    if 'testcase' in defn:
        gvar_add("check_SCRIPTS", name)
        gvar_add ("TESTS", name)
    else:
        var_add(installdir(defn) + "_SCRIPTS", name)
        if 'mansection' in defn:
            manpage(defn, "grub-mkconfig_lib")

    rule(name, "$(top_builddir)/config.status " + platform_sources(defn, platform) + platform_dependencies(defn, platform), """
(for x in """ + platform_sources(defn, platform) + """; do cat $(srcdir)/"$$x"; done) | $(top_builddir)/config.status --file=$@:-
chmod a+x """ + name + """
""")

    gvar_add("CLEANFILES", name)
    gvar_add("EXTRA_DIST", extra_dist(defn))
    gvar_add("dist_noinst_DATA", platform_sources(defn, platform))

def rules(target, closure):
    seen_target.clear()
    seen_vars.clear()

    for defn in defparser.definitions.find_all(target):
        if is_platform_independent(defn):
            under_platform_specific_conditionals(defn, GRUB_PLATFORMS[0], closure)
        else:
            foreach_enabled_platform(
                defn,
                lambda p: under_platform_specific_conditionals(defn, p, closure))
        # Remember that we've seen this target.
        seen_target.add(defn['name'])

parser = OptionParser(usage="%prog DEFINITION-FILES")
_, args = parser.parse_args()

for arg in args:
    defparser.read_definitions(arg)

rules("module", module)
rules("kernel", kernel)
rules("image", image)
rules("library", library)
rules("program", program)
rules("script", script)
rules("data", data)
rules("transform_data", transform_data)

write_output(section='decl')
write_output()
