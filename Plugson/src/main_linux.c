#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_json.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>

char g_log_file[MAX_PATH];
char g_cur_dir[MAX_PATH];
char g_ventoy_dir[MAX_PATH];

int ventoy_log_init(void);
void ventoy_log_exit(void);

void ventoy_signal_stop(int sig)
{
    vlog("ventoy server exit due to signal ...\n");
    printf("ventoy server exit ...\n");
    
    ventoy_http_stop();
    ventoy_http_exit();
#ifndef VENTOY_SIM    
    ventoy_www_exit();
#endif
    ventoy_disk_exit();
    ventoy_log_exit();
    exit(0);
}

int main(int argc, char **argv)
{
    int rc;
    const char *ip = "127.0.0.1";
    const char *port = "24681";

    if (argc != 9)
    {
        printf("Invalid argc %d\n", argc);
        return 1;
    }

    if (isdigit(argv[1][0]))
    {
        ip = argv[1];
    }
    
    if (isdigit(argv[2][0]))
    {
        port = argv[2];
    }

    strlcpy(g_ventoy_dir, argv[3]);
    scnprintf(g_log_file, sizeof(g_log_file), "%s/%s", g_ventoy_dir, LOG_FILE);
    ventoy_log_init();

    getcwd(g_cur_dir, MAX_PATH);

    if (!ventoy_is_directory_exist("./ventoy"))
    {
        printf("%s/ventoy directory does not exist\n", g_cur_dir);
        return 1;
    }

    ventoy_get_disk_info(argv);    

    vlog("===============================================\n");
    vlog("===== Ventoy Plugson %s:%s =====\n", ip, port);
    vlog("===============================================\n");

    ventoy_disk_init();
#ifndef VENTOY_SIM
    rc = ventoy_www_init();
    if (rc)
	{
		printf("Failed to init web data, check log for details.\n");
		ventoy_disk_exit();
		ventoy_log_exit();
		return 1;
	}
#endif
    ventoy_http_init();
    
    rc = ventoy_http_start(ip, port);
    if (rc)
    {
        printf("Ventoy failed to start http server, check ./ventoy/plugson.log for detail\n");
    }
    else
    {
        signal(SIGINT, ventoy_signal_stop);
        signal(SIGTSTP, ventoy_signal_stop);
        signal(SIGQUIT, ventoy_signal_stop);
        while (1)
        {
            sleep(100);
        }
    }
    
    return 0;
}

