/******************************************************************************
 * ventoy_def.h
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

#ifndef __VENTOY_DEF_H__
#define __VENTOY_DEF_H__

#define VTOY_MAX_SCRIPT_BUF    (4 * 1024 * 1024)

#define JSON_SUCCESS    0
#define JSON_FAILED     1
#define JSON_NOT_FOUND  2

#define ulong unsigned long
#define ulonglong  unsigned long long

#define vtoy_to_upper(c) (((char)(c) >= 'a' && (char)(c) <= 'z') ? ((char)(c) - 'a' + 'A') : (char)(c))

#define VENTOY_CMD_RETURN(err)  grub_errno = (err); return (err)
#define VENTOY_FILE_TYPE    (GRUB_FILE_TYPE_NO_DECOMPRESS | GRUB_FILE_TYPE_LINUX_INITRD)

#define ventoy_env_op1(op, a) grub_env_##op(a)
#define ventoy_env_op2(op, a, b) grub_env_##op((a), (b))

#define ventoy_get_env(key)         ventoy_env_op1(get, key)
#define ventoy_set_env(key, val)    ventoy_env_op2(set, key, val)

typedef struct ventoy_initrd_ctx
{
    const char *path_prefix;
    const char *dir_prefix;
}ventoy_initrd_ctx;

typedef struct cmd_para
{
    const char *name;
    grub_extcmd_func_t func;
    grub_command_flags_t flags;
    const struct grub_arg_option *parser;
    
    const char *summary;
    const char *description;

    grub_extcmd_t cmd;
}cmd_para;

#define ventoy_align(value, align)  (((value) + ((align) - 1)) & (~((align) - 1)))

#pragma pack(1)
typedef struct cpio_newc_header 
{
    char  c_magic[6];
    char  c_ino[8];
    char  c_mode[8];
    char  c_uid[8];
    char  c_gid[8];
    char  c_nlink[8];
    char  c_mtime[8];
    char  c_filesize[8];
    char  c_devmajor[8];
    char  c_devminor[8];
    char  c_rdevmajor[8];
    char  c_rdevminor[8];
    char  c_namesize[8];
    char  c_check[8];
}cpio_newc_header;
#pragma pack()


#define cmd_raw_name ctxt->extcmd->cmd->name
#define check_free(p, func) if (p) { func(p); p = NULL; }

typedef int (*grub_char_check_func)(int c);
#define ventoy_is_decimal(str)  ventoy_string_check(str, grub_isdigit)


// El Torito Boot Record Volume Descriptor
#pragma pack(1)
typedef struct eltorito_descriptor
{
    grub_uint8_t type;
    grub_uint8_t id[5];
    grub_uint8_t version;
    grub_uint8_t system_id[32];
    grub_uint8_t reserved[32];
    grub_uint32_t sector;
}eltorito_descriptor;

typedef struct ventoy_iso9660_override
{
    grub_uint32_t first_sector;
    grub_uint32_t first_sector_be;
    grub_uint32_t size;
    grub_uint32_t size_be;
}ventoy_iso9660_override;

typedef struct ventoy_udf_override
{
    grub_uint32_t length;
    grub_uint32_t position;
}ventoy_udf_override;

#pragma pack()

typedef struct img_info
{
    char path[512];
    char name[256];
    int id;
    grub_uint64_t size;
    int select;

    void *parent;

    struct img_info *next;
    struct img_info *prev;
}img_info;

typedef struct img_iterator_node
{
    struct img_iterator_node *next;
    img_info **tail;
    char dir[400];
    int dirlen;
    int isocnt;
    int done;
    int select;

    struct img_iterator_node *parent;
    struct img_iterator_node *firstchild;
    
    void *firstiso;    
}img_iterator_node;



typedef struct initrd_info
{
    char name[256];

    grub_uint64_t offset;
    grub_uint64_t size;

    grub_uint8_t  iso_type; // 0: iso9660  1:udf
    grub_uint32_t udf_start_block;
    
    grub_uint64_t override_offset;
    grub_uint32_t override_length;
    char          override_data[32];

    struct initrd_info *next;
    struct initrd_info *prev;
}initrd_info;

extern initrd_info *g_initrd_img_list;
extern initrd_info *g_initrd_img_tail;
extern int g_initrd_img_count;
extern int g_valid_initrd_count;

extern img_info *g_ventoy_img_list;
extern int g_ventoy_img_count;

extern grub_uint8_t *g_ventoy_cpio_buf;
extern grub_uint32_t g_ventoy_cpio_size;
extern cpio_newc_header *g_ventoy_initrd_head;
extern grub_uint8_t *g_ventoy_runtime_buf;

extern ventoy_guid  g_ventoy_guid;

extern ventoy_img_chunk_list g_img_chunk_list;

extern int g_ventoy_debug;
void ventoy_debug(const char *fmt, ...);
#define debug(fmt, ...) if (g_ventoy_debug) ventoy_debug("[VTOY]: "fmt, __VA_ARGS__)



#define FLAG_HEADER_RESERVED          0x00000001
#define FLAG_HEADER_COMPRESSION       0x00000002
#define FLAG_HEADER_READONLY          0x00000004
#define FLAG_HEADER_SPANNED           0x00000008
#define FLAG_HEADER_RESOURCE_ONLY     0x00000010
#define FLAG_HEADER_METADATA_ONLY     0x00000020
#define FLAG_HEADER_WRITE_IN_PROGRESS 0x00000040
#define FLAG_HEADER_RP_FIX            0x00000080 // reparse point fixup
#define FLAG_HEADER_COMPRESS_RESERVED 0x00010000
#define FLAG_HEADER_COMPRESS_XPRESS   0x00020000
#define FLAG_HEADER_COMPRESS_LZX      0x00040000

#define RESHDR_FLAG_FREE 0x01
#define RESHDR_FLAG_METADATA 0x02
#define RESHDR_FLAG_COMPRESSED 0x04
#define RESHDR_FLAG_SPANNED 0x08

#pragma pack(1)

/* A WIM resource header */
typedef struct wim_resource_header 
{
    grub_uint64_t size_in_wim:56; /* Compressed length */
    grub_uint64_t flags:8;        /* flags  */
    grub_uint64_t offset;         /* Offset */
    grub_uint64_t raw_size;       /* Uncompressed length */
}wim_resource_header;

