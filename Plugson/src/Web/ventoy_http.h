/******************************************************************************
 * ventoy_http.h
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef __VENTOY_HTTP_H__
#define __VENTOY_HTTP_H__

#include <civetweb.h>

#define MAX_LANGUAGE  128

#define L1 "    "
#define L2 "        "
#define L3 "            "
#define L4 "                "

typedef enum bios_mode
{
    bios_common = 0,
    bios_legacy,
    bios_uefi,
    bios_ia32,
    bios_aa64,
    bios_mips,
    
    bios_max
}bios_mode;


typedef struct data_control
{
    int default_menu_mode;
    int treeview_style;
    int filter_dot_underscore;
    int sort_casesensitive;
    int max_search_level;
    int vhd_no_warning;
    int filter_iso;
    int filter_wim;
    int filter_efi;
    int filter_img;
    int filter_vhd;
    int filter_vtoy;
    int win11_bypass_check;
    int menu_timeout;
    int linux_remount;
    char default_search_root[MAX_PATH];
    char default_image[MAX_PATH];
    char default_kbd_layout[32];
    char help_text_language[32];
}data_control;

#define display_mode_gui            0
#define display_mode_cli            1
#define display_mode_serial         2
#define display_mode_ser_console    3

typedef struct path_node
{
    char path[MAX_PATH];
    struct path_node *next;
}path_node;

typedef struct data_theme
{
    int default_file;
    path_node *filelist;
    int display_mode;
    char gfxmode[32];

    char ventoy_left[32];
    char ventoy_top[32];
    char ventoy_color[32];
    char serial_param[256];
    
    path_node *fontslist;
}data_theme;

#define path_type_file 0
#define path_type_dir  1

typedef struct data_alias_node
{
    int type;
    char path[MAX_PATH];
    char alias[256];
    struct data_alias_node *next;
}data_alias_node;

typedef struct data_alias
{
    data_alias_node *list;
}data_alias;


typedef struct data_tip_node
{
    int type;
    char path[MAX_PATH];
    char tip[256];
    struct data_tip_node *next;
}data_tip_node;

typedef struct data_tip
{
    char left[32];
    char top[32];
    char color[32];
    data_tip_node *list;
}data_tip;


#define class_type_key      0
#define class_type_dir      1
#define class_type_parent   2
typedef struct data_class_node
{
    int type;
    char path[MAX_PATH];
    char class[256];
    struct data_class_node *next;
}data_class_node;

typedef struct data_class
{
    data_class_node *list;
}data_class;


typedef struct data_auto_memdisk
{
    path_node *list;
}data_auto_memdisk;

typedef struct data_image_list
{
    int type;
    path_node *list;
}data_image_list;

typedef struct menu_password
{
    int type;
    char path[MAX_PATH];
    char pwd[256];
    struct menu_password *next;
}menu_password;

typedef struct data_password
{
    char bootpwd[256];
    char isopwd[256];
    char wimpwd[256];
    char vhdpwd[256];
    char imgpwd[256];
    char efipwd[256];
    char vtoypwd[256];

    menu_password *list;
}data_password;

typedef struct conf_replace_node
{
    int image;
    char path[MAX_PATH];
    char org[256];
    char new[MAX_PATH];
    struct conf_replace_node *next;
}conf_replace_node;
typedef struct data_conf_replace
{
    conf_replace_node *list;
}data_conf_replace;

typedef struct injection_node
{
    int type;
    char path[MAX_PATH];
    char archive[MAX_PATH];
    struct injection_node *next;
}injection_node;
typedef struct data_injection
{
    injection_node *list;
}data_injection;



typedef struct dud_node
{
    char path[MAX_PATH];
    path_node *list;

    struct dud_node *next;
}dud_node;

typedef struct data_dud
{
    dud_node *list;
}data_dud;

typedef struct auto_install_node
{
    int timeouten;
    int timeout;
    int autoselen;
    int autosel;
    int type;
    char path[MAX_PATH];
    path_node *list;

    struct auto_install_node *next;
}auto_install_node;

typedef struct data_auto_install
{
    auto_install_node *list;
}data_auto_install;

typedef struct persistence_node
{
    int timeouten;
    int timeout;
    int autoselen;
    int autosel;
    int type;
    char path[MAX_PATH];
    path_node *list;

    struct persistence_node *next;
}persistence_node;

typedef struct data_persistence
{
    persistence_node *list;
}data_persistence;




#define ventoy_save_plug(plug) \
{\
    for (i = 0; i < bios_max; i++) \
    {\
        scnprintf(title, sizeof(title), "%s%s", #plug, g_json_title_postfix[i]);\
        if (ventoy_data_cmp_##plug(g_data_##plug + i, g_data_##plug + bios_max))\
        {\
            pos += ventoy_data_save_##plug(g_data_##plug + i, title, JSON_SAVE_BUFFER + pos, JSON_BUF_MAX - pos);\
        }\
    }\
}



#define api_get_func(conn, json, name) \
{\
    int i = 0; \
    int pos = 0; \
\
    (void)json;\
\
    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);\
    VTOY_JSON_FMT_ARY_BEGIN();\
\
    for (i = 0; i <= bios_max; i++)\
    {\
        __uiCurPos += ventoy_data_json_##name(g_data_##name + i, JSON_BUFFER + __uiCurPos, JSON_BUF_MAX - __uiCurPos);\
        VTOY_JSON_FMT_COMA();\
    }\
\
    VTOY_JSON_FMT_ARY_END();\
    VTOY_JSON_FMT_END(pos);\
\
    ventoy_json_buffer(conn, JSON_BUFFER, pos);\
}


#define vtoy_list_free(type, list) \
{\
    type *__next = NULL;\
    type *__node = list;\
    while (__node)\
    {\
        __next = __node->next;\
        free(__node);\
        __node = __next;\
    }\
}

#define vtoy_list_del(last, node, LIST, field) \
for (last = node = LIST; node; node = node->next) \
{\
    if (strcmp(node->field, field) == 0)\
    {\
        if (node == LIST)\
        {\
            LIST = LIST->next;\
        }\
        else\
        {\
            last->next = node->next;\
        }\
        free(node);\
        break;\
    }\
\
    last = node;\
}


#define vtoy_list_del_ex(last, node, LIST, field, cb) \
for (last = node = LIST; node; node = node->next) \
{\
    if (strcmp(node->field, field) == 0)\
    {\
        if (node == LIST)\
        {\
            LIST = LIST->next;\
        }\
        else\
        {\
            last->next = node->next;\
        }\
        cb(node->list);\
        free(node);\
        break;\
    }\
\
    last = node;\
}

#define vtoy_list_add(LIST, cur, node) \
if (LIST)\
{\
    cur = LIST;\
    while (cur && cur->next)\
    {\
        cur = cur->next;\
    }\
    cur->next = node;\
}\
else\
{\
    LIST = node;\
}



#define ventoy_parse_json(name) \
{\
    int __loop;\
    int __len = strlen(#name);\
    if (strncmp(#name, node->pcName, __len) == 0)\
    {\
        for (__loop = 0; __loop < bios_max; __loop++)\
        {\
            if (strcmp(g_json_title_postfix[__loop], node->pcName + __len) == 0)\
            {\
                vlog("json parse <%s>\n", node->pcName);\
                ventoy_parse_##name(node, g_data_##name + __loop);\
                break;\
            }\
        }\
    } \
}

#define CONTROL_PARSE_INT(node, val) \
    if (node->unData.pcStrVal[0] == '1') val = 1


#define VTOY_JSON_INT(key, val) vtoy_json_get_int(json, key, &val)
#define VTOY_JSON_STR(key, buf) vtoy_json_get_string(json, key, sizeof(buf), buf)
#define VTOY_JSON_STR_EX(key) vtoy_json_get_string_ex(json, key)

typedef int (*ventoy_json_callback)(struct mg_connection *conn, VTOY_JSON *json);
typedef struct JSON_CB
{
    const char *method;
    ventoy_json_callback callback;
}JSON_CB;

int ventoy_http_init(void);
void ventoy_http_exit(void);
int ventoy_http_start(const char *ip, const char *port);
int ventoy_http_stop(void);
int ventoy_data_save_all(void);

#endif /* __VENTOY_HTTP_H__ */

