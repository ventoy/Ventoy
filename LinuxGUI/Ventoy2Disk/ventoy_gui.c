#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h> 
#include <sys/utsname.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include "ventoy_json.h"

#define LIB_FLAG_GTK2   (1 << 0)
#define LIB_FLAG_GTK3   (1 << 1)
#define LIB_FLAG_GTK4   (1 << 2)
#define LIB_FLAG_QT4    (1 << 3)
#define LIB_FLAG_QT5    (1 << 4)
#define LIB_FLAG_QT6    (1 << 5)
#define LIB_FLAG_GLADE2 (1 << 30)

#define LIB_FLAG_GTK    (LIB_FLAG_GTK2 | LIB_FLAG_GTK3 | LIB_FLAG_GTK4)
#define LIB_FLAG_QT     (LIB_FLAG_QT4 | LIB_FLAG_QT5 | LIB_FLAG_QT6)

#define MAX_PARAS       64
#define MAX_LOG_BUF     (1024 * 1024)
#define VTOY_GUI_PATH   "_vtoy_gui_path_="
#define VTOY_ENV_STR    "_vtoy_env_str_="
#define LD_CACHE_FILE   "/etc/ld.so.cache"
#define INT2STR_YN(a)   ((a) == 0 ? "NO" : "YES")

static int g_xdg_log = 0;
static int g_xdg_ini = 0;
static char g_log_file[PATH_MAX];
static char g_ini_file[PATH_MAX];
static char *g_log_buf = NULL;
extern char ** environ;

#define CACHEMAGIC "ld.so-1.7.0"

struct file_entry
{
  int flags;		/* This is 1 for an ELF library.  */
  unsigned int key, value; /* String table indices.  */
};

struct cache_file
{
  char magic[sizeof CACHEMAGIC - 1];
  unsigned int nlibs;
  struct file_entry libs[0];
};

#define CACHEMAGIC_NEW "glibc-ld.so.cache"
#define CACHE_VERSION "1.1"
#define CACHEMAGIC_VERSION_NEW CACHEMAGIC_NEW CACHE_VERSION

struct file_entry_new
{
  int32_t flags;		/* This is 1 for an ELF library.  */
  uint32_t key, value;		/* String table indices.  */
  uint32_t osversion;		/* Required OS version.	 */
  uint64_t hwcap;		/* Hwcap entry.	 */
};

struct cache_file_new
{
  char magic[sizeof CACHEMAGIC_NEW - 1];
  char version[sizeof CACHE_VERSION - 1];
  uint32_t nlibs;		/* Number of entries.  */
  uint32_t len_strings;		/* Size of string table. */
  uint32_t unused[5];		/* Leave space for future extensions
				   and align to 8 byte boundary.  */
  struct file_entry_new libs[0]; /* Entries describing libraries.  */
  /* After this the string table of size len_strings is found.	*/
};

/* Used to align cache_file_new.  */
#define ALIGN_CACHE(addr)				\
(((addr) + __alignof__ (struct cache_file_new) -1)	\
 & (~(__alignof__ (struct cache_file_new) - 1)))

#define vlog(fmt, args...) ventoy_syslog(0, fmt, ##args)

void ventoy_syslog(int level, const char *Fmt, ...)
{
    int buflen;
    char *buf = NULL;
    char log[512];
    va_list arg;
    time_t stamp;
    struct tm ttm;
    FILE *fp;

    (void)level;
    
    time(&stamp);
    localtime_r(&stamp, &ttm);

    if (g_log_buf)
    {
        buf = g_log_buf;
        buflen = MAX_LOG_BUF;
    }
    else
    {
        buf = log;
        buflen = sizeof(log);
    }

    va_start(arg, Fmt);
    vsnprintf(buf, buflen, Fmt, arg);
    va_end(arg);

    fp = fopen(g_log_file, "a+");
    if (fp)
    {
        fprintf(fp, "[%04u/%02u/%02u %02u:%02u:%02u] %s", 
           ttm.tm_year + 1900, ttm.tm_mon, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           buf);
        fclose(fp);
    }

    #if 0
    printf("[%04u/%02u/%02u %02u:%02u:%02u] %s", 
           ttm.tm_year + 1900, ttm.tm_mon, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           buf);
    #endif
}

