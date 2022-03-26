#include "ventoy2diskwindow.h"
#include "ui_ventoy2diskwindow.h"
#include "partcfgdialog.h"

#include <QMessageBox>

extern "C" {
#include "ventoy_define.h"
#include "ventoy_util.h"
#include "ventoy_disk.h"
#include "ventoy_json.h"
#include "ventoy_http.h"
#include "ventoy_qt.h"
}

MyQThread::MyQThread(QObject *parent) : QThread(parent)
{
    m_index = -1;
    m_type = 0;
    m_running = false;
}

void MyQThread::install_run()
{
    int ret = 0;
    int pos = 0;
    int buflen = 0;
    int percent = 0;
    char buf[1024];
    char dec[64];
    char out[256];
    char disk_name[32];    
    ventoy_disk *cur;

    vlog("install run %d ...\n", m_index);

    cur = g_disk_list + m_index;
    snprintf(disk_name, sizeof(disk_name), "%s", cur->disk_name);
    snprintf(dec, sizeof(dec), "%llu", (unsigned long long)m_reserve_space);

    buflen = sizeof(buf);
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("method", "install");
    VTOY_JSON_FMT_STRN("disk", disk_name);
    VTOY_JSON_FMT_STRN("reserve_space", dec);
    VTOY_JSON_FMT_UINT("partstyle", ventoy_code_get_cur_part_style());
    VTOY_JSON_FMT_UINT("secure_boot", m_secureboot);
    VTOY_JSON_FMT_UINT("align_4kb", m_align4K);
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
            emit thread_event(THREAD_MSG_PROGRESS_BAR, percent);
            msleep(50);
        }

        ret = ventoy_code_get_result();
        ventoy_code_refresh_device();
        cur = NULL;
    }
    else
    {
        ret = 1;
    }

    emit thread_event(THREAD_MSG_INSTALL_FINISH, ret);
    m_running = false;
}

void MyQThread::update_run()
{
    int ret = 0;
    int percent = 0;
    char buf[1024];
    char out[256];
    char disk_name[32]; 
    ventoy_disk *cur;

    vlog("install run %d ...\n", m_index);

    cur = g_disk_list + m_index;
    snprintf(disk_name, sizeof(disk_name), "%s", cur->disk_name);
    snprintf(buf, sizeof(buf), "{\"method\":\"update\",\"disk\":\"%s\",\"secure_boot\":%d}", disk_name, m_secureboot);

    out[0] = 0;
    ventoy_func_handler(buf, out, sizeof(out));
    vlog("func handler update <%s>\n", out);

    if (strstr(out, "success"))
    {
        while (percent != 100)
        {
            percent = ventoy_code_get_percent();
            emit thread_event(THREAD_MSG_PROGRESS_BAR, percent);
            msleep(50);
        }

        ret = ventoy_code_get_result();
        ventoy_code_refresh_device();
        cur = NULL;
    }
    else
    {
        ret = 1;
    }

    emit thread_event(THREAD_MSG_UPDATE_FINISH, ret);
    m_running = false;
}

void MyQThread::run()
{
    if (THREAD_TYPE_INSTALL == m_type)
    {
        install_run();
    }
    else if (THREAD_TYPE_UPDATE == m_type)
    {
        update_run();
    }
    else
    {

    }
}


Ventoy2DiskWindow::Ventoy2DiskWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Ventoy2DiskWindow)
{
    m_partcfg = new PartCfgDialog();
    m_part_group = new QActionGroup(this);
    m_lang_group = new QActionGroup(this);
    m_thread = new MyQThread(this);

    ui->setupUi(this);
}

Ventoy2DiskWindow::~Ventoy2DiskWindow()
{
    delete m_partcfg;
    delete m_part_group;
    delete m_lang_group;
    delete m_thread;
    delete ui;
}

