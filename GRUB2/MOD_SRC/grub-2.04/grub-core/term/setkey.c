/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/term.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define MAX_KEYMAP 255

struct keymap
{
  int cnt;
  int in[MAX_KEYMAP];
  int out[MAX_KEYMAP];
};

static struct keymap setkey_keymap;

struct keysym
{
  const char *name; /* the name in unshifted state */
  int code;   /* scan code */
};

/* The table for key symbols. (from GRUB4DOS) */
static struct keysym keysym_table[] =
{
  {"escape",        GRUB_TERM_ESC},    // ESC
  {"exclam",        0x21},    //    '!'
  {"at",            0x40},    //    '@'
  {"numbersign",    0x23},    //    '#'
  {"dollar",        0x24},    //    '$'
  {"percent",       0x25},    //    '%'
  {"caret",         0x5E},    //    '^'
  {"ampersand",     0x26},    //    '&'
  {"asterisk",      0x2A},    //    '*'
  {"parenleft",     0x28},    //    '('
  {"parenright",    0x29},    //    ')'
  {"minus",         0x2D},    //    '-'
  {"underscore",    0x5F},    //    '_'
  {"equal",         0x3D},    //    '='
  {"plus",          0x2B},    //    '+'
  {"backspace",     GRUB_TERM_BACKSPACE},    // BS
  {"ctrlbackspace", GRUB_TERM_CTRL | GRUB_TERM_BACKSPACE}, // (DEL)
  {"tab",           GRUB_TERM_TAB},    // Tab
  {"bracketleft",   0x5B},    // '['
  {"braceleft",     0x7B},    //    '{'
  {"bracketright",  0x5D},    // ']'
  {"braceright",    0x7D},    //    '}'
  {"enter",         0x0D},    // Enter
  {"semicolon",     0x3B},    // ';'
  {"colon",         0x3A},    //    ':'
  {"quote",         0x27},    // '\''
  {"doublequote",   0x22},    //    '"'
  {"backquote",     0x60},    // '`'
  {"tilde",         0x7E},    //    '~'
  {"backslash",     0x5C},    // '\\'
  {"bar",           0x7C},    //    '|'
  {"comma",         0x2C},    // ','
  {"less",          0x3C},    //    '<'
  {"period",        0x2E},    // '.'
  {"greater",       0x3E},    //    '>'
  {"slash",         0x2F},    // '/'
  {"question",      0x3F},    //    '?'
  {"space",         0x20},    // Space
  {"F1",            GRUB_TERM_KEY_F1},
  {"F2",            GRUB_TERM_KEY_F2},
  {"F3",            GRUB_TERM_KEY_F3},
  {"F4",            GRUB_TERM_KEY_F4},
  {"F5",            GRUB_TERM_KEY_F5},
  {"F6",            GRUB_TERM_KEY_F6},
  {"F7",            GRUB_TERM_KEY_F7},
  {"F8",            GRUB_TERM_KEY_F8},
  {"F9",            GRUB_TERM_KEY_F9},
  {"F10",           GRUB_TERM_KEY_F10},
  {"F11",           GRUB_TERM_KEY_F11},
  {"F12",           GRUB_TERM_KEY_F12},
  {"home",          GRUB_TERM_KEY_HOME},
  {"uparrow",       GRUB_TERM_KEY_UP},
  {"pageup",        GRUB_TERM_KEY_NPAGE},    // PgUp
  {"leftarrow",     GRUB_TERM_KEY_LEFT},
  {"center",        GRUB_TERM_KEY_CENTER},    // keypad center key
  {"rightarrow",    GRUB_TERM_KEY_RIGHT},
  {"end",           GRUB_TERM_KEY_END},
  {"downarrow",     GRUB_TERM_KEY_DOWN},
  {"pagedown",      GRUB_TERM_KEY_PPAGE},    // PgDn
  {"insert",        GRUB_TERM_KEY_INSERT},    // Insert
  {"delete",        GRUB_TERM_KEY_DC},    // Delete
  {"shiftF1",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F1},
  {"shiftF2",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F2},
  {"shiftF3",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F3},
  {"shiftF4",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F4},
  {"shiftF5",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F5},
  {"shiftF6",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F6},
  {"shiftF7",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F7},
  {"shiftF8",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F8},
  {"shiftF9",       GRUB_TERM_SHIFT | GRUB_TERM_KEY_F9},
  {"shiftF10",      GRUB_TERM_SHIFT | GRUB_TERM_KEY_F10},
  {"shiftF11",      GRUB_TERM_SHIFT | GRUB_TERM_KEY_F11},
  {"shiftF12",      GRUB_TERM_SHIFT | GRUB_TERM_KEY_F12},
  {"ctrlF1",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F1},
  {"ctrlF2",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F2},
  {"ctrlF3",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F3},
  {"ctrlF4",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F4},
  {"ctrlF5",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F5},
  {"ctrlF6",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F6},
  {"ctrlF7",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F7},
  {"ctrlF8",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F8},
  {"ctrlF9",        GRUB_TERM_CTRL | GRUB_TERM_KEY_F9},
  {"ctrlF10",       GRUB_TERM_CTRL | GRUB_TERM_KEY_F10},
  {"ctrlF11",       GRUB_TERM_CTRL | GRUB_TERM_KEY_F11},
  {"ctrlF12",       GRUB_TERM_CTRL | GRUB_TERM_KEY_F12},
  // A=Alt or AltGr.    Provided by steve.
  {"Aq",            GRUB_TERM_ALT | 0x71},
  {"Aw",            GRUB_TERM_ALT | 0x77},
  {"Ae",            GRUB_TERM_ALT | 0x65},
  {"Ar",            GRUB_TERM_ALT | 0x72},
  {"At",            GRUB_TERM_ALT | 0x74},
  {"Ay",            GRUB_TERM_ALT | 0x79},
  {"Au",            GRUB_TERM_ALT | 0x75},
  {"Ai",            GRUB_TERM_ALT | 0x69},
  {"Ao",            GRUB_TERM_ALT | 0x6F},
  {"Ap",            GRUB_TERM_ALT | 0x70},
  {"Aa",            GRUB_TERM_ALT | 0x61},
  {"As",            GRUB_TERM_ALT | 0x73},
  {"Ad",            GRUB_TERM_ALT | 0x64},
  {"Af",            GRUB_TERM_ALT | 0x66},
  {"Ag",            GRUB_TERM_ALT | 0x67},
  {"Ah",            GRUB_TERM_ALT | 0x68},
  {"Aj",            GRUB_TERM_ALT | 0x6A},
  {"Ak",            GRUB_TERM_ALT | 0x6B},
  {"Al",            GRUB_TERM_ALT | 0x6C},
  {"Az",            GRUB_TERM_ALT | 0x7A},
  {"Ax",            GRUB_TERM_ALT | 0x78},
  {"Ac",            GRUB_TERM_ALT | 0x63},
  {"Av",            GRUB_TERM_ALT | 0x76},
  {"Ab",            GRUB_TERM_ALT | 0x62},
  {"An",            GRUB_TERM_ALT | 0x6E},
  {"Am",            GRUB_TERM_ALT | 0x6D},
  {"A1",            GRUB_TERM_ALT | 0x31},
  {"A2",            GRUB_TERM_ALT | 0x32},
  {"A3",            GRUB_TERM_ALT | 0x33},
  {"A4",            GRUB_TERM_ALT | 0x34},
  {"A5",            GRUB_TERM_ALT | 0x35},
  {"A6",            GRUB_TERM_ALT | 0x36},
  {"A7",            GRUB_TERM_ALT | 0x37},
  {"A8",            GRUB_TERM_ALT | 0x38},
  {"A9",            GRUB_TERM_ALT | 0x39},
  {"A0",            GRUB_TERM_ALT | 0x30},
  //{"oem102",        0x5c},
  //{"shiftoem102",   0x7c},
  {"Aminus",        GRUB_TERM_ALT | 0x2D},
  {"Aequal",        GRUB_TERM_ALT | 0x3D},
  {"Abracketleft",  GRUB_TERM_ALT | 0x5B},
  {"Abracketright", GRUB_TERM_ALT | 0x5D},
  {"Asemicolon",    GRUB_TERM_ALT | 0x3B},
  {"Aquote",        GRUB_TERM_ALT | 0x27},
  {"Abackquote",    GRUB_TERM_ALT | 0x60},
  {"Abackslash",    GRUB_TERM_ALT | 0x5C},
  {"Acomma",        GRUB_TERM_ALT | 0x2C},
  {"Aperiod",       GRUB_TERM_ALT | 0x2E},
  {"Aslash",        GRUB_TERM_ALT | 0x2F},
  {"Acolon",        GRUB_TERM_ALT | 0x3A},
  {"Aplus",         GRUB_TERM_ALT | 0x2B},
  {"Aless",         GRUB_TERM_ALT | 0x3C},
  {"Aunderscore",   GRUB_TERM_ALT | 0x5F},
  {"Agreater",      GRUB_TERM_ALT | 0x3E},
  {"Aquestion",     GRUB_TERM_ALT | 0x3F},
  {"Atilde",        GRUB_TERM_ALT | 0x7E},
  {"Abraceleft",    GRUB_TERM_ALT | 0x7B},
  {"Abar",          GRUB_TERM_ALT | 0x7C},
  {"Abraceright",   GRUB_TERM_ALT | 0x7D},
  {"Adoublequote",  GRUB_TERM_ALT | 0x22},
};

