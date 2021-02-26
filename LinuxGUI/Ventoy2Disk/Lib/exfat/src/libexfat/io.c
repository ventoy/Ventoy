/*
	io.c (02.09.09)
	exFAT file system implementation library.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "exfat.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#if defined(__APPLE__)
#include <sys/disk.h>
#elif defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>
#elif __linux__
#include <sys/mount.h>
#endif
#ifdef USE_UBLIO
#include <sys/uio.h>
#include <ublio.h>
#endif

struct exfat_dev
{
	int fd;
	enum exfat_mode mode;
	off_t size; /* in bytes */
#ifdef USE_UBLIO
	off_t pos;
	ublio_filehandle_t ufh;
#endif
};

int g_vtoy_exfat_disk_fd = -1;
uint64_t g_vtoy_exfat_part_size = 0;

static bool is_open(int fd)
{
	return fcntl(fd, F_GETFD) != -1;
}

static int open_ro(const char* spec)
{
	return open(spec, O_RDONLY);
}

static int open_rw(const char* spec)
{
	int fd = open(spec, O_RDWR);
#ifdef __linux__
	int ro = 0;

	/*
	   This ioctl is needed because after "blockdev --setro" kernel still
	   allows to open the device in read-write mode but fails writes.
	*/
	if (fd != -1 && ioctl(fd, BLKROGET, &ro) == 0 && ro)
	{
		close(fd);
		errno = EROFS;
		return -1;
	}
#endif
	return fd;
}

struct exfat_dev* exfat_open(const char* spec, enum exfat_mode mode)
{
	struct exfat_dev* dev;
	struct stat stbuf;
#ifdef USE_UBLIO
	struct ublio_param up;
#endif

	/* The system allocates file descriptors sequentially. If we have been
	   started with stdin (0), stdout (1) or stderr (2) closed, the system
	   will give us descriptor 0, 1 or 2 later when we open block device,
	   FUSE communication pipe, etc. As a result, functions using stdin,
	   stdout or stderr will actually work with a different thing and can
	   corrupt it. Protect descriptors 0, 1 and 2 from such misuse. */
	while (!is_open(STDIN_FILENO)
		|| !is_open(STDOUT_FILENO)
		|| !is_open(STDERR_FILENO))
	{
		/* we don't need those descriptors, let them leak */
		if (open("/dev/null", O_RDWR) == -1)
		{
			exfat_error("failed to open /dev/null");
			return NULL;
		}
	}

	dev = malloc(sizeof(struct exfat_dev));
	if (dev == NULL)
	{
		exfat_error("failed to allocate memory for device structure");
		return NULL;
	}

	switch (mode)
	{
	case EXFAT_MODE_RO:
		dev->fd = g_vtoy_exfat_disk_fd < 0 ? open_ro(spec) : g_vtoy_exfat_disk_fd;
		if (dev->fd == -1)
		{
			free(dev);
			exfat_error("failed to open '%s' in read-only mode: %s", spec,
					strerror(errno));
			return NULL;
		}
		dev->mode = EXFAT_MODE_RO;
		break;
	case EXFAT_MODE_RW:
		dev->fd = g_vtoy_exfat_disk_fd < 0 ? open_rw(spec) : g_vtoy_exfat_disk_fd;
		if (dev->fd == -1)
		{
			free(dev);
			exfat_error("failed to open '%s' in read-write mode: %s", spec,
					strerror(errno));
			return NULL;
		}
		dev->mode = EXFAT_MODE_RW;
		break;
	case EXFAT_MODE_ANY:
		dev->fd = g_vtoy_exfat_disk_fd < 0 ? open_rw(spec) : g_vtoy_exfat_disk_fd;
		if (dev->fd != -1)
		{
			dev->mode = EXFAT_MODE_RW;
			break;
		}
		dev->fd = g_vtoy_exfat_disk_fd < 0 ? open_ro(spec) : g_vtoy_exfat_disk_fd;
		if (dev->fd != -1)
		{
			dev->mode = EXFAT_MODE_RO;
			exfat_warn("'%s' is write-protected, mounting read-only", spec);
			break;
		}
		free(dev);
		exfat_error("failed to open '%s': %s", spec, strerror(errno));
		return NULL;
	}