static int is_gtk_env(void)
{
    const char *env = NULL;
    
    env = getenv("GNOME_SETUP_DISPLAY");
    if (env && env[0] == ':')
    {
        vlog("GNOME_SETUP_DISPLAY=%s\n", env);
        return 1;
    }
    
    env = getenv("DESKTOP_SESSION");
    if (env && strcasecmp(env, "xfce") == 0)
    {
        vlog("DESKTOP_SESSION=%s\n", env);
        return 1;
    }

    return 0;
}

static int is_qt_env(void)
{
    return 0;
}

static int detect_gtk_version(int libflag)
{
    int gtk2;
    int gtk3;
    int gtk4;
    int glade2;

    gtk2 = libflag & LIB_FLAG_GTK2;
    gtk3 = libflag & LIB_FLAG_GTK3;
    gtk4 = libflag & LIB_FLAG_GTK4;
    glade2 = libflag & LIB_FLAG_GLADE2;

    if (gtk2 > 0 && glade2 > 0 && (gtk3 == 0 && gtk4 == 0))
    {
        return 2;
    }

    if (gtk3 > 0 && (gtk2 == 0 && gtk4 == 0))
    {
        return 3;
    }
    
    if (gtk4 > 0 && (gtk2 == 0 && gtk3 == 0))
    {
        return 4;
    }

    if (gtk3 > 0)
    {
        return 3;
    }

    if (gtk4 > 0)
    {
        return 4;
    }

    if (gtk2 > 0 && glade2 > 0)
    {
        return 2;
    }

    return 0;
}

static int detect_qt_version(int libflag)
{
    int qt4;
    int qt5;
    int qt6;

    qt4 = libflag & LIB_FLAG_QT4;
    qt5 = libflag & LIB_FLAG_QT5;
    qt6 = libflag & LIB_FLAG_QT6;

    if (qt4 > 0 && (qt5 == 0 && qt6 == 0))
    {
        return 4;
    }

    if (qt5 > 0 && (qt4 == 0 && qt6 == 0))
    {
        return 5;
    }
    
    if (qt6 > 0 && (qt4 == 0 && qt5 == 0))
    {
        return 6;
    }

    if (qt5 > 0)
    {
        return 5;
    }

    if (qt6 > 0)
    {
        return 6;
    }

    if (qt4 > 0)
    {
        return 4;
    }

    return 0;
}

int bit_from_machine(const char *machine)
{
    if (strstr(machine, "64"))
    {
        return 64;
    }
    else
    {
        return 32;
    }
}

int get_os_bit(int *bit)
{
    int ret;
    struct utsname unameData;

    memset(&unameData, 0, sizeof(unameData));
    ret = uname(&unameData);
    if (ret != 0)
    {
        vlog("uname error, code: %d\n", errno);
        return 1;
    }

    *bit = strstr(unameData.machine, "64") ? 64 : 32;
    vlog("uname -m <%s> %dbit\n", unameData.machine, *bit);
    
    return 0;
}

int read_file_1st_line(const char *file, char *buffer, int buflen)
{
    FILE *fp = NULL;

    fp = fopen(file, "r");
    if (fp == NULL)
    {
        vlog("Failed to open file %s code:%d", file, errno);
        return 1;
    }

    fgets(buffer, buflen, fp);
    fclose(fp);
    return 0;
}

static int read_pid_cmdline(long pid, char *Buffer, int BufLen)
{
    char path[256];
    
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
    return read_file_1st_line(path, Buffer, BufLen);
}

