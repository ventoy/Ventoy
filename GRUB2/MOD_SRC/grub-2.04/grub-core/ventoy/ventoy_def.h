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

#define VTOY_MAX_DIR_DEPTH  32

#define VTOY_MAX_SCRIPT_BUF    (4 * 1024 * 1024)

#define VTOY_PART_BUF_LEN  (128 * 1024)

#define VTOY_FILT_MIN_FILE_SIZE  32768

#define VTOY_SIZE_1GB     1073741824
#define VTOY_SIZE_1MB     (1024 * 1024)
#define VTOY_SIZE_2MB     (2 * 1024 * 1024)
#define VTOY_SIZE_4MB     (4 * 1024 * 1024)
#define VTOY_SIZE_512KB   (512 * 1024)
#define VTOY_SIZE_1KB     1024
#define VTOY_SIZE_32KB    (32 * 1024)

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

#define VTOY_WARNING  "!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!"

#define VTOY_PLAT_I386_UEFI     0x49413332
#define VTOY_PLAT_ARM64_UEFI    0x41413634
#define VTOY_PLAT_X86_64_UEFI   0x55454649
#define VTOY_PLAT_X86_LEGACY    0x42494f53
#define VTOY_PLAT_MIPS_UEFI     0x4D495053

#define VTOY_COMM_CPIO  "ventoy.cpio"
#if defined(__arm__) || defined(__aarch64__)
#define VTOY_ARCH_CPIO  "ventoy_arm64.cpio"
#elif defined(__mips__)
#define VTOY_ARCH_CPIO  "ventoy_mips64.cpio"
#else
#define VTOY_ARCH_CPIO  "ventoy_x86.cpio"
#endif

#define ventoy_varg_4(arg) arg[0], arg[1], arg[2], arg[3]
#define ventoy_varg_8(arg) arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]

#define VTOY_PWD_CORRUPTED(err) \
{\
    grub_printf("\n\n Password corrupted, will reboot after 5 seconds.\n\n"); \
    grub_refresh(); \
    grub_sleep(5); \
    grub_exit(); \
    return (err);\
}

typedef enum VTOY_FILE_FLT
{
    VTOY_FILE_FLT_ISO = 0, /* .iso */
    VTOY_FILE_FLT_WIM,     /* .wim */
    VTOY_FILE_FLT_EFI,     /* .efi */
    VTOY_FILE_FLT_IMG,     /* .img */
    VTOY_FILE_FLT_VHD,     /* .vhd(x) */
    VTOY_FILE_FLT_VTOY,    /* .vtoy */
    
    VTOY_FILE_FLT_BUTT
}VTOY_FILE_FLT;

#define FILE_FLT(type) (0 == g_vtoy_file_flt[VTOY_FILE_FLT_##type])

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

#define ventoy_align_2k(value)  ((value + 2047) / 2048 * 2048)
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
#define grub_check_free(p) if (p) { grub_free(p); p = NULL; }

typedef int (*grub_char_check_func)(int c);
#define ventoy_is_decimal(str)  ventoy_string_check(str, grub_isdigit)

#define OFFSET_OF(TYPE, MEMBER) ((grub_size_t) &((TYPE *)0)->MEMBER)

#pragma pack(1)
typedef struct ventoy_patch_vhd
{
    grub_uint8_t  part_offset_or_guid[16];
    grub_uint32_t reserved1;
    grub_uint32_t part_type;
    grub_uint8_t  disk_signature_or_guid[16];    
    grub_uint8_t  reserved2[16];
    grub_uint8_t  vhd_file_path[1];
}ventoy_patch_vhd;
#pragma pack()

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

typedef struct ventoy_iso9660_vd
{
    grub_uint8_t type;
    grub_uint8_t id[5];
    grub_uint8_t ver;
    grub_uint8_t res;
    char sys[32];
    char vol[32];
    grub_uint8_t res2[8];
    grub_uint32_t space;
}ventoy_iso9660_vd;

/* https://wiki.osdev.org/El-Torito */
typedef struct boot_info_table
{
    grub_uint32_t bi_data0;
    grub_uint32_t bi_data1;
    grub_uint32_t bi_PrimaryVolumeDescriptor;
    grub_uint32_t bi_BootFileLocation;
    grub_uint32_t bi_BootFileLength;
    grub_uint32_t bi_Checksum;
    grub_uint8_t bi_Reserved[40];
}boot_info_table;

#pragma pack()

#define img_type_start 0
#define img_type_iso   0
#define img_type_wim   1
#define img_type_efi   2
#define img_type_img   3
#define img_type_vhd   4
#define img_type_vtoy  5
#define img_type_max   6