	if (fstat(dev->fd, &stbuf) != 0)
	{
		close(dev->fd);
		free(dev);
		exfat_error("failed to fstat '%s'", spec);
		return NULL;
	}
	if (!S_ISBLK(stbuf.st_mode) &&
		!S_ISCHR(stbuf.st_mode) &&
		!S_ISREG(stbuf.st_mode))
	{
		close(dev->fd);
		free(dev);
		exfat_error("'%s' is neither a device, nor a regular file", spec);
		return NULL;
	}

#if defined(__APPLE__)
	if (!S_ISREG(stbuf.st_mode))
	{
		uint32_t block_size = 0;
		uint64_t blocks = 0;

		if (ioctl(dev->fd, DKIOCGETBLOCKSIZE, &block_size) != 0)
		{
			close(dev->fd);
			free(dev);
			exfat_error("failed to get block size");
			return NULL;
		}
		if (ioctl(dev->fd, DKIOCGETBLOCKCOUNT, &blocks) != 0)
		{
			close(dev->fd);
			free(dev);
			exfat_error("failed to get blocks count");
			return NULL;
		}
		dev->size = blocks * block_size;
	}
	else
#elif defined(__OpenBSD__)
	if (!S_ISREG(stbuf.st_mode))
	{
		struct disklabel lab;
		struct partition* pp;
		char* partition;

		if (ioctl(dev->fd, DIOCGDINFO, &lab) == -1)
		{
			close(dev->fd);
			free(dev);
			exfat_error("failed to get disklabel");
			return NULL;
		}

		/* Don't need to check that partition letter is valid as we won't get
		   this far otherwise. */
		partition = strchr(spec, '\0') - 1;
		pp = &(lab.d_partitions[*partition - 'a']);
		dev->size = DL_GETPSIZE(pp) * lab.d_secsize;

		if (pp->p_fstype != FS_NTFS)
			exfat_warn("partition type is not 0x07 (NTFS/exFAT); "
					"you can fix this with fdisk(8)");
	}
	else
#endif
	{
		/* works for Linux, FreeBSD, Solaris */
		dev->size = exfat_seek(dev, 0, SEEK_END);
		if (dev->size <= 0)
		{
			close(dev->fd);
			free(dev);
			exfat_error("failed to get size of '%s'", spec);
			return NULL;
		}
		if (exfat_seek(dev, 0, SEEK_SET) == -1)
		{
			close(dev->fd);
			free(dev);
			exfat_error("failed to seek to the beginning of '%s'", spec);
			return NULL;
		}
	}

#ifdef USE_UBLIO
	memset(&up, 0, sizeof(struct ublio_param));
	up.up_blocksize = 256 * 1024;
	up.up_items = 64;
	up.up_grace = 32;
	up.up_priv = &dev->fd;

	dev->pos = 0;
	dev->ufh = ublio_open(&up);
	if (dev->ufh == NULL)
	{
		close(dev->fd);
		free(dev);
		exfat_error("failed to initialize ublio");
		return NULL;
	}
#endif

	return dev;
}

int exfat_close(struct exfat_dev* dev)
{
	int rc = 0;

#ifdef USE_UBLIO
	if (ublio_close(dev->ufh) != 0)
	{
		exfat_error("failed to close ublio");
		rc = -EIO;
	}
#endif
    if (dev->fd != g_vtoy_exfat_disk_fd)
    {
        if (close(dev->fd) != 0)
    	{
    		exfat_error("failed to close device: %s", strerror(errno));
    		rc = -EIO;
    	}
    }
	
	free(dev);
	return rc;
}

int exfat_fsync(struct exfat_dev* dev)
{
	int rc = 0;

#ifdef USE_UBLIO
	if (ublio_fsync(dev->ufh) != 0)
	{
		exfat_error("ublio fsync failed");
		rc = -EIO;
	}
#endif
	if (fsync(dev->fd) != 0)
	{
		exfat_error("fsync failed: %s", strerror(errno));
		rc = -EIO;
	}
	return rc;
}

enum exfat_mode exfat_get_mode(const struct exfat_dev* dev)
{
	return dev->mode;
}


off_t exfat_get_size(const struct exfat_dev* dev)
{
	return dev->size;
}

off_t exfat_seek(struct exfat_dev* dev, off_t offset, int whence)
{
#ifdef USE_UBLIO
	/* XXX SEEK_CUR will be handled incorrectly */
	return dev->pos = lseek(dev->fd, offset, whence);
#else

    if (SEEK_SET == whence)
    {
        if (offset > g_vtoy_exfat_part_size)
        {
            return -1;
        }
        
        lseek(dev->fd, 512 * 2048 + offset, SEEK_SET);
        return offset;
    }
    else if (SEEK_END == whence)
    {
        if (offset == 0)
        {
            offset = 512 * 2048 + g_vtoy_exfat_part_size;
            lseek(dev->fd, offset, SEEK_SET);
            return (off_t)g_vtoy_exfat_part_size;
        }
        else
        {
            exfat_error("Invalid SEEK_END offset %llu", (unsigned long long)offset);
            return -1;
        }
    }
    else
    {
        exfat_error("Invalid seek whence %d", whence);
    	return lseek(dev->fd, offset, whence);        
    }
#endif
}

