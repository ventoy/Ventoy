/******************************************************************************
 * ventoy_gtk.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>

#include <ventoy_define.h>
#include <ventoy_json.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "ventoy_gtk.h"

int g_secure_boot_support = 0;
GtkWidget *g_topWindow = NULL;
GtkWidget *g_partCfgWindow = NULL;
GtkBuilder *g_pXmlBuilder = NULL;
GtkComboBoxText *g_dev_combobox = NULL;
GtkButton *g_refresh_button = NULL;
GtkButton *g_install_button = NULL;
GtkButton *g_update_button = NULL;
GtkMenu *g_lang_menu = NULL;;
GtkCheckMenuItem *g_menu_item_secure_boot = NULL;
GtkCheckMenuItem *g_menu_item_mbr = NULL;
GtkCheckMenuItem *g_menu_item_gpt = NULL;
GtkCheckMenuItem *g_menu_item_show_all = NULL;
GtkLabel *g_device_title = NULL;
GtkLabel *g_label_local_part_style = NULL;
GtkLabel *g_label_dev_part_style = NULL;
GtkLabel *g_label_local_ver = NULL;
GtkLabel *g_label_disk_ver = NULL;
GtkLabel *g_label_status = NULL;
GtkImage *g_image_secure_local = NULL;
GtkImage *g_image_secure_device = NULL;
GtkToggleButton *g_part_align_checkbox = NULL;
GtkToggleButton *g_part_preserve_checkbox = NULL;
GtkEntry *g_part_reserve_space_value = NULL;
GtkComboBoxText *g_part_space_unit_combox = NULL;
GtkProgressBar *g_progress_bar = NULL;
VTOY_JSON *g_languages_json = NULL;
int g_languages_toggled_proc = 0;
int g_dev_changed_proc = 0;
gboolean g_align_part_with_4k = TRUE;
gboolean g_preserve_space_check = FALSE;
int g_preserve_space_unit = 1;
int g_preserve_space_number = 0;
gboolean g_thread_run = FALSE;

const char *language_string(const char *id)
{
    const char *pName = NULL;
    VTOY_JSON *node = NULL;
    const char *pCurLang = ventoy_code_get_cur_language();

    for (node = g_languages_json->pstChild; node; node = node->pstNext)
    {
        pName = vtoy_json_get_string_ex(node->pstChild, "name");
        if (0 == g_strcmp0(pName, pCurLang))
        {
            break;
        }
    }

    if (NULL == node)
    {
        return "xxx";
    }

    return vtoy_json_get_string_ex(node->pstChild, id);
}

int msgbox(GtkMessageType type, GtkButtonsType buttons, const char *strid)
{
    int ret;
    GtkWidget *pMsgBox = NULL;

    pMsgBox = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, type, buttons, "%s", language_string(strid));

    ret = gtk_dialog_run(GTK_DIALOG(pMsgBox));
    gtk_widget_destroy(pMsgBox);

    return ret;
}


static void set_item_visible(const char *id, int visible)
{
    GtkWidget *pWidget = NULL;
    
    pWidget = GTK_WIDGET(gtk_builder_get_object(g_pXmlBuilder, id));
    if (visible)
    {
        gtk_widget_show(pWidget);        
    }
    else
    {
        gtk_widget_hide(pWidget);        
    }
}

static void init_part_style_menu(void)
{
    int style;

    style = ventoy_code_get_cur_part_style();
    gtk_check_menu_item_set_active(g_menu_item_mbr, (0 == style));
    gtk_check_menu_item_set_active(g_menu_item_gpt, (1 == style)); 
    gtk_label_set_text(g_label_local_part_style, style ? "GPT" : "MBR");
}

static void select_language(const char *lang)
{
    const char *pName = NULL;
    const char *pPos = NULL;
    const char *pDevice = NULL;
    VTOY_JSON *node = NULL;
    char device[256];
    
    for (node = g_languages_json->pstChild; node; node = node->pstNext)
    {
        pName = vtoy_json_get_string_ex(node->pstChild, "name");
        if (0 == g_strcmp0(pName, lang))
        {
            break;
        }
    }

    if (NULL == node)
    {
        return;
    }

    pDevice = gtk_label_get_text(g_device_title);
    if (pDevice && (pPos = strchr(pDevice, '[')) != NULL)
    {
        g_snprintf(device, sizeof(device), "%s  %s", vtoy_json_get_string_ex(node->pstChild, "STR_DEVICE"), pPos);
        gtk_label_set_text(g_device_title, device);
    }
    else
    {
        gtk_label_set_text(g_device_title, vtoy_json_get_string_ex(node->pstChild, "STR_DEVICE"));
    }
    
    LANG_LABEL_TEXT("label_local_ver", "STR_LOCAL_VER");
    LANG_LABEL_TEXT("label_device_ver", "STR_DISK_VER");
    
    LANG_LABEL_TEXT("label_status", "STR_STATUS");
    
    LANG_BUTTON_TEXT("button_install", "STR_INSTALL");
    LANG_BUTTON_TEXT("button_update", "STR_UPDATE");
    
    LANG_MENU_ITEM_TEXT("menu_option", "STR_MENU_OPTION");
    LANG_MENU_ITEM_TEXT("menu_item_secure", "STR_MENU_SECURE_BOOT");
    LANG_MENU_ITEM_TEXT("menu_part_style", "STR_MENU_PART_STYLE");
    LANG_MENU_ITEM_TEXT("menu_item_part_cfg", "STR_MENU_PART_CFG");
    LANG_MENU_ITEM_TEXT("menu_item_clear", "STR_MENU_CLEAR");
    LANG_MENU_ITEM_TEXT("menu_item_show_all", "STR_SHOW_ALL_DEV");

    LANG_BUTTON_TEXT("space_check_btn", "STR_PRESERVE_SPACE");
    LANG_BUTTON_TEXT("space_align_btn", "STR_PART_ALIGN_4KB");
    LANG_BUTTON_TEXT("button_partcfg_ok", "STR_BTN_OK");
    LANG_BUTTON_TEXT("button_partcfg_cancel", "STR_BTN_CANCEL");

    gtk_window_set_title(GTK_WINDOW(g_partCfgWindow), vtoy_json_get_string_ex(node->pstChild, "STR_MENU_PART_CFG"));

    /* 
     * refresh screen 
     */
     
    gtk_widget_hide(g_topWindow);
    gtk_widget_show(g_topWindow);
}