typedef struct img_info
{
    int pathlen;
    char path[512];
    char name[256];

    const char *alias;
    const char *tip1;
    const char *tip2;
    const char *class;
    const char *menu_prefix;
    
    int id;
    int type;
    int plugin_list_index;
    grub_uint64_t size;
    int select;
    int unsupport;

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
    int level;
    int isocnt;
    int done;
    int select;

    int plugin_list_index;

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
extern ventoy_img_chunk_list g_wimiso_chunk_list;
extern char *g_wimiso_path;
extern grub_uint32_t g_wimiso_size;
extern char g_arch_mode_suffix[64];
extern const char *g_menu_prefix[img_type_max];

extern int g_ventoy_debug;
void ventoy_debug(const char *fmt, ...);
#define debug(fmt, args...) if (g_ventoy_debug) ventoy_debug("[VTOY]: "fmt, ##args)

#define vtoy_ssprintf(buf, pos, fmt, ...) \
    pos += grub_snprintf(buf + pos, VTOY_MAX_SCRIPT_BUF - pos, fmt, __VA_ARGS__)

#define browser_ssprintf(mbuf, fmt, args...) \
    (mbuf)->pos += grub_snprintf((mbuf)->buf + (mbuf)->pos, (mbuf)->max - (mbuf)->pos, fmt, ##args)

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
#define FLAG_HEADER_COMPRESS_LZMS     0x00080000

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

typedef struct wim_stream_entry 
{
    grub_uint64_t len;
    grub_uint64_t unused1;
    wim_hash hash;
    grub_uint16_t name_len;
    /* name */
}wim_stream_entry;

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

typedef struct reg_vk
{
    grub_uint32_t res1;
    grub_uint16_t sig;
    grub_uint16_t namesize;
    grub_uint32_t datasize;
    grub_uint32_t dataoffset;
    grub_uint32_t datatype;
    grub_uint16_t flag;
    grub_uint16_t res2;
}reg_vk;

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

typedef struct wim_patch
{
    int pathlen;
    char path[256];

    wim_hash old_hash;
    wim_tail wim_data;
    wim_lookup_entry *replace_look;

    int valid;

    struct wim_patch *next;
}wim_patch;


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
typedef int (*ventoy_plugin_check_pf)(VTOY_JSON *json, const char *isodisk);

typedef struct plugin_entry
{
    const char *key;
    ventoy_plugin_entry_pf entryfunc;
    ventoy_plugin_check_pf checkfunc;
    int flag;
}plugin_entry;

typedef struct replace_fs_dir
{
    grub_device_t dev;
    grub_fs_t fs;
    char fullpath[512];
    char initrd[512];
    int curpos;
    int dircnt;
    int filecnt;
}replace_fs_dir;

typedef struct chk_case_fs_dir
{
    grub_device_t dev;
    grub_fs_t fs;
}chk_case_fs_dir;

int ventoy_strcmp(const char *pattern, const char *str);
int ventoy_strncmp (const char *pattern, const char *str, grub_size_t n);
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
grub_err_t ventoy_cmd_append_ext_sector(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_skip_svd(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_cpio_busybox_64(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_trailer_cpio(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_cpio_newc_fill_head(void *buf, int filesize, const void *filedata, const char *name);
grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...);
grub_uint64_t ventoy_grub_get_file_size(const char *fmt, ...);
int ventoy_is_dir_exist(const char *fmt, ...);
int ventoy_fill_data(grub_uint32_t buflen, char *buffer);
grub_err_t ventoy_cmd_load_plugin(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wimdows_reset(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_is_pe64(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_windows_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_windows_wimboot_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wim_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wim_check_bootable(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_dump_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_sel_wimboot(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_set_wim_prompt(grub_extcmd_context_t ctxt, int argc, char **args);
grub_ssize_t ventoy_load_file_with_prompt(grub_file_t file, void *buf, grub_ssize_t size);
int ventoy_need_prompt_load_file(void);

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

typedef struct ventoy_gpt_head
{
    char   Signature[8]; /* EFI PART */
    grub_uint8_t  Version[4];
    grub_uint32_t Length;
    grub_uint32_t Crc;
    grub_uint8_t  Reserved1[4];
    grub_uint64_t EfiStartLBA;
    grub_uint64_t EfiBackupLBA;
    grub_uint64_t PartAreaStartLBA;
    grub_uint64_t PartAreaEndLBA;
    grub_uint8_t  DiskGuid[16];
    grub_uint64_t PartTblStartLBA;
    grub_uint32_t PartTblTotNum;
    grub_uint32_t PartTblEntryLen;
    grub_uint32_t PartTblCrc;
    grub_uint8_t  Reserved2[420];
}ventoy_gpt_head;

typedef struct ventoy_gpt_part_tbl
{
    grub_uint8_t  PartType[16];
    grub_uint8_t  PartGuid[16];
    grub_uint64_t StartLBA;
    grub_uint64_t LastLBA;
    grub_uint64_t Attr;
    grub_uint16_t Name[36];
}ventoy_gpt_part_tbl;

typedef struct ventoy_gpt_info
{
    ventoy_mbr_head MBR;
    ventoy_gpt_head Head;
    ventoy_gpt_part_tbl PartTbl[128];
}ventoy_gpt_info;

typedef struct vhd_footer_t
{
    char             cookie[8];    // Cookie
    grub_uint32_t    features;     // Features
    grub_uint32_t    ffversion;    // File format version
    grub_uint32_t    dataoffset;   // Data offset
    grub_uint32_t    timestamp;    // Timestamp
    grub_uint32_t    creatorapp;   // Creator application
    grub_uint32_t    creatorver;   // Creator version
    grub_uint32_t    creatorhos;   // Creator host OS
    grub_uint32_t    origsize;     // Original size
    grub_uint32_t    currsize;     // Current size
    grub_uint32_t    diskgeom;     // Disk geometry
    grub_uint32_t    disktype;     // Disk type
    grub_uint32_t    checksum;     // Checksum
    grub_uint8_t     uniqueid[16]; // Unique ID
    grub_uint8_t     savedst;      // Saved state
}vhd_footer_t;

#define VDI_IMAGE_FILE_INFO   "<<< Oracle VM VirtualBox Disk Image >>>\n"

/** Image signature. */
#define VDI_IMAGE_SIGNATURE   (0xbeda107f)

typedef struct VDIPREHEADER
{
    /** Just text info about image type, for eyes only. */
    char            szFileInfo[64];
    /** The image signature (VDI_IMAGE_SIGNATURE). */
    grub_uint32_t   u32Signature;
    /** The image version (VDI_IMAGE_VERSION). */
    grub_uint32_t   u32Version;
} VDIPREHEADER, *PVDIPREHEADER;

#pragma pack()

typedef struct ventoy_video_mode
{
    grub_uint32_t width;
    grub_uint32_t height;
    grub_uint32_t bpp;
}ventoy_video_mode;



typedef struct file_fullpath
{
    char path[256];
    int vlnk_add;
}file_fullpath;

typedef struct theme_list
{
    file_fullpath theme;
    struct theme_list *next;
}theme_list;

#define auto_install_type_file   0
#define auto_install_type_parent 1
typedef struct install_template
{
    int type;
    int pathlen;
    char isopath[256];

    int timeout;
    int autosel;
    int cursel;
    int templatenum;
    file_fullpath *templatepath;

    struct install_template *next;
}install_template;

typedef struct dudfile
{
    int size;
    char *buf;
}dudfile;

typedef struct dud
{
    int pathlen;
    char isopath[256];

    int dudnum;
    file_fullpath *dudpath;
    dudfile *files;

    struct dud *next;
}dud;

typedef struct persistence_config
{
    int pathlen;
    char isopath[256];

    int timeout;
    int autosel;
    int cursel;
    int backendnum;
    file_fullpath *backendpath;
    
    struct persistence_config *next;
}persistence_config;

#define vtoy_alias_image_file 0
#define vtoy_alias_directory  1

typedef struct menu_alias
{
    int type;
    int pathlen;
    char isopath[256];
    char alias[256];

    struct menu_alias *next;
}menu_alias;

#define vtoy_tip_image_file 0
#define vtoy_tip_directory  1
typedef struct menu_tip
{
    int type;
    int pathlen;
    char isopath[256];
    char tip1[1024];
    char tip2[1024];

    struct menu_tip *next;
}menu_tip;


#define vtoy_class_image_file  0
#define vtoy_class_directory   1

typedef struct menu_class
{
    int  type;
    int  patlen;
    int  parent;
    char pattern[256];
    char class[64];

    struct menu_class *next;
}menu_class;

#define vtoy_custom_boot_image_file  0
#define vtoy_custom_boot_directory   1

typedef struct custom_boot
{
    int  type;
    int  pathlen;
    char path[256];
    char cfg[256];

    struct custom_boot *next;
}custom_boot;

#define vtoy_max_replace_file_size  (2 * 1024 * 1024)
typedef struct conf_replace
{
    int pathlen;
    int img;
    char isopath[256];
    char orgconf[256];
    char newconf[256];

    struct conf_replace *next;
}conf_replace;

#define injection_type_file   0
#define injection_type_parent 1
typedef struct injection_config
{
    int type;
    int pathlen;
    char isopath[256];
    char archive[256];

    struct injection_config *next;
}injection_config;

typedef struct auto_memdisk
{
    int pathlen;
    char isopath[256];

    struct auto_memdisk *next;
}auto_memdisk;

typedef struct image_list
{
    int pathlen;
    char isopath[256];

    struct image_list *next;
}image_list;

#define VTOY_PASSWORD_NONE       0
#define VTOY_PASSWORD_TXT        1
#define VTOY_PASSWORD_MD5        2
#define VTOY_PASSWORD_SALT_MD5   3

typedef struct vtoy_password
{
    int type;
    char text[128];
    char salt[64];
    grub_uint8_t md5[16];
}vtoy_password;

#define vtoy_menu_pwd_file   0
#define vtoy_menu_pwd_parent 1

typedef struct menu_password
{
    int type;
    int pathlen;
    char isopath[256];

    vtoy_password password;

    struct menu_password *next;
}menu_password;

extern int g_ventoy_menu_esc;
extern int g_ventoy_suppress_esc;
extern int g_ventoy_suppress_esc_default;
extern int g_ventoy_last_entry;
extern int g_ventoy_memdisk_mode;
extern int g_ventoy_iso_raw;
extern int g_ventoy_grub2_mode;
extern int g_ventoy_wimboot_mode;
extern int g_ventoy_iso_uefi_drv;
extern int g_ventoy_case_insensitive;
extern grub_uint8_t g_ventoy_chain_type;
extern int g_vhdboot_enable;

#define VENTOY_IMG_WHITE_LIST   1
#define VENTOY_IMG_BLACK_LIST   2
extern int g_plugin_image_list;

extern ventoy_gpt_info *g_ventoy_part_info;
extern grub_uint64_t g_conf_replace_offset;
extern grub_uint64_t g_svd_replace_offset;
extern conf_replace *g_conf_replace_node;
extern grub_uint8_t *g_conf_replace_new_buf;
extern int g_conf_replace_new_len;
extern int g_conf_replace_new_len_align;
extern grub_uint64_t g_ventoy_disk_size;
extern grub_uint64_t g_ventoy_disk_part_size[2];
extern grub_uint32_t g_ventoy_plat_data;

#define ventoy_unix_fill_virt(new_data, new_len) \
{ \
    data_secs = (new_len + 2047) / 2048; \
    cur->mem_sector_start   = sector; \
    cur->mem_sector_end     = cur->mem_sector_start + data_secs; \
    cur->mem_sector_offset  = offset; \
    cur->remap_sector_start = 0; \
    cur->remap_sector_end   = 0; \
    cur->org_sector_start   = 0; \
    grub_memcpy(override + offset, new_data, new_len); \
    cur++; \
    sector += data_secs; \
    offset += new_len; \
    chain->virt_img_size_in_bytes += data_secs * 2048; \
}

#define ventoy_syscall0(name) grub_##name()
#define ventoy_syscall1(name, a) grub_##name(a)

void ventoy_str_tolower(char *str);
void ventoy_str_toupper(char *str);
char * ventoy_get_line(char *start);
int ventoy_cmp_img(img_info *img1, img_info *img2);
void ventoy_swap_img(img_info *img1, img_info *img2);
char * ventoy_plugin_get_cur_install_template(const char *isopath);
install_template * ventoy_plugin_find_install_template(const char *isopath);
persistence_config * ventoy_plugin_find_persistent(const char *isopath);
grub_uint64_t ventoy_get_vtoy_partsize(int part);
void ventoy_plugin_dump_injection(void);
void ventoy_plugin_dump_auto_install(void);
int ventoy_fill_windows_rtdata(void *buf, char *isopath);
int ventoy_plugin_get_persistent_chunklist(const char *isopath, int index, ventoy_img_chunk_list *chunk_list);
const char * ventoy_plugin_get_injection(const char *isopath);
const char * ventoy_plugin_get_menu_alias(int type, const char *isopath);
const menu_tip * ventoy_plugin_get_menu_tip(int type, const char *isopath);
const char * ventoy_plugin_get_menu_class(int type, const char *name, const char *path);
int ventoy_plugin_check_memdisk(const char *isopath);
int ventoy_plugin_get_image_list_index(int type, const char *name);
conf_replace * ventoy_plugin_find_conf_replace(const char *iso);
dud * ventoy_plugin_find_dud(const char *iso);
int ventoy_plugin_load_dud(dud *node, const char *isopart);
int ventoy_get_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist, grub_disk_addr_t start);
int ventoy_check_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist, grub_disk_addr_t start);
void ventoy_plugin_dump_persistence(void);
grub_err_t ventoy_cmd_set_theme(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_set_theme_path(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_select_theme_cfg(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_plugin_check_json(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_check_password(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_linux_get_main_initrd_index(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_collect_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_wim_patch_count(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_locate_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_get_disk_guid(const char *filename, grub_uint8_t *guid, grub_uint8_t *signature);
grub_err_t ventoy_cmd_unix_reset(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_replace_conf(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_replace_grub_conf(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_replace_ko(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_ko_fillmap(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_fill_image_desc(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_gzip_newko(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_freebsd_ver(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_parse_freenas_ver(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_unix_freebsd_ver_elf(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_check_device_result(int ret);
int ventoy_check_device(grub_device_t dev);
void ventoy_debug_dump_guid(const char *prefix, grub_uint8_t *guid);
grub_err_t ventoy_cmd_load_vhdboot(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_patch_vhdboot(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_raw_chain_data(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_get_vtoy_type(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_check_password(const vtoy_password *pwd, int retry);
int ventoy_plugin_add_custom_boot(const char *vcfgpath);
const char * ventoy_plugin_get_custom_boot(const char *isopath);
grub_err_t ventoy_cmd_dump_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_gzip_compress(void *mem_in, int mem_in_len, void *mem_out, int mem_out_len);
int ventoy_load_part_table(const char *diskname);
int ventoy_env_init(void);
int ventoy_register_all_cmd(void);
int ventoy_unregister_all_cmd(void);
int ventoy_chain_file_size(const char *path);
int ventoy_chain_file_read(const char *path, int offset, int len, void *buf);

#define VTOY_CMD_CHECK(a) if (33554432 != g_ventoy_disk_part_size[a]) ventoy_syscall0(exit)

#define vtoy_theme_random_boot_second  0
#define vtoy_theme_random_boot_day     1
#define vtoy_theme_random_boot_month   2

#define ventoy_env_export(env, name) \
{\
    grub_env_set((env), (name));\
    grub_env_export(env);\
}

#define ret_goto_end(a) ret = a; goto end;

extern ventoy_grub_param *g_grub_param;

#pragma pack(1)
#define VENTOY_UNIX_SEG_MAGIC0    0x11223344
#define VENTOY_UNIX_SEG_MAGIC1    0x55667788
#define VENTOY_UNIX_SEG_MAGIC2    0x99aabbcc
#define VENTOY_UNIX_SEG_MAGIC3    0xddeeff00
#define VENTOY_UNIX_MAX_SEGNUM    40960
struct g_ventoy_seg {
    grub_uint64_t seg_start_bytes;
    grub_uint64_t seg_end_bytes;
};

struct g_ventoy_map{
    grub_uint32_t magic1[4];
    grub_uint32_t magic2[4];
    grub_uint64_t segnum;
    grub_uint64_t disksize;
    grub_uint8_t diskuuid[16];
    struct g_ventoy_seg seglist[VENTOY_UNIX_MAX_SEGNUM];
    grub_uint32_t magic3[4];
};
#pragma pack()

typedef struct ventoy_vlnk_part
{
    grub_uint32_t disksig;
    grub_uint64_t partoffset;
    char disk[64];
    char device[64];
    grub_device_t dev;
    grub_fs_t fs;
    int probe;
    struct ventoy_vlnk_part *next;
}ventoy_vlnk_part;


typedef struct browser_mbuf
{
    int max;
    int pos;
    char *buf;
}browser_mbuf;

typedef struct browser_node
{
    int  dir;
    char menuentry[1024];
    char filename[512];
    struct browser_node *prev;
    struct browser_node *next;
}browser_node;

extern char *g_tree_script_buf;
extern int g_tree_script_pos;
extern int g_tree_script_pre;
extern int g_tree_view_menu_style;
extern int g_sort_case_sensitive;
extern int g_wimboot_enable;
extern int g_filt_dot_underscore_file;
extern int g_vtoy_file_flt[VTOY_FILE_FLT_BUTT];
extern const char *g_menu_class[img_type_max];
extern char g_iso_path[256];
int ventoy_add_vlnk_file(char *dir, const char *name);
grub_err_t ventoy_cmd_browser_dir(grub_extcmd_context_t ctxt, int argc, char **args);
grub_err_t ventoy_cmd_browser_disk(grub_extcmd_context_t ctxt, int argc, char **args);
int ventoy_get_fs_type(const char *fs);
int ventoy_img_name_valid(const char *filename, grub_size_t namelen);

#endif /* __VENTOY_DEF_H__ */

