#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include <dirent.h>
#include <unistd.h>

#include "vlnk.h"

#ifndef PATH_MAX
#define PATH_MAX  4096
#endif

#define IS_DIGIT(x) ((x) >= '0' && (x) <= '9')

static int verbose = 0;
#define debug(fmt, args...) if(verbose) printf(fmt, ##args)

static uint8_t g_vlnk_buf[VLNK_FILE_LEN];

static int64_t get_file_size(char *file)
{
    struct stat stStat;
    
    if (stat(file, &stStat) < 0)
    {
        return -1;
    }

    return (int64_t)(stStat.st_size);
}

static int get_disk_sig(char *diskname, uint32_t *sig)
{
    int fd;
    uint8_t buf[512] = {0};
    
    fd = open(diskname, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open %s\n", diskname);
        return 1;
    }

    read(fd, buf, 512);
    close(fd);

    memcpy(sig, buf + 0x1b8, 4);    
    return 0;
}

static int vtoy_get_disk_guid(const char *diskname, uint8_t *vtguid, uint8_t *vtsig)
{
    int i = 0;
    int fd = 0;
    char devdisk[128] = {0};

    snprintf(devdisk, sizeof(devdisk) - 1, "/dev/%s", diskname);
    
    fd = open(devdisk, O_RDONLY);
    if (fd >= 0)
    {
        lseek(fd, 0x180, SEEK_SET);
        read(fd, vtguid, 16);
        
        lseek(fd, 0x1b8, SEEK_SET);
        read(fd, vtsig, 4);
        close(fd);
        return 0;
    }
    else
    {
        debug("failed to open %s %d\n", devdisk, errno);
        return errno;
    }
}

static int vtoy_is_possible_blkdev(const char *name)
{
    if (name[0] == '.')
    {
        return 0;
    }

    /* /dev/ramX */
    if (name[0] == 'r' && name[1] == 'a' && name[2] == 'm')
    {
        return 0;
    }

    /* /dev/loopX */
    if (name[0] == 'l' && name[1] == 'o' && name[2] == 'o' && name[3] == 'p')
    {
        return 0;
    }

    /* /dev/dm-X */
    if (name[0] == 'd' && name[1] == 'm' && name[2] == '-' && IS_DIGIT(name[3]))
    {
        return 0;
    }

    /* /dev/srX */
    if (name[0] == 's' && name[1] == 'r' && IS_DIGIT(name[2]))
    {
        return 0;
    }
    
    return 1;
}


static int find_disk_by_sig(uint8_t *sig, char *diskname)
{
    int rc = 0;
    int count = 0;
    DIR* dir = NULL;
    struct dirent* p = NULL;
    uint8_t vtguid[16];
    uint8_t vtsig[16];

    dir = opendir("/sys/block");
    if (!dir)
    {
        return 0;
    }
    
    while ((p = readdir(dir)) != NULL)
    {
        if (!vtoy_is_possible_blkdev(p->d_name))
        {
            continue;
        }
    
        rc = vtoy_get_disk_guid(p->d_name, vtguid, vtsig);
        if (rc == 0 && memcmp(vtsig, sig, 4) == 0)
        {
            sprintf(diskname, "%s", p->d_name);
            count++;
        }
    }
    closedir(dir);
    
    return count;    
}

static uint64_t get_part_offset(char *partname)
{
    int fd;
    uint64_t offset;
    char buf[32] = {0};
    char path[PATH_MAX];

    snprintf(path, PATH_MAX - 1, "/sys/class/block/%s/start", partname);

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }

    read(fd, buf, sizeof(buf));
    close(fd);

    offset = (uint64_t)strtoull(buf, NULL, 10);
    offset *= 512;

    return offset;
}

static int create_vlnk(char *infile, char *diskname, uint64_t partoff, char *outfile)
{
    FILE *fp;
    int len;
    uint32_t sig = 0;

    debug("create vlnk\n");
    
    if (infile[0] == 0 || outfile[0] == 0 || diskname[0] == 0 || partoff == 0)
    {
        debug("Invalid parameters: %d %d %d %llu\n", infile[0], outfile[0], diskname[0], (unsigned long long)partoff);
        return 1;
    }

    len = (int)strlen(infile);
    if (len >= VLNK_NAME_MAX)
    {
        printf("File name length %d is too long for vlnk!\n", len);
        return 1;
    }

    if (get_disk_sig(diskname, &sig))
    {
        printf("Failed to read disk sig\n");
        return 1;
    }

    fp = fopen(outfile, "wb+");
    if (!fp)
    {
        printf("Failed to create file %s\n", outfile);
        return 1;
    }

    memset(g_vlnk_buf, 0, sizeof(g_vlnk_buf));
    ventoy_create_vlnk(sig, partoff, infile, (ventoy_vlnk *)g_vlnk_buf);
    fwrite(g_vlnk_buf, 1, VLNK_FILE_LEN, fp);
    fclose(fp);

    return 0;
}