/* WIM resource header length mask */
#define WIM_RESHDR_ZLEN_MASK 0x00ffffffffffffffULL

/* WIM resource header flags */
typedef enum wim_resource_header_flags 
{
    WIM_RESHDR_METADATA = ( 0x02ULL << 56 ),       /* Resource contains metadata */
    WIM_RESHDR_COMPRESSED = ( 0x04ULL << 56 ),     /* Resource is compressed */
    WIM_RESHDR_PACKED_STREAMS = ( 0x10ULL << 56 ), /* Resource is compressed using packed streams */
}wim_resource_header_flags;

#define WIM_HEAD_SIGNATURE   "MSWIM\0\0"

/* WIM header */
typedef struct wim_header 
{
    grub_uint8_t signature[8];          /* Signature */
    grub_uint32_t header_len;           /* Header length */
    grub_uint32_t version;              /* Verson */
    grub_uint32_t flags;                /* Flags */
    grub_uint32_t chunk_len;            /* Chunk length */
    grub_uint8_t guid[16];              /* GUID */
    grub_uint16_t part;                 /* Part number */
    grub_uint16_t parts;                /* Total number of parts */
    grub_uint32_t images;               /* number of images */
    wim_resource_header lookup;    /* Lookup table */
    wim_resource_header xml;       /* XML data */
    wim_resource_header metadata;  /* Boot metadata */
    grub_uint32_t boot_index;           /* Boot index */
    wim_resource_header integrity; /* Integrity table */
    grub_uint8_t reserved[60];          /* Reserved */
} wim_header;

/* WIM header flags */
typedef enum wim_header_flags 
{
    WIM_HDR_XPRESS = 0x00020000, /* WIM uses Xpress compresson */
    WIM_HDR_LZX = 0x00040000,    /* WIM uses LZX compression */
}wim_header_flags;

/* A WIM file hash */
typedef struct wim_hash 
{
    /* SHA-1 hash */
    grub_uint8_t sha1[20];
}wim_hash;

