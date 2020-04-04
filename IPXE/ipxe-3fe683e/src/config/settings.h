#ifndef CONFIG_SETTINGS_H
#define CONFIG_SETTINGS_H

/** @file
 *
 * Configuration settings sources
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

//#define	PCI_SETTINGS	/* PCI device settings */
//#define	CPUID_SETTINGS	/* CPUID settings */
//#define	MEMMAP_SETTINGS	/* Memory map settings */
//#define	VMWARE_SETTINGS	/* VMware GuestInfo settings */
//#define	VRAM_SETTINGS	/* Video RAM dump settings */
//#define	ACPI_SETTINGS	/* ACPI settings */

#include <config/named.h>
#include NAMED_CONFIG(settings.h)
#include <config/local/settings.h>
#include LOCAL_NAMED_CONFIG(settings.h)

#endif /* CONFIG_SETTINGS_H */