static int find_exe_path(const char *exe, char *pathbuf, int buflen)
{
    int i;
    char path[PATH_MAX];
    char *tmpptr = NULL;
    char *saveptr = NULL;
    char *newenv = NULL;
    const char *env = getenv("PATH");

    if (NULL == env)
    {
        return 0;
    }

    newenv = strdup(env);
    if (!newenv)
    {
        return 0;
    }

    tmpptr = newenv;
    while (NULL != (tmpptr = strtok_r(tmpptr, ":", &saveptr)))
	{
        snprintf(path, sizeof(path), "%s/%s", tmpptr, exe);
        if (access(path, F_OK) != -1)
        {
            snprintf(pathbuf, buflen, "%s", path);
            free(newenv);
            return 1;
        }
		tmpptr = NULL;
    }
    
    free(newenv);
    return 0;
}

void dump_args(const char *prefix, char **argv)
{
    int i = 0;
    
    vlog("=========%s ARGS BEGIN===========\n", prefix);
    while (argv[i])
    {
        vlog("argv[%d]=<%s>\n", i, argv[i]);
        i++;
    }
    vlog("=========%s ARGS END===========\n", prefix);
}

int pre_check(void)
{
    int ret;
    int bit;
    int buildbit;
    const char *env = NULL;

    env = getenv("DISPLAY");
    if (NULL == env || env[0] != ':')
    {
        vlog("DISPLAY not exist(%p). Not in X environment.\n", env);
        return 1;
    }

    ret = get_os_bit(&bit);
    if (ret)
    {
        vlog("Failed to get os bit.\n");
        return 1;
    }

    buildbit = strstr(VTOY_GUI_ARCH, "64") ? 64 : 32;
    vlog("Build bit is %d (%s)\n", buildbit, VTOY_GUI_ARCH);

    if (bit != buildbit)
    {
        vlog("Current system is %d bit (%s). Please run the correct VentoyGUI.\n", bit, VTOY_GUI_ARCH);
        return 1;
    }

    return 0;
}

static char * find_argv(int argc, char **argv, char *key)
{
    int i;
    int len;

    len = (int)strlen(key);
    for (i = 0; i < argc; i++)
    {
        if (strncmp(argv[i], key, len) == 0)
        {
            return argv[i];
        }
    }

    return NULL;
}

static int adjust_cur_dir(char *argv0)
{
    int ret = 2;
    char c;
    char *pos = NULL;
    char *end = NULL;

    if (argv0[0] == '.')
    {
        return 1;
    }

    for (pos = argv0; pos && *pos; pos++)
    {
        if (*pos == '/')
        {
            end = pos;
        }
    }

    if (end)
    {
        c = *end;
        *end = 0;
        ret = chdir(argv0);
        *end = c;
    }

    return ret;
}



static char **recover_environ_param(char *env)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int cnt = 0;
    char **newenvs = NULL;

    for (i = 0; env[i]; i++)
    {
        if (env[i] == '\n')
        {
            cnt++;
        }
    }

    newenvs = malloc(sizeof(char *) * (cnt + 1));
    if (!newenvs)
    {
        vlog("malloc new envs fail %d\n", cnt + 1);
        return NULL;
    }
    memset(newenvs, 0, sizeof(char *) * (cnt + 1));

    for (j = i = 0; env[i]; i++)
    {
        if (env[i] == '\n')
        {
            env[i] = 0;
            newenvs[k++] = env + j;
            j = i + 1;
        }
    }

    vlog("recover environ %d %d\n", cnt, k);
    return newenvs;
}

static int restart_main(int argc, char **argv, char *guiexe)
{
    int i = 0;
    int j = 0;
    char *para = NULL;
    char **envs = NULL;
    char *newargv[MAX_PARAS + 1] = { NULL };

    para = find_argv(argc, argv, VTOY_ENV_STR);
    if (!para)
    {
        vlog("failed to find %s\n", VTOY_ENV_STR);
        return 1;
    }

    newargv[j++] = guiexe;
    for (i = 1; i < argc && j < MAX_PARAS; i++)
    {
        if (strncmp(argv[i], "_vtoy_", 6) != 0)
        {
            newargv[j++] = argv[i];
        }
    }

    envs = recover_environ_param(para + strlen(VTOY_ENV_STR));
    if (envs)
    {
        vlog("recover success, argc=%d evecve <%s>\n", j, guiexe);
        dump_args("EXECVE", newargv);
        execve(guiexe, newargv, envs); 
    }
    else
    {
        vlog("recover failed, argc=%d evecv <%s>\n", j, guiexe);
        execv(guiexe, newargv); 
    }

    return 1;
}