void on_secure_boot_toggled(GtkMenuItem *menuItem, gpointer data) 
{
    g_secure_boot_support = 1 - g_secure_boot_support;

    if (g_secure_boot_support)
    {
        gtk_widget_show((GtkWidget *)g_image_secure_local);        
    }
    else
    {
        gtk_widget_hide((GtkWidget *)g_image_secure_local);        
    }
}

void on_devlist_changed(GtkWidget *widget, gpointer data) 
{
    int active;
    ventoy_disk *cur = NULL;
    char version[512];

    if (g_dev_changed_proc == 0)
    {
        return;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), FALSE);

    gtk_widget_hide((GtkWidget *)g_image_secure_device);
    gtk_label_set_markup(g_label_disk_ver, "");
    gtk_label_set_text(g_label_dev_part_style, "");

    active = gtk_combo_box_get_active((GtkComboBox *)g_dev_combobox);
    if (active < 0 || active >= g_disk_num)
    {
        vlog("invalid active combox id %d\n", active);
        return;
    }

    cur = g_disk_list + active;
    if (cur->vtoydata.ventoy_valid)
    {
        if (cur->vtoydata.secure_boot_flag)
        {
            gtk_widget_show((GtkWidget *)g_image_secure_device);
        }
        else
        {
            gtk_widget_hide((GtkWidget *)g_image_secure_device);
        }

        if (g_secure_boot_support != cur->vtoydata.secure_boot_flag)
        {
            gtk_check_menu_item_set_active(g_menu_item_secure_boot, 1 - g_secure_boot_support);
        }

        g_snprintf(version, sizeof(version), VTOY_VER_FMT, cur->vtoydata.ventoy_ver);
        gtk_label_set_markup(g_label_disk_ver, version);
        gtk_label_set_text(g_label_dev_part_style, cur->vtoydata.partition_style ? "GPT" : "MBR");

        gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), TRUE);
    }
    else
    {
        if (!g_secure_boot_support)
        {
            gtk_check_menu_item_set_active(g_menu_item_secure_boot, 1 - g_secure_boot_support);
        }
    }
}


void on_language_toggled(GtkMenuItem *menuItem, gpointer data) 
{
    const char *cur_lang = NULL; 

    if (g_languages_toggled_proc == 0)
    {
        return;
    }

    cur_lang = ventoy_code_get_cur_language();
    if (g_strcmp0(cur_lang, (char *)data) != 0)
    {
        ventoy_code_set_cur_language((char *)data);
        select_language((char *)data);
    }
}

