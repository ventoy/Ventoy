#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/video.h>
#include <grub/font.h>
#include <grub/gfxterm.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/ventoy.h>
#include "ventoy_sysinfo.h"

#ifdef GRUB_MACHINE_EFI
#include <grub/efi/efi.h>
#endif

/* CPU Detection */
const char* ventoy_detect_cpu(void)
{
#if defined(__x86_64__) || defined(__i386__)
    return "x86/x64"; /* Detailed CPUID requires more complex inline asm compatible with GRUB build env */
#elif defined(__aarch64__)
    return "ARM64";
#elif defined(__mips__)
    return "MIPS";
#else
    return "Unknown Arch";
#endif
}

/* RAM Detection */
static int ventoy_ram_iter(grub_uint64_t addr, grub_uint64_t size, grub_memory_type_t type, void *data)
{
    grub_uint64_t *total = (grub_uint64_t *)data;
    /* GRUB_MEMORY_AVAILABLE = 1, we might want to count usable RAM. 
       Note: This is a simplification. GRUB often sees less than total physical RAM. */
    if (type == GRUB_MEMORY_AVAILABLE || type == GRUB_MEMORY_ACPI_RECLAIM) 
    {
        *total += size;
    }
    return 0;
}

char* ventoy_detect_ram(void)
{
    grub_uint64_t total_bytes = 0;
    static char buf[32];
    
    grub_machine_mmap_iterate(ventoy_ram_iter, &total_bytes);
    
    /* Convert to likely physical amount (rounding up to nearest GB/512MB often helps if reserved regions exist) */
    /* For now, just show raw MB/GB detected by GRUB */
    if (total_bytes > 1024 * 1024 * 1024)
        grub_snprintf(buf, sizeof(buf), "%d GB", (int)((total_bytes + 512*1024*1024) / (1024 * 1024 * 1024)));
    else
        grub_snprintf(buf, sizeof(buf), "%d MB", (int)(total_bytes / (1024 * 1024)));
        
    return buf;
}

/* Boot Mode Detection */
const char* ventoy_detect_boot_mode(void)
{
#ifdef GRUB_MACHINE_EFI
    return "UEFI";
#elif defined(GRUB_MACHINE_PCBIOS)
    return "Legacy BIOS";
#else
    return "Unknown";
#endif
}

/* Architecture Detection */
const char* ventoy_detect_arch(void)
{
#if defined(__x86_64__) || defined(__aarch64__)
    return "64-bit";
#else
    return "32-bit";
#endif
}

/* Storage Detection */
static int ventoy_disk_iter(const char *name, void *data)
{
    /* This iterates all devices. We want counting. */
    /* data is int array: [ssd_count, hdd_count, other_count] - simplified for now */
    int *counts = (int*)data;
    grub_device_t dev;
    
    dev = grub_device_open(name);
    if (!dev) return 0;
    
    if (dev->disk)
    {
        /* Simple heuristic: checking rotation usually not available easily in GRUB without ATA commands.
           We will just count disks. */
        counts[0]++; /* Just count total disks for now */
    }
    
    grub_device_close(dev);
    return 0;
}

char* ventoy_detect_storage(void)
{
    static char buf[64];
    int counts[3] = {0}; /* 0: Total */
    
    grub_device_iterate(ventoy_disk_iter, counts);
    
    grub_snprintf(buf, sizeof(buf), "%d Disks Found", counts[0]);
    return buf;
}

/* Extern declaration since we can't find the header easily in this env */
extern void grub_font_draw_string (const char *str, grub_font_t font, grub_video_color_t color, int left_x, int baseline_y);

/* Display System Info */
void ventoy_display_sysinfo(void)
{
    const char *cpu = ventoy_detect_cpu();
    const char *ram = ventoy_detect_ram();
    const char *boot = ventoy_detect_boot_mode();
    const char *arch = ventoy_detect_arch();
    const char *storage = ventoy_detect_storage();
    
    struct grub_video_viewport original_vp;
    grub_video_get_viewport (&original_vp);

    /* Text Color */
    grub_video_color_t color_fg = grub_video_map_rgb(255, 255, 255); /* White */
    /* grub_video_color_t color_bg = grub_video_map_rgb(128, 128, 128); */ /* Gray - Unused for text drawing */

    /* Calculate position - Top Right */
    /* Assuming default font width ~8px, height ~16px */
    unsigned int x_margin = 250; /* Space for text */
    unsigned int start_x = original_vp.width - x_margin;
    unsigned int start_y = 10;
    unsigned int line_height = 20;
    
    /* Check if graphics is active */
    grub_video_adapter_t adapter = grub_video_get_driver_id() != GRUB_VIDEO_DRIVER_NONE ? grub_video_get_active_adapter() : NULL;
    
    if (adapter)
    {
        grub_font_t font = grub_font_get ("Unknown Regular 16"); /* Try to get default font */
        if (!font) font = grub_font_get_current();
        
        if (font)
        {
             char tmp[128];
             
             grub_snprintf(tmp, sizeof(tmp), "CPU: %s", cpu);
             grub_font_draw_string(tmp, font, color_fg, start_x, start_y);
             
             start_y += line_height;
             grub_snprintf(tmp, sizeof(tmp), "RAM: %s", ram);
             grub_font_draw_string(tmp, font, color_fg, start_x, start_y);
             
             start_y += line_height;
             grub_snprintf(tmp, sizeof(tmp), "Mode: %s", boot);
             grub_font_draw_string(tmp, font, color_fg, start_x, start_y);
             
             start_y += line_height;
             grub_snprintf(tmp, sizeof(tmp), "Arch: %s", arch);
             grub_font_draw_string(tmp, font, color_fg, start_x, start_y);
             
             start_y += line_height;
             grub_snprintf(tmp, sizeof(tmp), "Sto: %s", storage);
             grub_font_draw_string(tmp, font, color_fg, start_x, start_y);
        }
    }
}
