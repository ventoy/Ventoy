
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include "ventoy_gtk.h"

static int g_kiosk_mode = 0;
char g_log_file[PATH_MAX];
char g_ini_file[PATH_MAX];

static int set_image_from_pixbuf(GtkBuilder *pBuilder, const char *id, const void *pData, int len)
{
    GtkImage *pImage = NULL;
    GdkPixbuf *pPixbuf = NULL;
    GInputStream *pStream = NULL;

    pImage = (GtkImage *)gtk_builder_get_object(pBuilder, id);
    pStream = g_memory_input_stream_new_from_data(pData, len, NULL);
    pPixbuf = gdk_pixbuf_new_from_stream(pStream, NULL, NULL);
    gtk_image_set_from_pixbuf(pImage, pPixbuf);

    return 0;
}

static int set_window_icon_from_pixbuf(GtkWindow *window, const void *pData, int len)
{
    GdkPixbuf *pPixbuf = NULL;
    GInputStream *pStream = NULL;

    pStream = g_memory_input_stream_new_from_data(pData, len, NULL);
    pPixbuf = gdk_pixbuf_new_from_stream(pStream, NULL, NULL);

    gtk_window_set_icon(window, pPixbuf);
    return 0;
}

int early_msgbox(GtkMessageType type, GtkButtonsType buttons, const char *str)
{
    int ret;
    GtkWidget *pMsgBox = NULL;
    
    pMsgBox= gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, type, buttons, str);
    
    ret = gtk_dialog_run(GTK_DIALOG(pMsgBox));
    gtk_widget_destroy(pMsgBox);

    return ret;
}

static int adjust_cur_dir(char *argv0)
{
    int ret = 2;
    char c;
    char *pos = NULL;
    char *end = NULL;

    if (argv0[0] == '.')
    {
        return 1;
    }

    for (pos = argv0; pos && *pos; pos++)
    {
        if (*pos == '/')
        {
            end = pos;
        }
    }

    if (end)
    {
        c = *end;
        *end = 0;

        pos = strstr(argv0, "/tool/");
        if (pos)
        {
            *pos = 0;
        }
        
        ret = chdir(argv0);
        
        *end = c;
        if (pos)
        {
            *pos = '/';
        }
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int i;
    int len;
    const void *pData = NULL;
    GtkWidget *pWidget = NULL;
    GtkBuilder *pBuilder = NULL;
    GError *error = NULL;
    struct stat logstat;

    gtk_init(&argc, &argv);
    
    if (geteuid() != 0)
    {
        early_msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
                     "Ventoy2Disk permission denied!\r\nPlease run with root privileges.");
        return EACCES;
    }

    if (access("./boot/boot.img", F_OK) == -1)
    {
        adjust_cur_dir(argv[0]);        
    }

    if (access("./boot/boot.img", F_OK) == -1)
    {
        early_msgbox(GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Please run under the correct directory.");
        return 1;
    }

    for (i = 0; i < argc; i++)
    {
        if (argv[i] && strcmp(argv[i], "--kiosk") == 0)
        {
            g_kiosk_mode = 1;
            break;
        }
    }
    
    snprintf(g_log_file, sizeof(g_log_file), "log.txt");
    snprintf(g_ini_file, sizeof(g_ini_file), "./Ventoy2Disk.ini");
    for (i = 0; i < argc; i++)
    {
        if (argv[i] && argv[i + 1] && strcmp(argv[i], "-l") == 0)
        {
            snprintf(g_log_file, sizeof(g_log_file), "%s", argv[i + 1]);
        }
        else if (argv[i] && argv[i + 1] &&  strcmp(argv[i], "-i") == 0)
        {
            snprintf(g_ini_file, sizeof(g_ini_file), "%s", argv[i + 1]);
        }
    }

    memset(&logstat, 0, sizeof(logstat));
    if (0 == stat(g_log_file, &logstat))
    {
        if (logstat.st_size >= 4 * SIZE_1MB)
        {
            remove(g_log_file);
        }
    }

    ventoy_log_init();

    vlog("================================================\n");
    vlog("===== Ventoy2Disk %s powered by GTK%d.x =====\n", ventoy_get_local_version(), GTK_MAJOR_VERSION);
    vlog("================================================\n");
    vlog("log file is <%s> lastsize:%lld\n", g_log_file, (long long)logstat.st_size);
    vlog("ini file is <%s>\n", g_ini_file);

    ventoy_disk_init();

    ventoy_http_init();

    pBuilder = gtk_builder_new();
    if (!pBuilder)
    {
        vlog("failed to create builder\n");
        return 1;
    }

    if (!gtk_builder_add_from_file(pBuilder, "./tool/VentoyGTK.glade", &error))
    {
        vlog("gtk_builder_add_from_file failed:%s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    if (g_kiosk_mode)
    {
        gtk_image_set_from_file((GtkImage *)gtk_builder_get_object(pBuilder, "image_refresh"), "/ventoy/refresh.png");        
        gtk_image_set_from_file((GtkImage *)gtk_builder_get_object(pBuilder, "image_secure_local"), "/ventoy/secure.png");        
        gtk_image_set_from_file((GtkImage *)gtk_builder_get_object(pBuilder, "image_secure_dev"), "/ventoy/secure.png");        
    }
    else
    {
        pData = get_refresh_icon_raw_data(&len);
        set_image_from_pixbuf(pBuilder, "image_refresh", pData, len);        
        pData = get_secure_icon_raw_data(&len);
        set_image_from_pixbuf(pBuilder, "image_secure_local", pData, len);
        set_image_from_pixbuf(pBuilder, "image_secure_dev", pData, len);
    }

    pWidget = GTK_WIDGET(gtk_builder_get_object(pBuilder, "window"));
    gtk_widget_show_all(pWidget);

    pData = get_window_icon_raw_data(&len);
    set_window_icon_from_pixbuf(GTK_WINDOW(pWidget), pData, len);

    on_init_window(pBuilder);
    g_signal_connect(G_OBJECT(pWidget), "delete_event", G_CALLBACK(on_exit_window), NULL);
    g_signal_connect(G_OBJECT(pWidget), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    ventoy_disk_exit();
    ventoy_http_exit();
    
    g_object_unref (G_OBJECT(pBuilder));
    
    vlog("######## Ventoy2Disk GTK %s exit ########\n", ventoy_get_local_version());

    /* log exit must at the end */
    ventoy_log_exit();
    return 0;
}

