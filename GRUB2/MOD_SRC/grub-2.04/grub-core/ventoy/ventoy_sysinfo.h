#ifndef VENTOY_SYSINFO_H
#define VENTOY_SYSINFO_H

#include <grub/types.h>
#include <grub/err.h>

/* Hardware detection functions */
const char* ventoy_detect_cpu(void);
char* ventoy_detect_ram(void);
const char* ventoy_detect_boot_mode(void);
const char* ventoy_detect_arch(void);
char* ventoy_detect_storage(void);

/* Display function */
void ventoy_display_sysinfo(void);

#endif /* VENTOY_SYSINFO_H */