void on_part_style_toggled(GtkMenuItem *menuItem, gpointer data) 
{
    int style;
    
    style = ventoy_code_get_cur_part_style();
    ventoy_code_set_cur_part_style(1 - style);

    gtk_label_set_text(g_label_local_part_style, style ? "MBR" : "GPT");
}

static ventoy_disk *select_active_dev(const char *select, int *activeid)
{
    int i;
    int alldev;
    ventoy_disk *cur = NULL;

    alldev = ventoy_code_get_cur_show_all();
    
    /* find the match one */
    if (select)
    {
        for (i = 0; i < g_disk_num; i++)
        {
            cur = g_disk_list + i;
            if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
            {
                continue;
            }
            
            if (strcmp(cur->disk_name, select) == 0)
            {
                *activeid = i;
                return cur;
            }
        }
    }

    /* find the first one that installed with Ventoy */
    for (i = 0; i < g_disk_num; i++)
    {
        cur = g_disk_list + i;
        if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
        {
            continue;
        }
        
        if (cur->vtoydata.ventoy_valid)
        {
            *activeid = i;
            return cur;
        }
    }

    /* find the first USB interface device */
    for (i = 0; i < g_disk_num; i++)
    {
        cur = g_disk_list + i;
        if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
        {
            continue;
        }
        
        if (cur->type == VTOY_DEVICE_USB)
        {
            *activeid = i;
            return cur;
        }
    }

    /* use the first one */
    for (i = 0; i < g_disk_num; i++)
    {
        cur = g_disk_list + i;
        if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
        {
            continue;
        }
        
        *activeid = i;
        return cur;
    }
        
    return NULL;
}

static void fill_dev_list(const char *select)
{
    int i;
    int alldev;
    int activeid;
    int count = 0;
    char line[512];
    ventoy_disk *cur = NULL;
    ventoy_disk *active = NULL;
    GtkListStore *store = NULL;

    g_dev_changed_proc = 0;

    alldev = ventoy_code_get_cur_show_all();
    
    vlog("fill_dev_list total disk: %d showall:%d\n", g_disk_num, alldev);

    /* gtk_combo_box_text_remove_all */
    store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(g_dev_combobox)));
    gtk_list_store_clear(store);

    for (i = 0; i < g_disk_num; i++)
    {
        cur = g_disk_list + i;

        if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
        {
            continue;
        }

        g_snprintf(line, sizeof(line), "%s  [%s]  %s", cur->disk_name, cur->human_readable_size, cur->disk_model);
        gtk_combo_box_text_append_text(g_dev_combobox, line);
        count++;
    }

    active = select_active_dev(select, &activeid);
    if (active)
    {
        vlog("combox count:%d, active:%s id:%d\n", count, active->disk_name, activeid);
        gtk_combo_box_set_active((GtkComboBox *)g_dev_combobox, activeid); 
        gtk_widget_set_sensitive(GTK_WIDGET(g_install_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), active->vtoydata.ventoy_valid);
    }
    else
    {
        vlog("combox count:%d, no active id\n", count);
        gtk_widget_set_sensitive(GTK_WIDGET(g_install_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), FALSE);
    }

    g_dev_changed_proc = 1;
    on_devlist_changed(NULL, NULL);
}


void on_show_all_toggled(GtkMenuItem *menuItem, gpointer data) 
{
    int show_all = ventoy_code_get_cur_show_all();
    
    ventoy_code_set_cur_show_all(1 - show_all);
    fill_dev_list(NULL);
}

void on_button_refresh_clicked(GtkWidget *widget, gpointer data) 
{
    if (g_thread_run || ventoy_code_is_busy())
    {
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_WAIT_PROCESS");
        return;
    }    

    ventoy_code_refresh_device();
    fill_dev_list(NULL);
}

static void set_progress_bar_percent(int percent)
{
    char *pos = NULL;
    const char *text = NULL;
    char tmp[128];

    gtk_progress_bar_set_fraction(g_progress_bar, percent * 1.0 / 100);
    vlog("set percent %d\n", percent);
    
    text = language_string("STR_STATUS");
    if (percent == 0)
    {
        gtk_label_set_text(g_label_status, text);
    }
    else
    {
        g_snprintf(tmp, sizeof(tmp), "%s", text);
        pos = strchr(tmp, '-');
        if (pos)
        {
            g_snprintf(pos + 2, sizeof(tmp), "%d%%", percent);
            gtk_label_set_text(g_label_status, tmp);
        }
    }
}