/* A WIM lookup table entry */
typedef struct wim_lookup_entry 
{
    wim_resource_header resource; /* Resource header */
    grub_uint16_t part;           /* Part number */
    grub_uint32_t refcnt;         /* Reference count */
    wim_hash hash;                /* Hash */
}wim_lookup_entry;

/* WIM chunk length */
#define WIM_CHUNK_LEN 32768

/* A WIM chunk buffer */
typedef struct wim_chunk_buffer 
{
    grub_uint8_t data[WIM_CHUNK_LEN]; /*Data */
}wim_chunk_buffer;

/* Security data */
typedef struct wim_security_header 
{
    grub_uint32_t len;   /* Length */
    grub_uint32_t count; /* Number of entries */
}wim_security_header;

/* Directory entry */
typedef struct wim_directory_entry 
{
    grub_uint64_t len;                 /* Length */
    grub_uint32_t attributes;     /* Attributes */
    grub_uint32_t security;       /* Security ID */
    grub_uint64_t subdir;              /* Subdirectory offset */
    grub_uint8_t reserved1[16];   /* Reserved */
    grub_uint64_t created;             /* Creation time */
    grub_uint64_t accessed;            /* Last access time */
    grub_uint64_t written;             /* Last written time */
    wim_hash hash;                /* Hash */
    grub_uint8_t reserved2[12];   /* Reserved */
    grub_uint16_t streams;        /* Streams */
    grub_uint16_t short_name_len; /* Short name length */
    grub_uint16_t name_len;       /* Name length */
}wim_directory_entry;

/** Normal file */
#define WIM_ATTR_NORMAL 0x00000080UL

/** No security information exists for this file */
#define WIM_NO_SECURITY 0xffffffffUL

#pragma pack()


typedef struct wim_tail
{
    grub_uint32_t wim_raw_size;
    grub_uint32_t wim_align_size;

    grub_uint8_t  iso_type;
    grub_uint64_t file_offset;
    grub_uint32_t udf_start_block;
    grub_uint64_t fe_entry_size_offset;
    grub_uint64_t override_offset;
    grub_uint32_t override_len;
    grub_uint8_t  override_data[32];

    wim_header wim_header;

    wim_hash bin_hash;
    grub_uint32_t jump_exe_len;
    grub_uint8_t *jump_bin_data;
    grub_uint32_t bin_raw_len;
    grub_uint32_t bin_align_len;

    grub_uint8_t *new_meta_data;
    grub_uint32_t new_meta_len;
    grub_uint32_t new_meta_align_len;

    grub_uint8_t *new_lookup_data;
    grub_uint32_t new_lookup_len;
    grub_uint32_t new_lookup_align_len;
}wim_tail;



typedef enum _JSON_TYPE
{
    JSON_TYPE_NUMBER = 0,
    JSON_TYPE_STRING,
    JSON_TYPE_BOOL,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT,
    JSON_TYPE_NULL,
    JSON_TYPE_BUTT
}JSON_TYPE;


typedef struct _VTOY_JSON
{
    struct _VTOY_JSON *pstPrev;
    struct _VTOY_JSON *pstNext;
    struct _VTOY_JSON *pstChild;

    JSON_TYPE enDataType;
    union 
    {
        char  *pcStrVal;
        int   iNumVal;
        grub_uint64_t lValue;
    }unData;

    char *pcName;
}VTOY_JSON;

typedef struct _JSON_PARSE
{
    char *pcKey;
    void *pDataBuf;
    grub_uint32_t  uiBufSize;
}JSON_PARSE;

#define JSON_NEW_ITEM(pstJson, ret) \
{ \
    (pstJson) = (VTOY_JSON *)grub_zalloc(sizeof(VTOY_JSON)); \
    if (NULL == (pstJson)) \
    { \
        json_debug("Failed to alloc memory for json.\n"); \
        return (ret); \
    } \
}

typedef int (*ventoy_plugin_entry_pf)(VTOY_JSON *json, const char *isodisk);

typedef struct plugin_entry
{
    const char *key;
    ventoy_plugin_entry_pf entryfunc;
}plugin_entry;


