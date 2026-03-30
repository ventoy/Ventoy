#!/usr/bin/env python3

from __future__ import annotations

import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
INSTALL_GRUB = ROOT.parent / "INSTALL" / "grub"
LAYOUTS_DIR = INSTALL_GRUB / "layouts"

MAGIC = b"GRUBLAYO"
VERSION = 10
ARRAY_SIZE = 160

GRUB_TERM_SHIFT = 0x01000000
GRUB_TERM_EXTENDED = 0x00800000
GRUB_TERM_KEY_LEFT = GRUB_TERM_EXTENDED | 0x4B
GRUB_TERM_KEY_RIGHT = GRUB_TERM_EXTENDED | 0x4D
GRUB_TERM_KEY_UP = GRUB_TERM_EXTENDED | 0x48
GRUB_TERM_KEY_DOWN = GRUB_TERM_EXTENDED | 0x50
GRUB_TERM_KEY_HOME = GRUB_TERM_EXTENDED | 0x47
GRUB_TERM_KEY_END = GRUB_TERM_EXTENDED | 0x4F
GRUB_TERM_KEY_DC = GRUB_TERM_EXTENDED | 0x53
GRUB_TERM_KEY_PPAGE = GRUB_TERM_EXTENDED | 0x49
GRUB_TERM_KEY_NPAGE = GRUB_TERM_EXTENDED | 0x51
GRUB_TERM_KEY_F1 = GRUB_TERM_EXTENDED | 0x3B
GRUB_TERM_KEY_F2 = GRUB_TERM_EXTENDED | 0x3C
GRUB_TERM_KEY_F3 = GRUB_TERM_EXTENDED | 0x3D
GRUB_TERM_KEY_F4 = GRUB_TERM_EXTENDED | 0x3E
GRUB_TERM_KEY_F5 = GRUB_TERM_EXTENDED | 0x3F
GRUB_TERM_KEY_F6 = GRUB_TERM_EXTENDED | 0x40
GRUB_TERM_KEY_F7 = GRUB_TERM_EXTENDED | 0x41
GRUB_TERM_KEY_F8 = GRUB_TERM_EXTENDED | 0x42
GRUB_TERM_KEY_F9 = GRUB_TERM_EXTENDED | 0x43
GRUB_TERM_KEY_F10 = GRUB_TERM_EXTENDED | 0x44
GRUB_TERM_KEY_F11 = GRUB_TERM_EXTENDED | 0x57
GRUB_TERM_KEY_F12 = GRUB_TERM_EXTENDED | 0x58
GRUB_TERM_KEY_INSERT = GRUB_TERM_EXTENDED | 0x52
GRUB_TERM_KEY_CENTER = GRUB_TERM_EXTENDED | 0x4C
GRUB_TERM_ESC = 0x1B
GRUB_TERM_TAB = ord("\t")
GRUB_TERM_BACKSPACE = ord("\b")

TOKENS = {
    "ampersand": ord("&"),
    "asterisk": ord("*"),
    "at": ord("@"),
    "backquote": ord("`"),
    "backslash": ord("\\"),
    "bar": ord("|"),
    "braceleft": ord("{"),
    "braceright": ord("}"),
    "bracketleft": ord("["),
    "bracketright": ord("]"),
    "caret": ord("^"),
    "colon": ord(":"),
    "comma": ord(","),
    "dollar": ord("$"),
    "doublequote": ord('"'),
    "equal": ord("="),
    "exclam": ord("!"),
    "greater": ord(">"),
    "less": ord("<"),
    "minus": ord("-"),
    "numbersign": ord("#"),
    "parenleft": ord("("),
    "parenright": ord(")"),
    "percent": ord("%"),
    "period": ord("."),
    "plus": ord("+"),
    "question": ord("?"),
    "quote": ord("'"),
    "semicolon": ord(";"),
    "slash": ord("/"),
    "tilde": ord("~"),
    "underscore": ord("_"),
}


def parse_symbol(token: str) -> int:
    if len(token) == 1:
        return ord(token)
    if token in TOKENS:
        return TOKENS[token]
    raise ValueError(f"unsupported key token: {token}")