void on_clear_ventoy(GtkMenuItem *menuItem, gpointer data) 
{
    int ret;
    int active;
    char buf[1024];
    char out[256];
    char disk_name[32]; 
    ventoy_disk *cur = NULL;

    if (g_thread_run || ventoy_code_is_busy())
    {
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_WAIT_PROCESS");
        return;
    }

    active = gtk_combo_box_get_active((GtkComboBox *)g_dev_combobox);
    if (active < 0 || active >= g_disk_num)
    {
        vlog("invalid active combox id %d\n", active);
        return;
    }

    if (GTK_RESPONSE_OK != msgbox(GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "STR_INSTALL_TIP"))
    {
        return;
    }

    if (GTK_RESPONSE_OK != msgbox(GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "STR_INSTALL_TIP2"))
    {
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET(g_refresh_button), FALSE);    
    gtk_widget_set_sensitive (GTK_WIDGET(g_install_button), FALSE);    
    gtk_widget_set_sensitive (GTK_WIDGET(g_update_button), FALSE);
    g_thread_run = TRUE;


    cur = g_disk_list + active;
    g_snprintf(disk_name, sizeof(disk_name), "%s", cur->disk_name);
    g_snprintf(buf, sizeof(buf), "{\"method\":\"clean\",\"disk\":\"%s\"}", disk_name);

    out[0] = 0;
    ventoy_func_handler(buf, out, sizeof(out));
    vlog("func handler clean <%s>\n", out);

    if (strstr(out, "success"))
    {
        ret = ventoy_code_get_result();
        ventoy_code_refresh_device();
        cur = NULL;
    }
    else
    {
        ret = 1;
    }

    if (ret == 0)
    {
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_CLEAR_SUCCESS");
    }
    else
    {
        msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_CLEAR_FAILED");
    }

    set_progress_bar_percent(0);
    gtk_widget_set_sensitive(GTK_WIDGET(g_refresh_button), TRUE);    
    gtk_widget_set_sensitive(GTK_WIDGET(g_install_button), TRUE);    
    gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), TRUE);

    fill_dev_list(disk_name);
    g_thread_run = FALSE;
}

static int install_proc(ventoy_disk *cur)
{
    int ret = 0;
    int pos = 0;
    int buflen = 0;
    int percent = 0;
    char buf[1024];
    char dec[64];
    char out[256];
    char disk_name[32]; 
    long long space;

    vlog("install_thread ...\n");

    g_snprintf(disk_name, sizeof(disk_name), "%s", cur->disk_name);

    buflen = sizeof(buf);
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("method", "install");
    VTOY_JSON_FMT_STRN("disk", disk_name);

    if (g_preserve_space_check)
    {
        space = g_preserve_space_number;
        if (g_preserve_space_unit == 1)
        {
            space = space * 1024 * 1024 * 1024LL;
        }
        else
        {
            space = space * 1024 * 1024LL;
        }

        snprintf(dec, sizeof(dec), "%lld", space);
        VTOY_JSON_FMT_STRN("reserve_space", dec);
    }
    else
    {
        VTOY_JSON_FMT_STRN("reserve_space", "0");
    }

    VTOY_JSON_FMT_UINT("partstyle", ventoy_code_get_cur_part_style());
    VTOY_JSON_FMT_UINT("secure_boot", g_secure_boot_support);
    VTOY_JSON_FMT_UINT("align_4kb", g_align_part_with_4k);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);
    
    out[0] = 0;
    ventoy_func_handler(buf, out, sizeof(out));
    vlog("func handler install <%s>\n", out);

    if (strstr(out, "success"))
    {
        while (percent != 100)
        {
            percent = ventoy_code_get_percent();
            set_progress_bar_percent(percent);
            GTK_MSG_ITERATION();
            usleep(50 * 1000);
        }

        ret = ventoy_code_get_result();
        ventoy_code_refresh_device();
        cur = NULL;
    }
    else
    {
        ret = 1;
    }

    if (ret)
    {
        msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_INSTALL_FAILED");
    }
    else
    {   
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_INSTALL_SUCCESS");
    }

    set_progress_bar_percent(0);
    gtk_widget_set_sensitive(GTK_WIDGET(g_refresh_button), TRUE);    
    gtk_widget_set_sensitive(GTK_WIDGET(g_install_button), TRUE);    
    gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), TRUE);

    fill_dev_list(disk_name);
    g_thread_run = FALSE;

    return 0;
}

