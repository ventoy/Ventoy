/******************************************************************************
 * vtoy_gen_uuid.c 
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#include <fcntl.h>

int main()
{
    int i;
    int fd;
    unsigned char uuid[16];
    
    fd = open("/dev/random", O_RDONLY);
    if (fd < 0)
    {
        srand(time(NULL));        
        for (i = 0; i < 16; i++)
        {
            uuid[i] = (unsigned char)(rand());
        }
    }
    else
    {
        read(fd, uuid, 16);
    }
    
    fwrite(uuid, 1, 16, stdout);
    return 0;
}
