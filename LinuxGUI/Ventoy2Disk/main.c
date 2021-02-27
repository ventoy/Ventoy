#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_json.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>

int ventoy_log_init(void);
void ventoy_log_exit(void);

void ventoy_signal_stop(int sig)
{
    vlog("ventoy server exit due to signal ...\n");
    printf("ventoy server exit ...\n");
    
    ventoy_http_stop();
    ventoy_http_exit();
    ventoy_disk_exit();
    ventoy_log_exit();
    exit(0);
}

int main(int argc, char **argv)
{
    int rc;
    const char *ip = "127.0.0.1";
    const char *port = "24680";

    if (argc != 3)
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

    ventoy_log_init();

    vlog("===============================================\n");
    vlog("===== Ventoy2Disk %s %s:%s =====\n", ventoy_get_local_version(), ip, port);
    vlog("===============================================\n");

    ventoy_disk_init();

    ventoy_http_init();
    
    rc = ventoy_http_start(ip, port);
    if (rc)
    {
        printf("Ventoy failed to start http server, check log.txt for detail\n");
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