void on_button_install_clicked(GtkWidget *widget, gpointer data) 
{
    int active;
    long long size;
    long long space;
    ventoy_disk *cur = NULL;

    if (g_thread_run || ventoy_code_is_busy())
    {
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_WAIT_PROCESS");
        return;
    }

    active = gtk_combo_box_get_active((GtkComboBox *)g_dev_combobox);
    if (active < 0 || active >= g_disk_num)
    {
        vlog("invalid active combox id %d\n", active);
        return;
    }

    cur = g_disk_list + active;

    if (cur->is4kn)
    {
        msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_4KN_UNSUPPORTED");
        return;
    }

    if (ventoy_code_get_cur_part_style() == 0 && cur->size_in_byte > 2199023255552ULL)
    {
        msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_DISK_2TB_MBR_ERROR");
        return;
    }

    if (g_preserve_space_check)
    {
        space = g_preserve_space_number;
        if (g_preserve_space_unit == 1)
        {
            space = space * 1024;
        }
        else
        {
            space = space;
        }

        size = cur->size_in_byte / SIZE_1MB;
        if (size <= space || (size - space) <= (VTOYEFI_PART_BYTES / SIZE_1MB))
    	{
    	    msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_SPACE_VAL_INVALID");
    		vlog("reserved space value too big ...\n");
    		return;
    	}
    }

    if (GTK_RESPONSE_OK != msgbox(GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "STR_INSTALL_TIP"))
    {
        return;
    }

    if (GTK_RESPONSE_OK != msgbox(GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "STR_INSTALL_TIP2"))
    {
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET(g_refresh_button), FALSE);    
    gtk_widget_set_sensitive (GTK_WIDGET(g_install_button), FALSE);    
    gtk_widget_set_sensitive (GTK_WIDGET(g_update_button), FALSE);
    
    g_thread_run = TRUE;

    install_proc(cur);
}


static int update_proc(ventoy_disk *cur)
{
    int ret = 0;
    int percent = 0;
    char buf[1024];
    char out[256];
    char disk_name[32]; 

    g_snprintf(disk_name, sizeof(disk_name), "%s", cur->disk_name);
    g_snprintf(buf, sizeof(buf), "{\"method\":\"update\",\"disk\":\"%s\",\"secure_boot\":%d}", 
        disk_name, g_secure_boot_support);

    out[0] = 0;
    ventoy_func_handler(buf, out, sizeof(out));
    vlog("func handler update <%s>\n", out);

    if (strstr(out, "success"))
    {
        while (percent != 100)
        {
            percent = ventoy_code_get_percent();
            set_progress_bar_percent(percent);
            GTK_MSG_ITERATION();
            usleep(50 * 1000);
        }

        ret = ventoy_code_get_result();
        ventoy_code_refresh_device();
        cur = NULL;
    }
    else
    {
        ret = 1;
    }

    if (ret)
    {
        msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_UPDATE_FAILED");
    }
    else
    {   
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_UPDATE_SUCCESS");
    }

    set_progress_bar_percent(0);
    gtk_widget_set_sensitive(GTK_WIDGET(g_refresh_button), TRUE);    
    gtk_widget_set_sensitive(GTK_WIDGET(g_install_button), TRUE);    
    gtk_widget_set_sensitive(GTK_WIDGET(g_update_button), TRUE);

    fill_dev_list(disk_name);
    g_thread_run = FALSE;

    return 0;
}


void on_button_update_clicked(GtkWidget *widget, gpointer data) 
{
    int active;
    ventoy_disk *cur = NULL;

    if (g_thread_run || ventoy_code_is_busy())
    {
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_WAIT_PROCESS");
        return;
    }

    active = gtk_combo_box_get_active((GtkComboBox *)g_dev_combobox);
    if (active < 0 || active >= g_disk_num)
    {
        vlog("invalid active combox id %d\n", active);
        return;
    }
    cur = g_disk_list + active;    

    if (cur->vtoydata.ventoy_valid == 0)
    {
        vlog("invalid ventoy version.\n");
        return;
    }

    if (GTK_RESPONSE_OK != msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL, "STR_UPDATE_TIP"))
    {
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET(g_refresh_button), FALSE);    
    gtk_widget_set_sensitive (GTK_WIDGET(g_install_button), FALSE);    
    gtk_widget_set_sensitive (GTK_WIDGET(g_update_button), FALSE);
    
    g_thread_run = TRUE;
    
    update_proc(cur);
}