bool LangCompare(const QString &s1, const QString &s2)
{
    if (true == s1.startsWith("Chinese Simplified") && false == s2.startsWith("Chinese Simplified"))
    {
        return true;
    }
    else if (false == s1.startsWith("Chinese Simplified") && true == s2.startsWith("Chinese Simplified"))
    {
        return false;
    }
    else
    {
        return s1 < s2;
    }
}

int Ventoy2DiskWindow::lang_string(const QString &id, QString &str)
{
    QString cur = ventoy_code_get_cur_language();

    for (QJsonArray::iterator p = m_lang_array.begin(); p != m_lang_array.end(); p++)
    {
        if (p->toObject().value("name") == cur)
        {
            str = p->toObject().value(id).toString();
            str = str.replace("#@", "\r\n");
            return 0;
        }
    }

    return 1;
}

void Ventoy2DiskWindow::update_ui_language()
{
    QString dev;
    QJsonObject obj;
    QString cur = ventoy_code_get_cur_language();

    for (QJsonArray::iterator p = m_lang_array.begin(); p != m_lang_array.end(); p++)
    {
        if (p->toObject().value("name") == cur)
        {
            obj = p->toObject();
            break;
        }
    }

    ui->menuOption->setTitle(_LANG_STR("STR_MENU_OPTION"));
    ui->actionSecure_Boot_Support->setText(_LANG_STR("STR_MENU_SECURE_BOOT"));
    ui->menuPartition_Style->setTitle(_LANG_STR("STR_MENU_PART_STYLE"));
    ui->actionPartition_Configuration->setText(_LANG_STR("STR_MENU_PART_CFG"));
    ui->actionClear_Ventoy->setText(_LANG_STR("STR_MENU_CLEAR"));
    ui->actionShow_All_Devices->setText(_LANG_STR("STR_SHOW_ALL_DEV"));

    dev = _LANG_STR("STR_DEVICE");
    if (m_partcfg->reserve)
    {
        QString str;
        str.sprintf(" [ -%lld%s ]", (long long)m_partcfg->resvalue, m_partcfg->unit ? "GB" : "MB");
        ui->groupBoxDevice->setTitle(dev + str);
    }
    else
    {
        ui->groupBoxDevice->setTitle(dev);
    }

    ui->groupBoxVentoyLocal->setTitle(_LANG_STR("STR_LOCAL_VER"));
    ui->groupBoxVentoyDevice->setTitle(_LANG_STR("STR_DISK_VER"));
    ui->groupBoxStatus->setTitle(_LANG_STR("STR_STATUS"));
    ui->ButtonInstall->setText(_LANG_STR("STR_INSTALL"));
    ui->ButtonUpdate->setText(_LANG_STR("STR_UPDATE"));
    m_partcfg->update_language_ui(obj);
    m_partcfg->setWindowTitle(_LANG_STR("STR_MENU_PART_CFG"));
}

void Ventoy2DiskWindow::lang_check_action(QAction *act)
{
    ventoy_code_set_cur_language(act->text().toStdString().c_str());
    update_ui_language();
}

void Ventoy2DiskWindow::LoadLanguages()
{
    QString curlang = ventoy_code_get_cur_language();
    if (curlang.isEmpty())
    {
        QString LANG = qgetenv("LANG");
        if (LANG.startsWith("zh_CN"))
        {
            ventoy_code_set_cur_language("Chinese Simplified (简体中文)");
        }
        else
        {
            ventoy_code_set_cur_language("English (English)");                    
        }
        curlang = ventoy_code_get_cur_language();
    }


    QFile inFile("./tool/languages.json");
    inFile.open(QIODevice::ReadOnly|QIODevice::Text);
    QByteArray data = inFile.readAll();
    inFile.close();

    QJsonParseError errorPtr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &errorPtr);

    m_lang_array = doc.array();
    QVector<QString> List;
    for (QJsonArray::iterator p = m_lang_array.begin(); p != m_lang_array.end(); p++)
    {
        List.push_back(p->toObject().value("name").toString());
    }

    connect(m_lang_group, SIGNAL(triggered(QAction *)), this, SLOT(lang_check_action(QAction *)));

    std::sort(List.begin(), List.end(), LangCompare);

    for (QVector<QString>::iterator p = List.begin(); p != List.end(); p++)
    {
        QAction *action = new QAction(*p, m_lang_group);
        action->setCheckable(true);

        if (p->compare(curlang) == 0)
        {
            action->setChecked(true);
            m_lang_group->triggered(action);
        }

        ui->menuLanguage->addAction(action);
    }


}