static char *create_environ_param(const char *prefix, char **envs)
{
    int i = 0;
    int cnt = 0;
    int envlen = 0;
    int prelen = 0;
    char *cur = NULL;
    char *para = NULL;

    prelen = strlen(prefix);
    for (i = 0; envs[i]; i++)
    {
        cnt++;
        envlen += strlen(envs[i]) + 1;
    }

    para = malloc(prelen + envlen);
    if (!para)
    {
        vlog("failed to malloc env str %d\n", prelen + envlen);
        return NULL;
    }

    cur = para;
    memcpy(cur, prefix, prelen);
    cur += prelen;

    for (i = 0; envs[i]; i++)
    {
        envlen = strlen(envs[i]);
        memcpy(cur, envs[i], envlen);

        cur[envlen] = '\n';
        cur += envlen + 1;
    }

    vlog("create environment param %d\n", cnt);
    return para;
}

static int restart_by_pkexec(int argc, char **argv, const char *curpath, const char *exe)
{
    int i = 0;
    int j = 0;
    char envcount[64];
    char path[PATH_MAX];
    char pkexec[PATH_MAX];
    char exepara[PATH_MAX];
    char *newargv[MAX_PARAS + 1] = { NULL };

    vlog("try restart self by pkexec ...\n");

    if (find_exe_path("pkexec", pkexec, sizeof(pkexec)))
    {
        vlog("Find pkexec at <%s>\n", pkexec);
    }
    else
    {
        vlog("pkexec not found\n");
        return 1;
    }

    if (argv[0][0] != '/')
    {
        snprintf(path, sizeof(path), "%s/%s", curpath, argv[0]);
    }
    else
    {
        snprintf(path, sizeof(path), "%s", argv[0]);
    }
    snprintf(exepara, sizeof(exepara), "%s%s", VTOY_GUI_PATH, exe);

    newargv[j++] = pkexec;
    newargv[j++] = path;
    for (i = 1; i < argc && j < MAX_PARAS; i++)
    {
        if (strcmp(argv[i], "--xdg") == 0)
        {
            continue;
        }
        newargv[j++] = argv[i];
    }

    if (j < MAX_PARAS)
    {
        newargv[j++] = create_environ_param(VTOY_ENV_STR, environ);        
    }

    if (j < MAX_PARAS)
    {
        newargv[j++] = exepara;        
    }
    
    if (g_xdg_log && j + 1 < MAX_PARAS)
    {
        newargv[j++] = "-l";        
        newargv[j++] = g_log_file;        
    }
    
    if (g_xdg_ini && j + 1 < MAX_PARAS)
    {
        newargv[j++] = "-i";        
        newargv[j++] = g_ini_file;        
    }

    dump_args("PKEXEC", newargv);
    execv(pkexec, newargv);

    return 1;
}

static int ld_cache_lib_check(const char *lib, int *flag)
{
    if (((*flag) & LIB_FLAG_GTK3) == 0)
    {
        if (strncmp(lib, "libgtk-3.so", 11) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GTK3;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_GTK2) == 0)
    {
        if (strncmp(lib, "libgtk-x11-2.0.so", 17) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GTK2;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_GTK4) == 0)
    {
        if (strncmp(lib, "libgtk-4.so", 11) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GTK4;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_QT4) == 0)
    {
        if (strncmp(lib, "libQt4", 6) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_QT4;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_QT5) == 0)
    {
        if (strncmp(lib, "libQt5", 6) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_QT5;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_QT6) == 0)
    {
        if (strncmp(lib, "libQt6", 6) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_QT6;
            return 0;
        }
    }
    
    if (((*flag) & LIB_FLAG_GLADE2) == 0)
    {
        if (strncmp(lib, "libglade-2", 10) == 0)
        {
            vlog("LIB:<%s>\n", lib);
            *flag |= LIB_FLAG_GLADE2;
            return 0;
        }
    }

    return 0;
}

