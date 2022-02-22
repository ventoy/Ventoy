#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "vlnk.h"

int ventoy_create_vlnk(uint32_t disksig, uint64_t partoffset, const char *path, ventoy_vlnk *vlnk)
{
    uint32_t crc;
    ventoy_guid guid = VENTOY_GUID;

    memcpy(&(vlnk->guid), &guid, sizeof(ventoy_guid));
    vlnk->disk_signature = disksig;
    vlnk->part_offset = partoffset;

#ifdef WIN32
    strcpy_s(vlnk->filepath, sizeof(vlnk->filepath) - 1, path);
#else
    strncpy(vlnk->filepath, path, sizeof(vlnk->filepath) - 1);
#endif

    crc = ventoy_getcrc32c(0, vlnk, sizeof(ventoy_vlnk));
    vlnk->crc32 = crc;

    return 0;
}


int CheckVlnkData(ventoy_vlnk *vlnk)
{
    uint32_t readcrc, calccrc;
    ventoy_guid guid = VENTOY_GUID;

    if (memcmp(&vlnk->guid, &guid, sizeof(guid)))
    {
        return 0;
    }

    readcrc = vlnk->crc32;
    vlnk->crc32 = 0;
    calccrc = ventoy_getcrc32c(0, vlnk, sizeof(ventoy_vlnk));

    if (readcrc != calccrc)
    {
        return 0;
    }

    return 1;
}

int IsSupportedImgSuffix(char *suffix)
{
    int i = 0;
    const char *suffixs[] =
    {
        ".iso", ".img", ".wim", ".efi", ".vhd", ".vhdx", ".dat", ".vtoy", NULL
    };

    if (!suffix)
    {
        return 0;
    }

    while (suffixs[i])
    {

#ifdef WIN32
        if (_stricmp(suffixs[i], suffix) == 0)
#else
        if (strcasecmp(suffixs[i], suffix) == 0)
#endif
        {
            return 1;
        }

        i++;
    }

    return 0;
}