static int get_mount_point(char *partname, char *mntpoint)
{
    int i;
    int len;
    int rc = 1;
    FILE *fp = NULL;
    char line[PATH_MAX];

    fp = fopen("/proc/mounts", "r");
    if (!fp)
    {
        return 1;
    }

    len = (int)strlen(partname);
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, partname, len) == 0)
        {
            for (i = len; i < PATH_MAX && line[i]; i++)
            {
                if (line[i] == ' ')
                {
                    line[i] = 0;
                    rc = 0;
                    strncpy(mntpoint, line + len, PATH_MAX - 1);                    
                    break;
                }
            }
            break;
        }
    }

    fclose(fp);
    return rc;
}

static int parse_vlnk(char *infile)
{
    int i;
    int fd;
    int cnt;
    int pflag = 0;
    char diskname[128] = {0};
    char partname[128] = {0};
    char partpath[256] = {0};
    char mntpoint[PATH_MAX];
    ventoy_vlnk vlnk;

    debug("parse vlnk\n");
    
    if (infile[0] == 0)
    {
        debug("input file null\n");
        return 1;
    }

    fd = open(infile, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open file %s error %d\n", infile, errno);
        return 1;
    }

    memset(&vlnk, 0, sizeof(vlnk));
    read(fd, &vlnk, sizeof(vlnk));
    close(fd);

    debug("disk_signature:%08X\n", vlnk.disk_signature);
    debug("file path:<%s>\n", vlnk.filepath);
    debug("part offset: %llu\n", (unsigned long long)vlnk.part_offset);
    
    cnt = find_disk_by_sig((uint8_t *)&(vlnk.disk_signature), diskname);
    if (cnt != 1)
    {
        printf("Disk in vlnk not found!\n");
        return 1;
    }

    debug("Disk is <%s>\n", diskname);

    if (strstr(diskname, "nvme") || strstr(diskname, "mmc") || strstr(diskname, "nbd"))
    {
        pflag = 1;
    }

    for (i = 1; i <= 128; i++)
    {
        if (pflag)
        {
            snprintf(partname, sizeof(partname) - 1, "%sp%d", diskname, i);            
        }
        else
        {
            snprintf(partname, sizeof(partname) - 1, "%s%d", diskname, i);            
        }

        if (get_part_offset(partname) == vlnk.part_offset)
        {
            debug("Find correct partition </dev/%s>\n", partname);
            break;
        }
    }

    if (i > 128)
    {
        printf("Partition in vlnk not found!");
        return 1;
    }

    snprintf(partpath, sizeof(partpath), "/dev/%s ", partname);
    if (get_mount_point(partpath, mntpoint))
    {
        printf("Mountpoint of %s is not found!\n", partpath);
        return 1;
    }
    debug("moutpoint of %s is <%s>\n", partpath, mntpoint);

    strcat(mntpoint, vlnk.filepath);
    printf("Vlnk Point: %s\n", mntpoint);
    if (access(mntpoint, F_OK) >= 0)
    {
        printf("File Exist: YES\n");    
    }
    else
    {
        printf("File Exist: NO\n");    
    }

    return 0;
}

static int check_vlnk(char *infile)
{
    int fd;
    int64_t size;
    ventoy_vlnk vlnk;

    debug("check vlnk\n");
    
    if (infile[0] == 0)
    {
        debug("input file null\n");
        return 1;
    }

    size = get_file_size(infile);
    if (size != VLNK_FILE_LEN)
    {
        debug("file size %lld is not a vlnk file size\n", (long long)size);
        return 1;
    }

    fd = open(infile, O_RDONLY);
    if (fd < 0)
    {
        debug("Failed to open file %s error %d\n", infile, errno);
        return 1;
    }

    memset(&vlnk, 0, sizeof(vlnk));
    read(fd, &vlnk, sizeof(vlnk));
    close(fd);

    if (CheckVlnkData(&vlnk))
    {
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    int ch = 0;
    int cmd = 0;
    uint64_t partoff = 0;
    char infile[PATH_MAX] = {0};
    char outfile[PATH_MAX] = {0};
    char diskname[256] = {0};

    while ((ch = getopt(argc, argv, "c:t:l:d:p:o:v::")) != -1)
    {
        if (ch == 'c')
        {
            cmd = 1;
            strncpy(infile, optarg, sizeof(infile) - 1);
        }
        else if (ch == 'o')
        {
            strncpy(outfile, optarg, sizeof(outfile) - 1);
        }
        else if (ch == 'l')
        {
            cmd = 2;
            strncpy(infile, optarg, sizeof(infile) - 1);
        }
        else if (ch == 't')
        {
            cmd = 3;
            strncpy(infile, optarg, sizeof(infile) - 1);
        }        
        else if (ch == 'd')
        {
            strncpy(diskname, optarg, sizeof(diskname) - 1);
        }
        else if (ch == 'p')
        {
            partoff = (uint64_t)strtoull(optarg, NULL, 10);
            partoff *= 512;
        }
        else if (ch == 'v')
        {
            verbose = 1;
        }
        else
        {
            return 1;
        }
    }

    if (cmd == 1)
    {
        return create_vlnk(infile, diskname, partoff, outfile);
    }
    else if (cmd == 2)
    {
        return parse_vlnk(infile);
    }
    else if (cmd == 3)
    {
        return check_vlnk(infile);
    }
    else
    {
        printf("Invalid command %d\n", cmd);
        return 1;
    }
}