static int parse_ld_cache(int *flag)
{
    int fd;
    int format;
    unsigned int i;
    struct stat st;
    size_t offset = 0;
    size_t cache_size = 0;
    const char *cache_data = NULL;
    struct cache_file *cache = NULL;
    struct cache_file_new *cache_new = NULL;

    *flag = 0;
    
    fd = open(LD_CACHE_FILE, O_RDONLY);
    if (fd < 0)
    {
        vlog("failed to open %s err:%d\n", LD_CACHE_FILE, errno);
        return 1;
    }

    if (fstat(fd, &st) < 0 || st.st_size == 0)
    {
        close(fd);
        return 1;
    }

    cache = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (cache == MAP_FAILED)
    {
        close(fd);
        return 1;
    }

    cache_size = st.st_size;
    if (cache_size < sizeof (struct cache_file))
    {
        vlog("File is not a cache file.\n");
        munmap (cache, cache_size);
        close(fd);
        return 1;
    }

    if (memcmp(cache->magic, CACHEMAGIC, sizeof CACHEMAGIC - 1))
    {
        /* This can only be the new format without the old one.  */
        cache_new = (struct cache_file_new *) cache;

        if (memcmp(cache_new->magic, CACHEMAGIC_NEW, sizeof CACHEMAGIC_NEW - 1) ||
            memcmp (cache_new->version, CACHE_VERSION, sizeof CACHE_VERSION - 1))
        {
            munmap (cache, cache_size);
            close(fd);
            return 1;
        }
          
        format = 1;
        /* This is where the strings start.  */
        cache_data = (const char *) cache_new;
    }
    else
    {
        /* Check for corruption, avoiding overflow.  */
        if ((cache_size - sizeof (struct cache_file)) / sizeof (struct file_entry) < cache->nlibs)
        {
            vlog("File is not a cache file.\n");
            munmap (cache, cache_size);
            close(fd);
            return 1;
        }

        offset = ALIGN_CACHE(sizeof (struct cache_file) + (cache->nlibs * sizeof (struct file_entry)));
        
        /* This is where the strings start.  */
        cache_data = (const char *) &cache->libs[cache->nlibs];

        /* Check for a new cache embedded in the old format.  */
        if (cache_size > (offset + sizeof (struct cache_file_new)))
        {
            cache_new = (struct cache_file_new *) ((void *)cache + offset);

            if (memcmp(cache_new->magic, CACHEMAGIC_NEW, sizeof CACHEMAGIC_NEW - 1) == 0 &&
                memcmp(cache_new->version, CACHE_VERSION, sizeof CACHE_VERSION - 1) == 0)
            {
                cache_data = (const char *) cache_new;
                format = 1;
            }
        }
    }

    if (format == 0)
    {
        vlog("%d libs found in cache format 0\n", cache->nlibs);
        for (i = 0; i < cache->nlibs; i++)
        {
            ld_cache_lib_check(cache_data + cache->libs[i].key, flag);
        }
    }
    else if (format == 1)
    {
        vlog("%d libs found in cache format 1\n", cache_new->nlibs);

        for (i = 0; i < cache_new->nlibs; i++)
        {
            ld_cache_lib_check(cache_data + cache_new->libs[i].key, flag);
        }
    }

    vlog("ldconfig lib flags 0x%x\n", *flag);
    vlog("lib flags GLADE2:[%s] GTK2:[%s] GTK3:[%s] GTK4:[%s] QT4:[%s] QT5:[%s] QT6:[%s]\n", 
        INT2STR_YN((*flag) & LIB_FLAG_GLADE2), INT2STR_YN((*flag) & LIB_FLAG_GTK2),
        INT2STR_YN((*flag) & LIB_FLAG_GTK3), INT2STR_YN((*flag) & LIB_FLAG_GTK4),
        INT2STR_YN((*flag) & LIB_FLAG_QT4), INT2STR_YN((*flag) & LIB_FLAG_QT5),
        INT2STR_YN((*flag) & LIB_FLAG_QT6));

    munmap (cache, cache_size);
    close (fd);
    return 0;
}