void Ventoy2DiskWindow::part_style_check_action(QAction *action)
{
    int style = 0;

    if (action->text() == "MBR")
    {
        style = 0;
        ui->labelVentoyLocalPartStyle->setText("MBR");
    }
    else
    {
        style = 1;
        ui->labelVentoyLocalPartStyle->setText("GPT");
    }

    if (style != ventoy_code_get_cur_part_style())
    {
        ventoy_code_set_cur_part_style(style);
    }
}

static ventoy_disk *select_active_dev(const QString &select, int *activeid)
{
    int i;
    int alldev = ventoy_code_get_cur_show_all();
    ventoy_disk *cur = NULL;

    /* find the match one */
    if (!select.isEmpty())
    {
        for (i = 0; i < g_disk_num; i++)
        {
            cur = g_disk_list + i;
            if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
            {
                continue;
            }

            if (select.compare(cur->disk_name) == 0)
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


void Ventoy2DiskWindow::FillDeviceList(const QString &select)
{
    int active;
    int count = 0;
    int alldev = ventoy_code_get_cur_show_all();
    ventoy_disk *cur;

    ui->comboBoxDevice->clear();

    for (int i = 0; i < g_disk_num; i++)
    {
        cur = g_disk_list + i;

        if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
        {
            continue;
        }

        QString item;
        item.sprintf("%s  [%s]  %s", cur->disk_name, cur->human_readable_size, cur->disk_model);
        ui->comboBoxDevice->addItem(item);
        count++;
    }

    cur = select_active_dev(select, &active);
    if (cur)
    {
        vlog("combox count:%d, active:%s id:%d\n", count, cur->disk_name, active);
        ui->ButtonInstall->setEnabled(true);
        ui->ButtonUpdate->setEnabled(cur->vtoydata.ventoy_valid);
        ui->comboBoxDevice->setCurrentIndex(active);
    }
    else
    {
        vlog("combox count:%d, no active id\n", count);
        ui->ButtonInstall->setEnabled(false);
        ui->ButtonUpdate->setEnabled(false);
    }
}


void Ventoy2DiskWindow::OnInitWindow(void)
{
    int len;
    const uchar *data;
    QIcon icon;
    QPixmap pix1;
    QPixmap pix2;
    char ver[512];

    ui->labelVentoyLocalSecure->hide();

    m_part_group->addAction(ui->actionMBR);
    m_part_group->addAction(ui->actionGPT);
    connect(m_part_group, SIGNAL(triggered(QAction *)), this, SLOT(part_style_check_action(QAction *)));

    if (ventoy_code_get_cur_part_style())
    {
        ui->actionGPT->setChecked(true);
        m_part_group->triggered(ui->actionGPT);
    }
    else
    {
        ui->actionMBR->setChecked(true);
        m_part_group->triggered(ui->actionMBR);
    }

    snprintf(ver, sizeof(ver), VERSION_FMT, ventoy_get_local_version());
    ui->labelVentoyLocalVer->setText(QApplication::translate("Ventoy2DiskWindow", ver, nullptr));

    LoadLanguages();

    data = (const uchar *)get_refresh_icon_raw_data(&len);
    pix1.loadFromData(data, len);
    icon.addPixmap(pix1);
    ui->ButtonRefresh->setIcon(icon);

    data = (const uchar *)get_secure_icon_raw_data(&len);
    pix2.loadFromData(data, len);
    ui->labelVentoyLocalSecure->setPixmap(pix2);
    ui->labelVentoyDeviceSecure->setPixmap(pix2);

    ui->labelVentoyDeviceSecure->setHidden(true);
    ui->labelVentoyDeviceVer->setText("");
    ui->labelVentoyDevicePartStyle->setText("");

    ui->actionShow_All_Devices->setChecked(ventoy_code_get_cur_show_all());

    connect(m_thread, &MyQThread::thread_event, this,  &Ventoy2DiskWindow::thread_event);

    FillDeviceList("");
}

void Ventoy2DiskWindow::showEvent(QShowEvent *ev)
{
    QMainWindow::showEvent(ev);
    OnInitWindow();
}

void Ventoy2DiskWindow::on_ButtonInstall_clicked()
{
    int index;
    quint64 size = 0, space = 0;
    ventoy_disk *cur;
    QString title_warn, title_err, msg;

    lang_string("STR_ERROR", title_err);
    lang_string("STR_WARNING", title_warn);

    if (m_thread->m_running || ventoy_code_is_busy())
    {
        lang_string("STR_WAIT_PROCESS", msg);
        QMessageBox::warning(NULL, title_warn, msg);
        return;
    }

    index = ui->comboBoxDevice->currentIndex();
    if (index < 0 || index > g_disk_num)
    {
        vlog("Invalid combobox current index %d\n", index);
        return;
    }

    cur = g_disk_list + index;
    if (ventoy_code_get_cur_part_style() == 0 && cur->size_in_byte > 2199023255552ULL)
    {
        lang_string("STR_DISK_2TB_MBR_ERROR", msg);
        QMessageBox::critical(NULL, title_err, msg);
        return;
    }

    if (m_partcfg->reserve)
    {
        size = cur->size_in_byte / SIZE_1MB;
        space = m_partcfg->resvalue;
        if (m_partcfg->unit == 1)
        {
            space = m_partcfg->resvalue * 1024;
        }

        if (size <= space || (size - space) <= VTOYEFI_PART_BYTES / SIZE_1MB)
        {
            lang_string("STR_SPACE_VAL_INVALID", msg);
            QMessageBox::critical(NULL, title_err, msg);
            vlog("reserved space too big.\n");
            return;
        }
    }

    lang_string("STR_INSTALL_TIP", msg);
    if (QMessageBox::Yes != QMessageBox::warning(NULL, title_warn, msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
    {
        return;
    }

    lang_string("STR_INSTALL_TIP2", msg);
    if (QMessageBox::Yes != QMessageBox::warning(NULL, title_warn, msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
    {
        return;
    }

    ui->ButtonRefresh->setEnabled(false);
    ui->ButtonInstall->setEnabled(false);
    ui->ButtonRefresh->setEnabled(false);

    m_thread->m_type = THREAD_TYPE_INSTALL;
    m_thread->m_index = index;
    m_thread->m_reserve_space = space * SIZE_1MB;
    m_thread->m_secureboot = ui->actionSecure_Boot_Support->isChecked();
    m_thread->m_align4K = m_partcfg->align;
    m_thread->m_running = true;

    m_thread->start();
}

void Ventoy2DiskWindow::on_ButtonUpdate_clicked()
{
    int index;
    ventoy_disk *cur;
    QString title_info, title_warn, title_err, msg;

    lang_string("STR_ERROR", title_err);
    lang_string("STR_WARNING", title_warn);
    lang_string("STR_INFO", title_info);

    if (m_thread->m_running || ventoy_code_is_busy())
    {        
        lang_string("STR_WAIT_PROCESS", msg);
        QMessageBox::warning(NULL, title_warn, msg);
        return;
    }

    index = ui->comboBoxDevice->currentIndex();
    if (index < 0 || index > g_disk_num)
    {
        vlog("Invalid combobox current index %d\n", index);
        return;
    }

    cur = g_disk_list + index;
    if (cur->vtoydata.ventoy_valid == 0)
    {
        vlog("invalid ventoy version");
        return;
    }

    lang_string("STR_UPDATE_TIP", msg);
    if (QMessageBox::Yes != QMessageBox::information(NULL, title_info, msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
    {
        return;
    }

    ui->ButtonRefresh->setEnabled(false);
    ui->ButtonInstall->setEnabled(false);
    ui->ButtonRefresh->setEnabled(false);

    m_thread->m_type = THREAD_TYPE_UPDATE;
    m_thread->m_index = index;        
    m_thread->m_secureboot = ui->actionSecure_Boot_Support->isChecked();

    m_thread->m_running = true;
    m_thread->start();
}

void Ventoy2DiskWindow::on_ButtonRefresh_clicked()
{
    QString title_warn, msg;

    if (m_thread->m_running || ventoy_code_is_busy())
    {
        lang_string("STR_WARNING", title_warn);
        lang_string("STR_WAIT_PROCESS", msg);
        QMessageBox::warning(NULL, title_warn, msg);
        return;
    }

    ventoy_code_refresh_device();
    FillDeviceList("");
}

void Ventoy2DiskWindow::on_comboBoxDevice_currentIndexChanged(int index)
{    
    char ver[512];
    ventoy_disk *cur;

    ui->labelVentoyDeviceSecure->setHidden(true);
    ui->labelVentoyDeviceVer->setText("");
    ui->labelVentoyDevicePartStyle->setText("");

    if (index < 0 || index > g_disk_num)
    {
        vlog("invalid combobox index %d\n", index);
        return;
    }

    cur = g_disk_list + index;
    if (cur->vtoydata.ventoy_valid)
    {
        if (cur->vtoydata.secure_boot_flag)
        {
            ui->labelVentoyDeviceSecure->setHidden(false);
        }
        else
        {
            ui->labelVentoyDeviceSecure->setHidden(true);
        }

        if ((int)(ui->actionSecure_Boot_Support->isChecked()) != cur->vtoydata.secure_boot_flag)
        {
            ui->actionSecure_Boot_Support->trigger();
        }

        snprintf(ver, sizeof(ver), VERSION_FMT, cur->vtoydata.ventoy_ver);
        ui->labelVentoyDeviceVer->setText(QApplication::translate("Ventoy2DiskWindow", ver, nullptr));
        ui->labelVentoyDevicePartStyle->setText(cur->vtoydata.partition_style ? "GPT" : "MBR");
    }
    else
    {
        if (ui->actionSecure_Boot_Support->isChecked())
        {
            ui->actionSecure_Boot_Support->trigger();
        }
    }
}

void Ventoy2DiskWindow::on_actionPartition_Configuration_triggered()
{
    m_partcfg->update_ui_status();
    if (QDialog::Accepted == m_partcfg->exec())
    {
        QString str;
        QString dev;
        lang_string("STR_DEVICE", dev);

        if (m_partcfg->reserve)
        {
            str.sprintf(" [ -%lld%s ]", (long long)m_partcfg->resvalue, m_partcfg->unit ? "GB" : "MB");
            ui->groupBoxDevice->setTitle(dev + str);
        }
        else
        {
            ui->groupBoxDevice->setTitle(dev);
        }
    }
}

void Ventoy2DiskWindow::on_actionClear_Ventoy_triggered()
{
    int ret;
    int index;
    ventoy_disk *cur;
    QString title_err, title_warn, title_info, msg;
    char disk_name[64];
    char buf[256];
    char out[256];

    lang_string("STR_ERROR", title_err);
    lang_string("STR_WARNING", title_warn);
    lang_string("STR_INFO", title_info);

    if (m_thread->m_running || ventoy_code_is_busy())
    {
        lang_string("STR_WAIT_PROCESS", msg);
        QMessageBox::warning(NULL, title_warn, msg);
        return;
    }

    index = ui->comboBoxDevice->currentIndex();
    if (index < 0 || index > g_disk_num)
    {
        vlog("Invalid combobox current index %d\n", index);
        return;
    }

    cur = g_disk_list + index;

    lang_string("STR_INSTALL_TIP", msg);
    if (QMessageBox::Yes != QMessageBox::warning(NULL, title_warn, msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
    {
        return;
    }

    lang_string("STR_INSTALL_TIP2", msg);
    if (QMessageBox::Yes != QMessageBox::warning(NULL, title_warn, msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
    {
        return;
    }

    snprintf(disk_name, sizeof(disk_name), "%s", cur->disk_name);
    snprintf(buf, sizeof(buf), "{\"method\":\"clean\",\"disk\":\"%s\"}", disk_name);

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
        lang_string("STR_CLEAR_SUCCESS", msg);
        QMessageBox::information(NULL, title_info, msg);
    }
    else
    {
        lang_string("STR_CLEAR_FAILED", msg);
        QMessageBox::critical(NULL, title_err, msg);
    }

    FillDeviceList(disk_name);
}

void Ventoy2DiskWindow::on_actionShow_All_Devices_toggled(bool arg1)
{
    ventoy_code_set_cur_show_all(arg1);
    FillDeviceList("");
}


void Ventoy2DiskWindow::closeEvent(QCloseEvent *event)
{
    vlog("On closeEvent ...\n");

    if (m_thread->m_running)
    {
        QString title;
        QString msg;

        lang_string("STR_WARNING", title);
        lang_string("STR_WAIT_PROCESS", msg);
        QMessageBox::warning(NULL, title, msg);

        event->ignore();
        return;
    }

    ventoy_code_save_cfg();

    event->accept();
}

void Ventoy2DiskWindow::on_actionSecure_Boot_Support_triggered()
{
    ui->labelVentoyLocalSecure->setHidden(!(ui->actionSecure_Boot_Support->isChecked()));
}

void Ventoy2DiskWindow::set_percent(int percent)
{
    int index;
    QString status, radio;

    ui->progressBar->setValue(percent);

    lang_string("STR_STATUS", status);

    if (percent == 0)
    {
        ui->groupBoxStatus->setTitle(status);
    }
    else
    {
        index = status.indexOf("-");
        radio.sprintf("%d%%", percent);
        ui->groupBoxStatus->setTitle(status.left(index + 2) + radio);
    }
}

void Ventoy2DiskWindow::thread_event(int msg, int data)
{
    char disk_name[32];
    QString title_err, title_info, tipmsg;

    if (msg == THREAD_MSG_PROGRESS_BAR)
    {
        set_percent(data);
    }
    else if (msg == THREAD_MSG_INSTALL_FINISH)
    {
        lang_string("STR_ERROR", title_err);
        lang_string("STR_INFO", title_info);

        if (data == 0)
        {
            lang_string("STR_INSTALL_SUCCESS", tipmsg);
            QMessageBox::information(NULL, title_info, tipmsg);
        }
        else
        {
            lang_string("STR_INSTALL_FAILED", tipmsg);
            QMessageBox::critical(NULL, title_err, tipmsg);
        }

        set_percent(0);
        ui->ButtonRefresh->setEnabled(true);
        ui->ButtonInstall->setEnabled(true);
        ui->ButtonRefresh->setEnabled(true);

        snprintf(disk_name, sizeof(disk_name), "%s", g_disk_list[m_thread->m_index].disk_name);
        FillDeviceList(disk_name);
    }
    else if (msg == THREAD_MSG_UPDATE_FINISH)
    {
        lang_string("STR_ERROR", title_err);
        lang_string("STR_INFO", title_info);

        if (data == 0)
        {
            lang_string("STR_UPDATE_SUCCESS", tipmsg);
            QMessageBox::information(NULL, title_info, tipmsg);
        }
        else
        {
            lang_string("STR_UPDATE_FAILED", tipmsg);
            QMessageBox::critical(NULL, title_err, tipmsg);
        }

        set_percent(0);
        ui->ButtonRefresh->setEnabled(true);
        ui->ButtonInstall->setEnabled(true);
        ui->ButtonRefresh->setEnabled(true);

        snprintf(disk_name, sizeof(disk_name), "%s", g_disk_list[m_thread->m_index].disk_name);
        FillDeviceList(disk_name);
    }
}