static gint lang_compare(gconstpointer a, gconstpointer b)
{
    const char *name1 = (const char *)a;
    const char *name2 = (const char *)b;

    if (strncmp(name1, "Chinese Simplified", 18) == 0)
    {
        return -1;
    }
    else if (strncmp(name2, "Chinese Simplified", 18) == 0)
    {
        return 1;
    }
    else
    {
        return g_strcmp0(name1, name2);        
    }
}

static int load_languages(void)
{
    int size = 0;
    char *pBuf = NULL;
    char *pCur = NULL;
    const char *pCurLang = NULL;
    const char *pName = NULL;
    VTOY_JSON *json = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *lang = NULL;
    GSList *pGroup = NULL;
    GList *pNameNode = NULL;
    GList *pNameList = NULL;
    GtkRadioMenuItem *pItem = NULL;

    vlog("load_languages ...\n");

    pCurLang = ventoy_code_get_cur_language();
    if (pCurLang[0] == 0)
    {
        pName = getenv("LANG");
        if (pName && strncmp(pName, "zh_CN", 5) == 0)
        {
            ventoy_code_set_cur_language("Chinese Simplified (简体中文)");            
        }
        else
        {
            ventoy_code_set_cur_language("English (English)");            
        }
        pCurLang = ventoy_code_get_cur_language();
    }

    vlog("current language <%s>\n", pCurLang);
    
    ventoy_read_file_to_buf("./tool/languages.json", 1, (void **)&pBuf, &size);

    json = vtoy_json_create();
    vtoy_json_parse(json, pBuf);
    g_languages_json = json;

    for (node = json->pstChild; node; node = node->pstNext)
    {
        pName = vtoy_json_get_string_ex(node->pstChild, "name");
        if (pName)
        {
            pNameList = g_list_append(pNameList, (gpointer)pName);
        }

        for (lang = node->pstChild; lang; lang = lang->pstNext)
        {
            if (lang->enDataType == JSON_TYPE_STRING)
            {
                pCur = lang->unData.pcStrVal;
                while (*pCur)
                {
                    if (*pCur == '#')
                    {
                        *pCur = '\r';
                    }
                    else if (*pCur == '@')
                    {
                        *pCur = '\n';
                    }
                    pCur++;
                }
            }
        }
    }

    pNameList = g_list_sort(pNameList, lang_compare);

    for (pNameNode = g_list_first(pNameList); pNameNode; pNameNode = g_list_next(pNameNode))
    {
        pName = (char *)(pNameNode->data);
        pItem = (GtkRadioMenuItem *)gtk_radio_menu_item_new_with_label(pGroup, pName);
        pGroup = gtk_radio_menu_item_get_group(pItem);
        
        if (strcmp(pCurLang, pName) == 0)
        {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pItem), TRUE);
        }

        g_signal_connect(pItem, "toggled", G_CALLBACK(on_language_toggled), (gpointer)pName);
        gtk_widget_show((GtkWidget *)pItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(g_lang_menu), (GtkWidget *)pItem);
    }
    
    g_list_free(pNameList);
    free(pBuf);

    select_language(pCurLang);
    g_languages_toggled_proc = 1;
    
    return 0;
}

void on_part_cfg_ok(GtkWidget *widget, gpointer data) 
{
    const char *pos = NULL;
    const char *input = NULL;
    char device[256];
    gboolean checked = gtk_toggle_button_get_active(g_part_preserve_checkbox);

    if (checked)
    {
        input = gtk_entry_get_text(g_part_reserve_space_value);
        if (input == NULL || input[0] == 0)
        {
            msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_SPACE_VAL_INVALID");
            return;
        }

        for (pos = input; *pos; pos++)
        {
            if (*pos < '0' || *pos > '9')
            {
                msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "STR_SPACE_VAL_INVALID");
                return;
            }
        }

        g_preserve_space_unit = gtk_combo_box_get_active((GtkComboBox *)g_part_space_unit_combox);
        g_preserve_space_number = (int)strtol(input, NULL, 10);

        g_snprintf(device, sizeof(device), "%s  [ -%d%s ]", 
            language_string("STR_DEVICE"), g_preserve_space_number, 
            g_preserve_space_unit ? "GB" : "MB");
        gtk_label_set_text(g_device_title, device);
    }
    else
    {
        gtk_label_set_text(g_device_title, language_string("STR_DEVICE"));
    }

    g_preserve_space_check = checked;
    g_align_part_with_4k = gtk_toggle_button_get_active(g_part_align_checkbox);
    gtk_widget_hide(g_partCfgWindow);
}

