#ifndef _WIMBOOT_EFI_PROCESSOR_BIND_H
#define _WIMBOOT_EFI_PROCESSOR_BIND_H

/*
 * EFI header files rely on having the CPU architecture directory
 * present in the search path in order to pick up ProcessorBind.h.  We
 * use this header file as a quick indirection layer.
 *
 */

#if __i386__
#include <efi/Ia32/ProcessorBind.h>
#endif

#if __x86_64__
#include <efi/X64/ProcessorBind.h>
#endif

#endif /* _WIMBOOT_EFI_PROCESSOR_BIND_H */
