#include "ventoy2diskwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QStyle>
#include <QDir>
#include <QDesktopWidget>
#include <QPixmap>
#include <unistd.h>
#include <sys/types.h>

extern "C" {
#include "ventoy_define.h"
#include "ventoy_util.h"
#include "ventoy_qt.h"
}

using namespace std;

char g_log_file[4096];
char g_ini_file[4096];

int main(int argc, char *argv[])
{
    int ret;
    long long size;
    QApplication a(argc, argv);
    Ventoy2DiskWindow w;

#ifdef QT_CHECK_EUID
    if (geteuid() != 0)
    {
        QMessageBox::critical(NULL, "Error", "Permission denied!\nPlease run with root privileges.");
        return 1;
    }
#endif

    if (!QFileInfo::exists("./boot/boot.img"))
    {
        QString curdir = a.applicationDirPath();
        int index = curdir.indexOf("/tool/");

        if (index >= 0)
        {
            QDir::setCurrent(curdir.left(index));
        }
        else
        {
            QDir::setCurrent(curdir);        
        }        
    }

    if (!QFileInfo::exists("./boot/boot.img"))
    {
        QMessageBox::critical(NULL, "Error", "Please run under the correct directory.");
        return 1;
    }

    ventoy_log_init();

    snprintf(g_log_file, sizeof(g_log_file), "./log.txt");
    snprintf(g_ini_file, sizeof(g_ini_file), "./Ventoy2Disk.ini");
    for (int i = 0; i < argc; i++)
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

    QFileInfo Info(g_log_file);
    size = (long long)Info.size();
    if (size >= 4 * SIZE_1MB)
    {
        QFile::remove(g_log_file);
    }

    vlog("===================================================\n");
    vlog("===== Ventoy2Disk %s powered by QT %s =====\n", ventoy_get_local_version(), qVersion());
    vlog("===================================================\n");
    vlog("log file is <%s> lastsize:%lld\n", g_log_file, (long long)size);
    vlog("ini file is <%s>\n", g_ini_file);

    ventoy_disk_init();
    ventoy_http_init();

    w.setGeometry(QStyle::alignedRect(Qt::LeftToRight,
                                      Qt::AlignCenter,
                                      w.size(),
                                      a.desktop()->availableGeometry()));

    QPixmap pix;
    QIcon icon;
    int len;
    const uchar *data;

    data = (const uchar *)get_window_icon_raw_data(&len);
    pix.loadFromData(data, len);
    icon.addPixmap(pix);
    w.setWindowIcon(icon);

    w.show();
    w.setFixedSize(w.size());

    ret = a.exec();

    ventoy_disk_exit();
    ventoy_http_exit();
        
    vlog("######## Ventoy2Disk QT %s exit ########\n", ventoy_get_local_version());

    /* log exit must at the end */
    ventoy_log_exit();

    return ret;
}