static int grub_keymap_getkey (int key)
{
  int i;
  if (key == GRUB_TERM_NO_KEY)
    return key;
  if (setkey_keymap.cnt > MAX_KEYMAP)
    setkey_keymap.cnt = MAX_KEYMAP;
  for (i = 0; i < setkey_keymap.cnt; i++)
  {
    if (key == setkey_keymap.in[i])
    {
      key = setkey_keymap.out[i];
      break;
    }
  }
  return key;
}

static void
grub_keymap_reset (void)
{
  grub_memset (&setkey_keymap, 0, sizeof (struct keymap));
}

static grub_err_t
grub_keymap_add (int in, int out)
{
  if (in == GRUB_TERM_NO_KEY || out == GRUB_TERM_NO_KEY)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid key: %d -> %d", in, out);
  if (setkey_keymap.cnt >= MAX_KEYMAP)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY,
                       "keymap FULL %d", setkey_keymap.cnt);
  setkey_keymap.in[setkey_keymap.cnt] = in;
  setkey_keymap.out[setkey_keymap.cnt] = out;
  setkey_keymap.cnt++;
  return GRUB_ERR_NONE;
}

static void
grub_keymap_enable (void)
{
  grub_key_remap = grub_keymap_getkey;
}

static void
grub_keymap_disable (void)
{
  grub_key_remap = NULL;
}