ssize_t exfat_read(struct exfat_dev* dev, void* buffer, size_t size)
{
#ifdef USE_UBLIO
	ssize_t result = ublio_pread(dev->ufh, buffer, size, dev->pos);
	if (result >= 0)
		dev->pos += size;
	return result;
#else
	return read(dev->fd, buffer, size);
#endif
}

ssize_t exfat_write(struct exfat_dev* dev, const void* buffer, size_t size)
{
#ifdef USE_UBLIO
	ssize_t result = ublio_pwrite(dev->ufh, buffer, size, dev->pos);
	if (result >= 0)
		dev->pos += size;
	return result;
#else
	return write(dev->fd, buffer, size);
#endif
}

ssize_t exfat_pread(struct exfat_dev* dev, void* buffer, size_t size,
		off_t offset)
{
#ifdef USE_UBLIO
	return ublio_pread(dev->ufh, buffer, size, offset);
#else
	return pread(dev->fd, buffer, size, offset);
#endif
}

ssize_t exfat_pwrite(struct exfat_dev* dev, const void* buffer, size_t size,
		off_t offset)
{
#ifdef USE_UBLIO
	return ublio_pwrite(dev->ufh, buffer, size, offset);
#else
	return pwrite(dev->fd, buffer, size, offset);
#endif
}

ssize_t exfat_generic_pread(const struct exfat* ef, struct exfat_node* node,
		void* buffer, size_t size, off_t offset)
{
	cluster_t cluster;
	char* bufp = buffer;
	off_t lsize, loffset, remainder;

	if (offset >= node->size)
		return 0;
	if (size == 0)
		return 0;

	cluster = exfat_advance_cluster(ef, node, offset / CLUSTER_SIZE(*ef->sb));
	if (CLUSTER_INVALID(*ef->sb, cluster))
	{
		exfat_error("invalid cluster 0x%x while reading", cluster);
		return -EIO;
	}

	loffset = offset % CLUSTER_SIZE(*ef->sb);
	remainder = MIN(size, node->size - offset);
	while (remainder > 0)
	{
		if (CLUSTER_INVALID(*ef->sb, cluster))
		{
			exfat_error("invalid cluster 0x%x while reading", cluster);
			return -EIO;
		}
		lsize = MIN(CLUSTER_SIZE(*ef->sb) - loffset, remainder);
		if (exfat_pread(ef->dev, bufp, lsize,
					exfat_c2o(ef, cluster) + loffset) < 0)
		{
			exfat_error("failed to read cluster %#x", cluster);
			return -EIO;
		}
		bufp += lsize;
		loffset = 0;
		remainder -= lsize;
		cluster = exfat_next_cluster(ef, node, cluster);
	}
	if (!(node->attrib & EXFAT_ATTRIB_DIR) && !ef->ro && !ef->noatime)
		exfat_update_atime(node);
	return MIN(size, node->size - offset) - remainder;
}

ssize_t exfat_generic_pwrite(struct exfat* ef, struct exfat_node* node,
		const void* buffer, size_t size, off_t offset)
{
	int rc;
	cluster_t cluster;
	const char* bufp = buffer;
	off_t lsize, loffset, remainder;

 	if (offset > node->size)
	{
		rc = exfat_truncate(ef, node, offset, true);
		if (rc != 0)
			return rc;
	}
  	if (offset + size > node->size)
	{
		rc = exfat_truncate(ef, node, offset + size, false);
		if (rc != 0)
			return rc;
	}
	if (size == 0)
		return 0;

	cluster = exfat_advance_cluster(ef, node, offset / CLUSTER_SIZE(*ef->sb));
	if (CLUSTER_INVALID(*ef->sb, cluster))
	{
		exfat_error("invalid cluster 0x%x while writing", cluster);
		return -EIO;
	}

	loffset = offset % CLUSTER_SIZE(*ef->sb);
	remainder = size;
	while (remainder > 0)
	{
		if (CLUSTER_INVALID(*ef->sb, cluster))
		{
			exfat_error("invalid cluster 0x%x while writing", cluster);
			return -EIO;
		}
		lsize = MIN(CLUSTER_SIZE(*ef->sb) - loffset, remainder);
		if (exfat_pwrite(ef->dev, bufp, lsize,
				exfat_c2o(ef, cluster) + loffset) < 0)
		{
			exfat_error("failed to write cluster %#x", cluster);
			return -EIO;
		}
		bufp += lsize;
		loffset = 0;
		remainder -= lsize;
		cluster = exfat_next_cluster(ef, node, cluster);
	}
	if (!(node->attrib & EXFAT_ATTRIB_DIR))
		/* directory's mtime should be updated by the caller only when it
		   creates or removes something in this directory */
		exfat_update_mtime(node);
	return size - remainder;
}
