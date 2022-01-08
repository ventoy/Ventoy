/******************************************************************************
 * vtoyksym.c  ---- ventoy ksym
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
#include <string.h>
#include <errno.h>
#include <unistd.h>

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

int vtoyksym_main(int argc, char **argv)
{
    int i;
    unsigned long long addr1 = 0;
    unsigned long long addr2 = 0;
    char sym[256];
    char line[1024];
    const char *name = NULL;
    FILE *fp;

    for (i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'p')
        {
            printf("%d", getpagesize());
            return 0;
        }
    }
    
    for (i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            verbose = 1;
            break;
        }
    }

    name = argv[2] ? argv[2] : "/proc/kallsyms";
    fp = fopen(name, "r");
    if (!fp)
    {
        fprintf(stderr, "Failed to open file %s err:%d\n", name, errno);
        return 1;
    }

    debug("open %s success\n", name);

    snprintf(sym, sizeof(sym), " %s", argv[1]);
    debug("lookup for <%s>\n", sym);

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, sym))
        {
            addr1 = strtoull(line, NULL, 16);
            if (!fgets(line, sizeof(line), fp))
            {
                addr1 = 0;
                fprintf(stderr, "Failed to read next line\n");
            }
            else
            {
                addr2 = strtoull(line, NULL, 16);                
            }

            debug("addr1=<0x%llx> addr2=<0x%llx>\n", addr1, addr2);
            break;
        }
    }

    if (addr1 > addr2)
    {
        debug("Invalid addr range\n");
        printf("0 0\n");
    }
    else
    {
        printf("0x%llx %llu\n", addr1, addr2 - addr1);
    }

    fclose(fp);

    return 0;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoyksym_main(argc, argv);
}
#endif