def base_layout() -> tuple[list[int], list[int]]:
    unshift = [0] * ARRAY_SIZE
    shift = [0] * ARRAY_SIZE

    for offset, ch in enumerate("abcdefghijklmnopqrstuvwxyz", start=0x04):
        unshift[offset] = ord(ch)
        shift[offset] = ord(ch.upper())

    shifted_digits = "!@#$%^&*()"
    for offset, (plain, shifted) in enumerate(zip("1234567890", shifted_digits), start=0x1E):
        unshift[offset] = ord(plain)
        shift[offset] = ord(shifted)

    plain_map = {
        0x28: ord("\n"),
        0x29: GRUB_TERM_ESC,
        0x2A: GRUB_TERM_BACKSPACE,
        0x2B: GRUB_TERM_TAB,
        0x2C: ord(" "),
        0x2D: ord("-"),
        0x2E: ord("="),
        0x2F: ord("["),
        0x30: ord("]"),
        0x31: 0,
        0x32: ord("\\"),
        0x33: ord(";"),
        0x34: ord("'"),
        0x35: ord("`"),
        0x36: ord(","),
        0x37: ord("."),
        0x38: ord("/"),
        0x39: 0,
        0x3A: GRUB_TERM_KEY_F1,
        0x3B: GRUB_TERM_KEY_F2,
        0x3C: GRUB_TERM_KEY_F3,
        0x3D: GRUB_TERM_KEY_F4,
        0x3E: GRUB_TERM_KEY_F5,
        0x3F: GRUB_TERM_KEY_F6,
        0x40: GRUB_TERM_KEY_F7,
        0x41: GRUB_TERM_KEY_F8,
        0x42: GRUB_TERM_KEY_F9,
        0x43: GRUB_TERM_KEY_F10,
        0x44: GRUB_TERM_KEY_F11,
        0x45: GRUB_TERM_KEY_F12,
        0x46: 0,
        0x47: 0,
        0x48: 0,
        0x49: GRUB_TERM_KEY_INSERT,
        0x4A: GRUB_TERM_KEY_HOME,
        0x4B: GRUB_TERM_KEY_PPAGE,
        0x4C: GRUB_TERM_KEY_DC,
        0x4D: GRUB_TERM_KEY_END,
        0x4E: GRUB_TERM_KEY_NPAGE,
        0x4F: GRUB_TERM_KEY_RIGHT,
        0x50: GRUB_TERM_KEY_LEFT,
        0x51: GRUB_TERM_KEY_DOWN,
        0x52: GRUB_TERM_KEY_UP,
        0x53: 0,
        0x54: ord("/"),
        0x55: ord("*"),
        0x56: ord("-"),
        0x57: ord("+"),
        0x58: ord("\n"),
        0x59: GRUB_TERM_KEY_END,
        0x5A: GRUB_TERM_KEY_DOWN,
        0x5B: GRUB_TERM_KEY_NPAGE,
        0x5C: GRUB_TERM_KEY_LEFT,
        0x5D: GRUB_TERM_KEY_CENTER,
        0x5E: GRUB_TERM_KEY_RIGHT,
        0x5F: GRUB_TERM_KEY_HOME,
        0x60: GRUB_TERM_KEY_UP,
        0x61: GRUB_TERM_KEY_PPAGE,
        0x62: GRUB_TERM_KEY_INSERT,
        0x63: GRUB_TERM_KEY_DC,
        0x64: ord("\\"),
    }

    shift_map = {
        0x28: GRUB_TERM_SHIFT | ord("\n"),
        0x29: GRUB_TERM_SHIFT | GRUB_TERM_ESC,
        0x2A: GRUB_TERM_SHIFT | GRUB_TERM_BACKSPACE,
        0x2B: GRUB_TERM_SHIFT | GRUB_TERM_TAB,
        0x2C: GRUB_TERM_SHIFT | ord(" "),
        0x2D: ord("_"),
        0x2E: ord("+"),
        0x2F: ord("{"),
        0x30: ord("}"),
        0x31: 0,
        0x32: ord("|"),
        0x33: ord(":"),
        0x34: ord('"'),
        0x35: ord("~"),
        0x36: ord("<"),
        0x37: ord(">"),
        0x38: ord("?"),
        0x39: 0,
        0x3A: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F1,
        0x3B: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F2,
        0x3C: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F3,
        0x3D: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F4,
        0x3E: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F5,
        0x3F: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F6,
        0x40: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F7,
        0x41: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F8,
        0x42: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F9,
        0x43: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F10,
        0x44: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F11,
        0x45: GRUB_TERM_SHIFT | GRUB_TERM_KEY_F12,
        0x46: 0,
        0x47: 0,
        0x48: 0,
        0x49: GRUB_TERM_SHIFT | GRUB_TERM_KEY_INSERT,
        0x4A: GRUB_TERM_SHIFT | GRUB_TERM_KEY_HOME,
        0x4B: GRUB_TERM_SHIFT | GRUB_TERM_KEY_PPAGE,
        0x4C: GRUB_TERM_SHIFT | GRUB_TERM_KEY_DC,
        0x4D: GRUB_TERM_SHIFT | GRUB_TERM_KEY_END,
        0x4E: GRUB_TERM_SHIFT | GRUB_TERM_KEY_NPAGE,
        0x4F: GRUB_TERM_SHIFT | GRUB_TERM_KEY_RIGHT,
        0x50: GRUB_TERM_SHIFT | GRUB_TERM_KEY_LEFT,
        0x51: GRUB_TERM_SHIFT | GRUB_TERM_KEY_DOWN,
        0x52: GRUB_TERM_SHIFT | GRUB_TERM_KEY_UP,
        0x53: 0,
        0x54: ord("/"),
        0x55: ord("*"),
        0x56: ord("-"),
        0x57: ord("+"),
        0x58: GRUB_TERM_SHIFT | ord("\n"),
        0x59: ord("1"),
        0x5A: ord("2"),
        0x5B: ord("3"),
        0x5C: ord("4"),
        0x5D: ord("5"),
        0x5E: ord("6"),
        0x5F: ord("7"),
        0x60: ord("8"),
        0x61: ord("9"),
        0x62: ord("0"),
        0x63: ord("."),
        0x64: ord("|"),
    }

    for index, value in plain_map.items():
        unshift[index] = value
    for index, value in shift_map.items():
        shift[index] = value

    return unshift, shift


