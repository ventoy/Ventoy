/******************************************************************************
 * biso_joliet.c
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

#include "biso.h"
#include "biso_joliet.h"

UCHAR BISO_JOLIET_GetLevel(IN CONST UCHAR *pucEscape)
{
    UCHAR ucLevel = 0;
    
    /*
     * Standard  Level           Decimal        Hex Bytes           ASCII
     * UCS-2     Level 1     2/5, 2/15, 4/0   (25)(2F)(40)         '%\@'         
     * UCS-2     Level 2     2/5, 2/15, 4/3   (25)(2F)(43)         '%\C'         
     * UCS-2     Level 3     2/5, 2/15, 4/5   (25)(2F)(45)         '%\E
     */
     
    if ((NULL != pucEscape) && (0x25 == pucEscape[0]) && (0x2F == pucEscape[1]))
    {
        if (0x40 == pucEscape[2])
        {
            ucLevel = 1;
        }
        else if (0x43 == pucEscape[2])
        {
            ucLevel = 2;
        }
        else if (0x45 == pucEscape[2])
        {
            ucLevel = 3;
        }
    }

    return ucLevel;
}

