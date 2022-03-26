#include <stdio.h>
#include <string.h>
#include <sys/types.h>  
#include <sys/stat.h>
#include <sys/utsname.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        return 1;
    }

    if (argv[1][0] == '-' && argv[1][1] == '6')
    {
        struct utsname buf;
        if (0 == uname(&buf))
        {
            if (strstr(buf.machine, "amd64"))
            {
                return 0;
            }
            
            if (strstr(buf.machine, "x86_64"))
            {
                return 0;
            }
        }
        return 1;
    }

    return chmod(argv[1], 0777);
}