static int gui_type_check(VTOY_JSON *pstNode)
{
    FILE *fp = NULL;
    const char *env = NULL;
    const char *arch = NULL;
    const char *srctype = NULL;
    const char *srcname = NULL;
    const char *condition = NULL;
    const char *expression = NULL;
    char line[1024];
    
    arch = vtoy_json_get_string_ex(pstNode, "arch");
    srctype = vtoy_json_get_string_ex(pstNode, "type");
    srcname = vtoy_json_get_string_ex(pstNode, "name");
    condition = vtoy_json_get_string_ex(pstNode, "condition");
    expression = vtoy_json_get_string_ex(pstNode, "expression");
    
    if (srctype == NULL || srcname == NULL || condition == NULL)
    {
        return 0;
    }

    if (arch && NULL == strstr(arch, VTOY_GUI_ARCH))
    {
        return 0;
    }

    vlog("check <%s> <%s> <%s>\n", srctype, srcname, condition);

    if (strcmp(srctype, "file") == 0)
    {
        if (access(srcname, F_OK) == -1)
        {
            return 0;
        }
    
        if (strcmp(condition, "exist") == 0)
        {
            vlog("File %s exist\n", srcname);
            return 1;
        }
        else if (strcmp(condition, "contains") == 0)
        {
            fp = fopen(srcname, "r");
            if (fp == NULL)
            {
                return 0;
            }

            while (fgets(line, sizeof(line), fp))
            {
                if (strstr(line, expression))
                {
                    vlog("File %s contains %s\n", srcname, expression);
                    fclose(fp);
                    return 1;
                }
            }

            fclose(fp);
            return 0;
        }
    }
    else if (strcmp(srctype, "env") == 0)
    {
        env = getenv(srcname);
        if (env == NULL)
        {
            return 0;
        }

        if (strcmp(condition, "exist") == 0)
        {
            vlog("env %s exist\n", srcname);
            return 1;
        }
        else if (strcmp(condition, "equal") == 0)
        {
            if (strcmp(expression, env) == 0)
            {
                vlog("env %s is %s\n", srcname, env);
                return 1;
            }
            return 0;
        }
        else if (strcmp(condition, "contains") == 0)
        {
            if (strstr(env, expression))
            {
                vlog("env %s is %s contains %s\n", srcname, env, expression);
                return 1;
            }
            return 0;
        }
    }
    
    return 0;
}

static int read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int FileSize;
    FILE *fp = NULL;
    void *Data = NULL;

    fp = fopen(FileName, "rb");
    if (fp == NULL)
    {
        vlog("Failed to open file %s", FileName);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);

    Data = malloc(FileSize + ExtLen);
    if (!Data)
    {
        fclose(fp);
        return 1;
    }

    fseek(fp, 0, SEEK_SET);
    fread(Data, 1, FileSize, fp);

    fclose(fp);

    *Bufer = Data;
    *BufLen = FileSize;

    return 0;
}

static int distro_check_gui_env(char *type, int len, int *pver)
{
    int size;
    int length;
    char *pBuf = NULL;
    VTOY_JSON *pstNode = NULL;
    VTOY_JSON *pstJson = NULL;

    vlog("distro_check_gui_env ...\n");

    if (access("./tool/distro_gui_type.json", F_OK) == -1)
    {
        vlog("distro_gui_type.json file not exist\n");
        return 0;
    }

    read_file_to_buf("./tool/distro_gui_type.json", 1, (void **)&pBuf, &size);
    pBuf[size] = 0;
    
    pstJson = vtoy_json_create();
    vtoy_json_parse(pstJson, pBuf);

    for (pstNode = pstJson->pstChild; pstNode; pstNode = pstNode->pstNext)
    {
        if (gui_type_check(pstNode->pstChild))
        {
            length = (int)snprintf(type, len, "%s", vtoy_json_get_string_ex(pstNode->pstChild, "gui"));
            *pver = type[length - 1] - '0';
            type[length - 1] = 0;
            break;
        }
    }

    vtoy_json_destroy(pstJson);
    return pstNode ? 1 : 0;
}