void on_part_cfg_cancel(GtkWidget *widget, gpointer data) 
{
    gtk_widget_hide(g_partCfgWindow);
}

int on_part_cfg_close(GtkWidget *widget, gpointer data) 
{
    gtk_widget_hide(g_partCfgWindow);
    return TRUE;
}

void on_part_config(GtkMenuItem *menuItem, gpointer data) 
{
    char value[64];

    gtk_toggle_button_set_active(g_part_align_checkbox, g_align_part_with_4k);
    gtk_toggle_button_set_active(g_part_preserve_checkbox, g_preserve_space_check);
    gtk_widget_set_sensitive(GTK_WIDGET(g_part_reserve_space_value), g_preserve_space_check);
    gtk_widget_set_sensitive(GTK_WIDGET(g_part_space_unit_combox), g_preserve_space_check);

    if (g_preserve_space_check)
    {
        g_snprintf(value, sizeof(value), "%d", g_preserve_space_number);
        gtk_entry_set_text(g_part_reserve_space_value, value);
        gtk_combo_box_set_active((GtkComboBox *)g_part_space_unit_combox, g_preserve_space_unit);
    }

    gtk_widget_show_all(g_partCfgWindow);
}

void on_reserve_space_toggled(GtkMenuItem *menuItem, gpointer data) 
{
    gboolean checked = gtk_toggle_button_get_active(g_part_preserve_checkbox);

    gtk_widget_set_sensitive(GTK_WIDGET(g_part_reserve_space_value), checked);
    gtk_widget_set_sensitive(GTK_WIDGET(g_part_space_unit_combox), checked);
}

static void init_part_cfg_window(GtkBuilder *pBuilder)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkWidget *pHeader = NULL;

    pHeader = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(pHeader), FALSE);
    gtk_window_set_titlebar (GTK_WINDOW(g_partCfgWindow), pHeader);
    gtk_window_set_title(GTK_WINDOW(g_partCfgWindow), "Partition Configuration");
#endif

    gtk_combo_box_set_active((GtkComboBox *)g_part_space_unit_combox, g_preserve_space_unit);
    gtk_widget_set_sensitive (GTK_WIDGET(g_part_reserve_space_value), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET(g_part_space_unit_combox), FALSE);

    SIGNAL("space_check_btn",   "toggled",  on_reserve_space_toggled);

    SIGNAL("button_partcfg_ok", "clicked", on_part_cfg_ok);
    SIGNAL("button_partcfg_cancel", "clicked", on_part_cfg_cancel);
    SIGNAL("part_cfg_dlg", "delete_event", on_part_cfg_close);
}

