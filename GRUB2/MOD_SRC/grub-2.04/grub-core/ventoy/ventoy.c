/******************************************************************************
 * ventoy.c 
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/net.h>
#include <grub/misc.h>
#include <grub/kernel.h>
#include <grub/time.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

int g_ventoy_debug = 0;
static int g_efi_os = 0xFF;
grub_uint32_t g_ventoy_plat_data;

void ventoy_debug(const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    grub_vprintf (fmt, args);
    va_end (args);
}

void ventoy_str_tolower(char *str)
{
    while (*str)
    {
        *str = grub_tolower(*str);
        str++;
    }
}

void ventoy_str_toupper(char *str)
{
    while (*str)
    {
        *str = grub_toupper(*str);
        str++;
    }
}



int ventoy_strcmp(const char *pattern, const char *str)
{
    while (*pattern && *str)
    {
        if ((*pattern != *str) && (*pattern != '*'))
            break;

        pattern++;
        str++;
    }

    return (int)(grub_uint8_t)*pattern - (int)(grub_uint8_t)*str;
}

int ventoy_strncmp (const char *pattern, const char *str, grub_size_t n)
{
    if (n == 0)
        return 0;

    while (*pattern && *str && --n)
    {
        if ((*pattern != *str) && (*pattern != '*'))
            break;

        pattern++;
        str++;
    }

    return (int)(grub_uint8_t)*pattern - (int)(grub_uint8_t)*str;
}

void ventoy_debug_dump_guid(const char *prefix, grub_uint8_t *guid)
{
    int i;

    if (!g_ventoy_debug)
    {
        return;
    }
    
    debug("%s", prefix);
    for (i = 0; i < 16; i++)
    {
        grub_printf("%02x ", guid[i]);
    }
    grub_printf("\n");       
}

int ventoy_is_efi_os(void)
{
    if (g_efi_os > 1)
    {
        g_efi_os = (grub_strstr(GRUB_PLATFORM, "efi")) ? 1 : 0;
    }

    return g_efi_os;
}

static int ventoy_arch_mode_init(void)
{
    #ifdef GRUB_MACHINE_EFI
    if (grub_strcmp(GRUB_TARGET_CPU, "i386") == 0)
    {
        g_ventoy_plat_data = VTOY_PLAT_I386_UEFI;
        grub_snprintf(g_arch_mode_suffix, sizeof(g_arch_mode_suffix), "%s", "ia32");
    }
    else if (grub_strcmp(GRUB_TARGET_CPU, "arm64") == 0)
    {
        g_ventoy_plat_data = VTOY_PLAT_ARM64_UEFI;
        grub_snprintf(g_arch_mode_suffix, sizeof(g_arch_mode_suffix), "%s", "aa64");
    }
    else if (grub_strcmp(GRUB_TARGET_CPU, "mips64el") == 0)
    {
        g_ventoy_plat_data = VTOY_PLAT_MIPS_UEFI;
        grub_snprintf(g_arch_mode_suffix, sizeof(g_arch_mode_suffix), "%s", "mips");
    }
    else
    {
        g_ventoy_plat_data = VTOY_PLAT_X86_64_UEFI;
        grub_snprintf(g_arch_mode_suffix, sizeof(g_arch_mode_suffix), "%s", "uefi");
    }
#else
    g_ventoy_plat_data = VTOY_PLAT_X86_LEGACY;
    grub_snprintf(g_arch_mode_suffix, sizeof(g_arch_mode_suffix), "%s", "legacy");
#endif

    return 0;
}

GRUB_MOD_INIT(ventoy)
{
    ventoy_env_init();
    ventoy_arch_mode_init();
    ventoy_register_all_cmd();
}

GRUB_MOD_FINI(ventoy)
{
    ventoy_unregister_all_cmd();
}