static int detect_gui_exe_path(int argc, char **argv, const char *curpath, char *pathbuf, int buflen)
{
    int i;
    int ret;
    int ver;
    int libflag = 0;
    const char *guitype = NULL;
    char line[256];
    mode_t mode;
    struct stat filestat;

    for (i = 1; i < argc; i++)
    {
        if (argv[i] && strcmp(argv[i], "--gtk2") == 0)
        {
            guitype = "gtk";
            ver = 2;
        }
        else if (argv[i] && strcmp(argv[i], "--gtk3") == 0)
        {
            guitype = "gtk";
            ver = 3;
        }
        else if (argv[i] && strcmp(argv[i], "--gtk4") == 0)
        {
            guitype = "gtk";
            ver = 4;
        }
        else if (argv[i] && strcmp(argv[i], "--qt4") == 0)
        {
            guitype = "qt";
            ver = 4;
        }
        else if (argv[i] && strcmp(argv[i], "--qt5") == 0)
        {
            guitype = "qt";
            ver = 5;
        }
        else if (argv[i] && strcmp(argv[i], "--qt6") == 0)
        {
            guitype = "qt";
            ver = 6;
        }
    }

    if (guitype)
    {
        vlog("Get GUI type from param <%s%d>.\n", guitype, ver);
    }
    else if (access("./ventoy_gui_type", F_OK) != -1)
    {
        vlog("Get GUI type from ventoy_gui_type file.\n");
    
        line[0] = 0;
        read_file_1st_line("./ventoy_gui_type", line, sizeof(line));
        if (strncmp(line, "gtk2", 4) == 0)
        {
            guitype = "gtk";
            ver = 2;

            parse_ld_cache(&libflag);
            if ((libflag & LIB_FLAG_GLADE2) == 0)
            {
                vlog("libglade2 is necessary for GTK2, but not found.\n");
                return 1;
            }
        }
        else if (strncmp(line, "gtk3", 4) == 0)
        {
            guitype = "gtk";
            ver = 3;
        }
        else if (strncmp(line, "gtk4", 4) == 0)
        {
            guitype = "gtk";
            ver = 4;
        }
        else if (strncmp(line, "qt4", 3) == 0)
        {
            guitype = "qt";
            ver = 4;
        }
        else if (strncmp(line, "qt5", 3) == 0)
        {
            guitype = "qt";
            ver = 5;
        }
        else if (strncmp(line, "qt6", 3) == 0)
        {
            guitype = "qt";
            ver = 6;
        }
        else
        {
            vlog("Current X environment is NOT supported.\n");
            return 1;
        }
    }
    else
    {
        vlog("Now detect the GUI type ...\n");

        parse_ld_cache(&libflag);

        if ((LIB_FLAG_GTK & libflag) > 0 && (LIB_FLAG_QT & libflag) == 0)
        {
            guitype = "gtk";
            ver = detect_gtk_version(libflag);
        }
        else if ((LIB_FLAG_GTK & libflag) == 0 && (LIB_FLAG_QT & libflag) > 0)
        {
            guitype = "qt";
            ver = detect_qt_version(libflag);
        }
        else if ((LIB_FLAG_GTK & libflag) > 0 && (LIB_FLAG_QT & libflag) > 0)
        {
            if (distro_check_gui_env(line, sizeof(line), &ver))
            {
                guitype = line;
                vlog("distro_check_gui <%s%d> ...\n", line, ver);
            }
            else if (is_gtk_env())
            {
                guitype = "gtk";
                ver = detect_gtk_version(libflag);
            }
            else if (is_qt_env())
            {
                guitype = "qt";
                ver = detect_qt_version(libflag);
            }
            else
            {
                vlog("Can not distinguish GTK and QT, default use GTK.\n");
                guitype = "gtk";
                ver = detect_gtk_version(libflag);
            }
        }
        else
        {
            vlog("Current X environment is NOT supported.\n");
            return 1;
        }
    }

    snprintf(pathbuf, buflen, "%s/tool/%s/Ventoy2Disk.%s%d", curpath, VTOY_GUI_ARCH, guitype, ver);

    vlog("This is %s%d X environment.\n", guitype, ver);
    vlog("exe = %s\n", pathbuf);
    
    if (access(pathbuf, F_OK) == -1)
    {
        vlog("%s is not exist.\n", pathbuf);
        return 1;
    }

    if (access(pathbuf, X_OK) == -1)
    {
        vlog("execute permission check fail, try chmod.\n", pathbuf);
        if (stat(pathbuf, &filestat) == 0)
        {
            mode = filestat.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
            ret = chmod(pathbuf, mode);
            vlog("old mode=%o new mode=%o ret=%d\n", filestat.st_mode, mode, ret);
        }
    }
    else
    {
        vlog("execute permission check success.\n");
    }

    return 0;
}

