
#ifndef __VLNK_H__
#define __VLNK_H__

#define VLNK_FILE_LEN  32768

#define VLNK_NAME_MAX  384

#define VENTOY_GUID { 0x77772020, 0x2e77, 0x6576, { 0x6e, 0x74, 0x6f, 0x79, 0x2e, 0x6e, 0x65, 0x74 }}

#pragma pack(1)

typedef struct ventoy_guid
{
    uint32_t   data1;
    uint16_t   data2;
    uint16_t   data3;
    uint8_t    data4[8];
}ventoy_guid;

typedef struct ventoy_vlnk
{
    ventoy_guid   guid;         // VENTOY_GUID
    uint32_t crc32;        // crc32
    uint32_t  disk_signature;
    uint64_t part_offset; // in bytes
    char filepath[VLNK_NAME_MAX];
    uint8_t reserverd[96];
}ventoy_vlnk;
#pragma pack()

uint32_t ventoy_getcrc32c (uint32_t crc, const void *buf, int size);
int ventoy_create_vlnk(uint32_t disksig, uint64_t partoffset, const char *path, ventoy_vlnk *vlnk);
int CheckVlnkData(ventoy_vlnk *vlnk);
int IsSupportedImgSuffix(char *suffix);

#endif