def parse_cfg(path: Path, base_map: list[int], base_shift: list[int]) -> tuple[list[int], list[int], list[int], list[int]]:
    plain_map: dict[int, int] = {}
    l3_map = [0] * ARRAY_SIZE
    shift_l3_map = [0] * ARRAY_SIZE

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith("if ") or line == "fi" or line.startswith("echo ") or line.startswith("sleep "):
            continue
        if line in ("setkey -r", "setkey -e", "setkey -d"):
            continue
        if not line.startswith("setkey "):
            continue

        parts = line.split()
        if len(parts) != 3:
            raise ValueError(f"unexpected setkey syntax in {path}: {line}")

        dst_token, src_token = parts[1], parts[2]
        dst_code = parse_symbol(dst_token)

        if src_token.startswith("A") and len(src_token) > 1:
            src_code = parse_symbol(src_token[1:])
            hits = 0
            for idx, value in enumerate(base_map):
                if value == src_code:
                    l3_map[idx] = dst_code
                    hits += 1
            for idx, value in enumerate(base_shift):
                if value == src_code:
                    shift_l3_map[idx] = dst_code
                    hits += 1
            if hits == 0:
                raise ValueError(f"unmatched AltGr source token {src_token} in {path}")
        else:
            plain_map[parse_symbol(src_token)] = dst_code

    def remap(values: list[int]) -> list[int]:
        return [plain_map.get(value, value) for value in values]

    return remap(base_map), remap(base_shift), remap(l3_map), remap(shift_l3_map)


def write_gkb(path: Path, arrays: tuple[list[int], list[int], list[int], list[int]]) -> None:
    with path.open("wb") as fp:
        fp.write(MAGIC)
        fp.write(struct.pack("<I", VERSION))
        for arr in arrays:
            fp.write(struct.pack("<" + "I" * ARRAY_SIZE, *arr))


def generate_keyboard_cfg(names: list[str]) -> None:
    lines = ['submenu "$VTLANG_KEYBRD_LAYOUT" --class=debug_krdlayout --class=F5tool {']
    for name in names:
        lines.append(f'    menuentry {name} --class=debug_kbd --class=debug_krdlayout --class=F5tool {{')
        lines.append(f"        keymap {name}")
        lines.append("    }")
    lines.append("}")
    (INSTALL_GRUB / "keyboard.cfg").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    base_map, base_shift = base_layout()
    INSTALL_GRUB.mkdir(parents=True, exist_ok=True)
    LAYOUTS_DIR.mkdir(parents=True, exist_ok=True)

    base_arrays = (base_map, base_shift, [0] * ARRAY_SIZE, [0] * ARRAY_SIZE)
    write_gkb(LAYOUTS_DIR / "QWERTY_USA.gkb", base_arrays)

    names: list[str] = ["QWERTY_USA"]
    for cfg in sorted(ROOT.glob("cfg/*.cfg")):
        name = cfg.stem.replace("KBD_", "", 1)
        names.append(name)
        arrays = parse_cfg(cfg, base_map, base_shift)
        write_gkb(LAYOUTS_DIR / f"{name}.gkb", arrays)

    generate_keyboard_cfg(names)


if __name__ == "__main__":
    main()