int real_main(int argc, char **argv)
{
    int ret;
    int euid;
    char *exe = NULL;
    char path[PATH_MAX];
    char curpath[PATH_MAX];

    ret = adjust_cur_dir(argv[0]);

    vlog("\n");
    vlog("=========================================================\n");
    vlog("=========================================================\n");
    vlog("=============== VentoyGui %s ===============\n", VTOY_GUI_ARCH);
    vlog("=========================================================\n");
    vlog("=========================================================\n");
    vlog("log file is <%s>\n", g_log_file);

    euid = geteuid();
    getcwd(curpath, sizeof(curpath));

    vlog("pid:%ld ppid:%ld uid:%d euid:%d\n", (long)getpid(), (long)getppid(), getuid(), euid);
    vlog("adjust dir:%d  current path:%s\n", ret, curpath);
    dump_args("RAW", argv);

    if (access("./boot/boot.img", F_OK) == -1)
    {
        vlog("Please run under the correct directory!\n");
        return 1;
    }

    exe = find_argv(argc, argv, VTOY_GUI_PATH);
    if (exe)
    {
        if (euid != 0)
        {
            vlog("Invalid euid %d when restart.\n", euid);
            return 1;
        }

        return restart_main(argc, argv, exe + strlen(VTOY_GUI_PATH));
    }
    else
    {
        if (pre_check())
        {
            return 1;
        }

        if (detect_gui_exe_path(argc, argv, curpath, path, sizeof(path)))
        {
            return 1;
        }

        if (euid == 0)
        {
            vlog("We have root privileges, just exec %s\n", path);
            argv[0] = path;
            execv(argv[0], argv);
        }
        else
        {
            vlog("EUID check failed.\n");

            /* try pkexec */
            restart_by_pkexec(argc, argv, curpath, path);

            vlog("### Please run with root privileges. ###\n");
            return 1;
        }
    }

    return 1;
}

int main(int argc, char **argv)
{
    int i;
    int ret;
    const char *env = NULL;

    snprintf(g_log_file, sizeof(g_log_file), "log.txt");
    for (i = 0; i < argc; i++)
    {
        if (argv[i] && argv[i + 1] && strcmp(argv[i], "-l") == 0)
        {
            snprintf(g_log_file, sizeof(g_log_file), "%s", argv[i + 1]);
            break;
        }
        else if (argv[i] && strcmp(argv[i], "--xdg") == 0)
        {
            env = getenv("XDG_CACHE_HOME");
            if (env)
            {
                g_xdg_log = 1;
                snprintf(g_log_file, sizeof(g_log_file), "%s/ventoy.log", env);
            }
            
            env = getenv("XDG_CONFIG_HOME");
            if (env)
            {
                g_xdg_ini = 1;
                snprintf(g_ini_file, sizeof(g_ini_file), "%s/Ventoy2Disk.ini", env);
            }
        }
    }

    g_log_buf = malloc(MAX_LOG_BUF);
    if (!g_log_buf)
    {
        vlog("Failed to malloc log buffer %d\n", MAX_LOG_BUF);
        return 1;
    }

    ret = real_main(argc, argv);
    free(g_log_buf);
    return ret;
}