static void add_accelerator(GtkAccelGroup *agMain, void *widget, const char *signal, guint accel_key)
{
    gtk_widget_add_accelerator(GTK_WIDGET(widget), signal, agMain, accel_key, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(widget), signal, agMain, accel_key, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
}

void on_init_window(GtkBuilder *pBuilder)
{
    GSList *pGroup = NULL;
    GtkAccelGroup *agMain = NULL;
    char version[512];

    vlog("on_init_window ...\n");
    
    g_pXmlBuilder = pBuilder;
    g_topWindow = BUILDER_ITEM(GtkWidget, "window");
    g_partCfgWindow = BUILDER_ITEM(GtkWidget, "part_cfg_dlg");

    g_dev_combobox = BUILDER_ITEM(GtkComboBoxText, "combobox_devlist");
    g_refresh_button = BUILDER_ITEM(GtkButton, "button_refresh");
    g_install_button = BUILDER_ITEM(GtkButton, "button_install");
    g_update_button = BUILDER_ITEM(GtkButton, "button_update");
    
    g_lang_menu = BUILDER_ITEM(GtkMenu, "submenu_language");
    g_menu_item_secure_boot = BUILDER_ITEM(GtkCheckMenuItem, "menu_item_secure");
    g_menu_item_mbr = BUILDER_ITEM(GtkCheckMenuItem, "menu_item_mbr");
    g_menu_item_gpt = BUILDER_ITEM(GtkCheckMenuItem, "menu_item_gpt");
    g_menu_item_show_all = BUILDER_ITEM(GtkCheckMenuItem, "menu_item_show_all");
    
    g_device_title = BUILDER_ITEM(GtkLabel, "label_device");
    g_label_local_part_style = BUILDER_ITEM(GtkLabel, "label_local_part_style");
    g_label_dev_part_style = BUILDER_ITEM(GtkLabel, "label_dev_part_style");
    
    g_label_local_ver = BUILDER_ITEM(GtkLabel, "label_local_ver_value");
    g_label_disk_ver = BUILDER_ITEM(GtkLabel, "label_dev_ver_value");
    
    g_label_status = BUILDER_ITEM(GtkLabel, "label_status");
    
    g_image_secure_local = BUILDER_ITEM(GtkImage, "image_secure_local");
    g_image_secure_device = BUILDER_ITEM(GtkImage, "image_secure_dev");
    
    g_part_preserve_checkbox = BUILDER_ITEM(GtkToggleButton, "space_check_btn");
    g_part_align_checkbox = BUILDER_ITEM(GtkToggleButton, "space_align_btn");

    g_part_reserve_space_value = BUILDER_ITEM(GtkEntry, "entry_reserve_space");
    g_part_space_unit_combox = BUILDER_ITEM(GtkComboBoxText, "comboboxtext_unit");
    
    g_progress_bar = BUILDER_ITEM(GtkProgressBar, "progressbar1");

    init_part_cfg_window(pBuilder);

    /* for gtk2 */
    gtk_frame_set_shadow_type(BUILDER_ITEM(GtkFrame, "frame_dummy1"), GTK_SHADOW_NONE); 
    gtk_frame_set_shadow_type(BUILDER_ITEM(GtkFrame, "frame_dummy2"), GTK_SHADOW_NONE); 

    /* join group */
    pGroup = gtk_radio_menu_item_get_group((GtkRadioMenuItem *)g_menu_item_mbr);
    gtk_radio_menu_item_set_group((GtkRadioMenuItem *)g_menu_item_gpt, pGroup);

    gtk_widget_hide((GtkWidget *)g_image_secure_local);
    gtk_widget_hide((GtkWidget *)g_image_secure_device);

    g_snprintf(version, sizeof(version), VTOY_VER_FMT, ventoy_get_local_version());
    gtk_label_set_markup(g_label_local_ver, version);

    init_part_style_menu();
    gtk_check_menu_item_set_active(g_menu_item_show_all, ventoy_code_get_cur_show_all());

    load_languages();

    SIGNAL("combobox_devlist", "changed", on_devlist_changed);
    
    SIGNAL("button_refresh", "clicked", on_button_refresh_clicked);
    SIGNAL("button_install", "clicked", on_button_install_clicked);
    SIGNAL("button_update",  "clicked", on_button_update_clicked);
    
    SIGNAL("menu_item_secure",   "toggled",  on_secure_boot_toggled);
    SIGNAL("menu_item_mbr",      "toggled",  on_part_style_toggled);
    SIGNAL("menu_item_show_all", "toggled",  on_show_all_toggled);
    
    SIGNAL("menu_item_part_cfg", "activate",  on_part_config);
    SIGNAL("menu_item_clear", "activate",  on_clear_ventoy);

    agMain = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(g_topWindow), agMain);
    add_accelerator(agMain, g_dev_combobox,   "popup",   GDK_KEY_d);
    add_accelerator(agMain, g_install_button, "clicked", GDK_KEY_i);
    add_accelerator(agMain, g_update_button,  "clicked", GDK_KEY_u);
    add_accelerator(agMain, g_refresh_button, "clicked", GDK_KEY_r);

    gtk_check_menu_item_set_active(g_menu_item_secure_boot, 1 - g_secure_boot_support);

    fill_dev_list(NULL);

    return;
}

int on_exit_window(GtkWidget *widget, gpointer data) 
{
    vlog("on_exit_window ...\n");

    if (g_thread_run || ventoy_code_is_busy())
    {
        msgbox(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "STR_WAIT_PROCESS");
        return TRUE;
    }

    ventoy_code_save_cfg();
    return FALSE;
}