static void
grub_keymap_status (void)
{
  int i;
  if (setkey_keymap.cnt > MAX_KEYMAP)
    setkey_keymap.cnt = MAX_KEYMAP;
  for (i = 0; i < setkey_keymap.cnt; i++)
  {
    grub_printf ("0x%x -> 0x%x\n", setkey_keymap.in[i], setkey_keymap.out[i]);
  }
}

static const struct grub_arg_option options[] =
{
  {"reset", 'r', 0, N_("Reset keymap."), 0, 0},
  {"enable", 'e', 0, N_("Enable keymap."), 0, 0},
  {"disable", 'd', 0, N_("Disable keymap."), 0, 0},
  {"status", 's', 0, N_("Display keymap."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

enum options
{
  SETKEY_RESET,
  SETKEY_ENABLE,
  SETKEY_DISABLE,
  SETKEY_STATUS,
};

static int
ishex (const char *str)
{
  if (grub_strlen (str) < 3 || str[0] != '0')
    return 0;
  if (str[1] != 'x' && str[1] != 'X')
    return 0;
  return 1;
}

static int
parse_key (const char *str)
{
  int i;
  if (ishex (str))
    return grub_strtol (str, NULL, 16);
  if (grub_strlen (str) == 1)
    return (int) str[0];
  for (i = 0; i < (int) (sizeof (keysym_table) / sizeof (keysym_table[0])); i++)
  {
    if (grub_strcmp (str, keysym_table[i].name) == 0)
      return keysym_table[i].code;
  }
  grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid key %s", str);
  return 0;
}

static grub_err_t
grub_cmd_setkey (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  int in, out;
  if (state[SETKEY_ENABLE].set)
  {
    grub_keymap_enable ();
    goto out;
  }
  if (state[SETKEY_DISABLE].set)
  {
    grub_keymap_disable ();
    goto out;
  }
  if (state[SETKEY_RESET].set)
  {
    grub_keymap_reset ();
    goto out;
  }
  if (state[SETKEY_STATUS].set)
  {
    grub_keymap_status ();
    goto out;
  }
  if (argc != 2)
  {
    grub_printf
      ("Key names: 0-9, A-Z, a-z or escape, exclam, at, numbersign, dollar,"
       "percent, caret, ampersand, asterisk, parenleft, parenright, minus,"
       "underscore, equal, plus, backspace, tab, bracketleft, braceleft,"
       "bracketright, braceright, enter, semicolon, colon, quote, doublequote,"
       "backquote, tilde, backslash, bar, comma, less, period, greater,"
       "slash, question, alt, space, delete, [ctrl|shift]F1-12."
       "For Alt+ prefix with A, e.g. \'setkey at Aequal\'.");
    goto out;
  }
  in = parse_key (args[1]);
  out = parse_key (args[0]);
  if (!in || !out)
    goto out;
  grub_keymap_add (in, out);
out:
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(setkey)
{
  cmd = grub_register_extcmd ("setkey", grub_cmd_setkey, 0, N_("NEW_KEY USA_KEY"),
                              N_("Map default USA_KEY to NEW_KEY."), options);
}

GRUB_MOD_FINI(setkey)
{
  grub_unregister_extcmd (cmd);
}