void ventoy_fill_os_param(grub_file_t file, ventoy_os_param *param);
grub_err_t ventoy_cmd_isolinux_initrd_collect(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_grub_initrd_collect(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_specify_initrd_file(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_initrd_list(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_clear_initrd_list(grub_extcmd_context_t ctxt, int argc, char **args);
grub_uint32_t ventoy_get_iso_boot_catlog(grub_file_t file);
int ventoy_has_efi_eltorito(grub_file_t file, grub_uint32_t sector);
grub_err_t ventoy_cmd_linux_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_linux_locate_initrd(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_initrd_count(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_valid_initrd_count(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_load_cpio(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_cpio_newc_fill_head(void *buf, int filesize, const void *filedata, const char *name);
grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...);
int ventoy_is_file_exist(const char *fmt, ...);
int ventoy_fill_data(grub_uint32_t buflen, char *buffer);
grub_err_t ventoy_cmd_load_plugin(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wimdows_reset(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wimdows_locate_wim(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_windows_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);

VTOY_JSON *vtoy_json_find_item
(
    VTOY_JSON *pstJson,
    JSON_TYPE  enDataType,
    const char *szKey
);
int vtoy_json_parse_value
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
);
VTOY_JSON * vtoy_json_create(void);
int vtoy_json_parse(VTOY_JSON *pstJson, const char *szJsonData);

int vtoy_json_scan_parse
(
    const VTOY_JSON    *pstJson,
    grub_uint32_t       uiParseNum,
    JSON_PARSE         *pstJsonParse
);

int vtoy_json_scan_array
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstArrayItem
);

int vtoy_json_scan_array_ex
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstArrayItem
);
int vtoy_json_scan_object
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
    VTOY_JSON **ppstObjectItem
);
int vtoy_json_get_int
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    int *piValue
);
int vtoy_json_get_uint
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    grub_uint32_t *puiValue
);
int vtoy_json_get_uint64
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    grub_uint64_t *pui64Value
);
int vtoy_json_get_bool
(
    VTOY_JSON *pstJson,
    const char *szKey, 
    grub_uint8_t *pbValue
);
int vtoy_json_get_string
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     grub_uint32_t  uiBufLen,
     char *pcBuf
);
const char * vtoy_json_get_string_ex(VTOY_JSON *pstJson,  const char *szKey);
int vtoy_json_destroy(VTOY_JSON *pstJson);


grub_uint32_t CalculateCrc32
(
    const void       *Buffer,
    grub_uint32_t          Length,
    grub_uint32_t          InitValue
);

static inline int ventoy_isspace (int c)
{
    return (c == '\n' || c == '\r' || c == ' ' || c == '\t');
}

static inline int ventoy_is_word_end(int c)
{
    return (c == 0 || c == ',' || ventoy_isspace(c));    
}

#pragma pack(1)
typedef struct ventoy_part_table
{
    grub_uint8_t  Active; // 0x00  0x80

    grub_uint8_t  StartHead;
    grub_uint16_t StartSector : 6;
    grub_uint16_t StartCylinder : 10;

    grub_uint8_t  FsFlag;

    grub_uint8_t  EndHead;
    grub_uint16_t EndSector : 6;
    grub_uint16_t EndCylinder : 10;

    grub_uint32_t StartSectorId;
    grub_uint32_t SectorCount;
}ventoy_part_table;

typedef struct ventoy_mbr_head
{
    grub_uint8_t BootCode[446];
    ventoy_part_table PartTbl[4];
    grub_uint8_t Byte55;
    grub_uint8_t ByteAA;
}ventoy_mbr_head;
#pragma pack()


typedef struct install_template
{
    char isopath[256];
    char templatepath[256];

    struct install_template *next;
}install_template;

extern int g_ventoy_last_entry;
extern int g_ventoy_memdisk_mode;
extern int g_ventoy_iso_raw;
extern int g_ventoy_iso_uefi_drv;

int ventoy_cmp_img(img_info *img1, img_info *img2);
void ventoy_swap_img(img_info *img1, img_info *img2);
char * ventoy_plugin_get_install_template(const char *isopath);
void ventoy_plugin_dump_auto_install(void);
int ventoy_fill_windows_rtdata(void *buf, char *isopath);


#endif /* __VENTOY_DEF_H__ */

