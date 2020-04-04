
#ifndef __VENTOY_INT13_H__
#define __VENTOY_INT13_H__

#undef for_each_sandev
#define for_each_sandev( sandev ) sandev = g_sandev; if (sandev)

int ventoy_vdisk_read(struct san_device*sandev, uint64_t lba, unsigned int count, unsigned long buffer);

static inline int ventoy_sandev_write ( struct san_device *sandev, uint64_t lba, unsigned int count, unsigned long buffer )
{
    (void)sandev;
    (void)lba;
    (void)count;
    (void)buffer;
    DBGC(sandev, "ventoy_sandev_write\n");
    return 0;
}

static inline int ventoy_sandev_reset (void *sandev)
{
    (void)sandev;
    DBGC(sandev, "ventoy_sandev_reset\n");
    return 0;
}

#define sandev_reset  ventoy_sandev_reset
#define sandev_read   ventoy_vdisk_read
#define sandev_write  ventoy_sandev_write

#undef  ECANCELED
#define ECANCELED 0x0b
#undef  ENODEV
#define ENODEV 0x2c
#undef  ENOTSUP
#define ENOTSUP 0x3c
#undef  ENOMEM
#define ENOMEM 0x31
#undef  EIO
#define EIO 0x1d
#undef  ENOEXEC
#define ENOEXEC 0x2e
#undef  ENOSPC
#define ENOSPC 0x34


#endif /* __VENTOY_INT13_H__ */

