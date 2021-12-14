/******************************************************************************
 * ventoy_disk.c  ---- ventoy disk
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
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ventoy_define.h>
#include <ventoy_disk.h>
#include <ventoy_util.h>
#include <fat_filelib.h>

static int g_disk_num = 0;
ventoy_disk *g_disk_list = NULL;

int ventoy_disk_init(void)
{
	char Letter = 'A';
	DWORD Drives = GetLogicalDrives();

    vlog("ventoy disk init ...\n");   

    g_disk_list = zalloc(sizeof(ventoy_disk) * MAX_DISK);

	while (Drives)
	{
		if (Drives & 0x01)
		{
			if (CheckRuntimeEnvironment(Letter, g_disk_list + g_disk_num) == 0)
			{
				g_disk_list[g_disk_num].devname[0] = Letter;
				g_disk_num++;
                vlog("%C: is ventoy disk\n", Letter);
			}
			else
			{
				memset(g_disk_list + g_disk_num, 0, sizeof(ventoy_disk));
                vlog("%C: is NOT ventoy disk\n", Letter);
			}
		}

		Letter++;
		Drives >>= 1;
	}

    return 0;
}

void ventoy_disk_exit(void)
{
    vlog("ventoy disk exit ...\n");   

    check_free(g_disk_list);
    g_disk_list = NULL;
    g_disk_num = 0;
}

const ventoy_disk * ventoy_get_disk_list(int *num)
{
    *num = g_disk_num;
    return g_disk_list;
}

const ventoy_disk * ventoy_get_disk_node(int id)
{
    if (id >= 0 && id < g_disk_num)
    {
        return g_disk_list + id;
    }

    return NULL;
}

