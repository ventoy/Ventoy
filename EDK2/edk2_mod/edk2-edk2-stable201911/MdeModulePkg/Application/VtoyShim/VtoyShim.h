/******************************************************************************
 * VtoyShim.h
 *
 * Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#ifndef __VTOYSHIM_H__
#define __VTOYSHIM_H__

#if defined (MDE_CPU_IA32)
#define REAL_GRUB_FILE    L"grubia32_real.efi"
#elif defined (MDE_CPU_X64)
#define REAL_GRUB_FILE    L"grubx64_real.efi"
#elif defined (MDE_CPU_AARCH64)
#define REAL_GRUB_FILE    L"grubaa64_real.efi"
#else
#error "Not supported now"
#endif


/* The following definations are copied from shim source code */

#define SHIM_LOCK_GUID  {0x605dab50, 0xe046, 0x4300, {0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23 } };

typedef
EFI_STATUS
(*EFI_SHIM_LOCK_VERIFY) (
	IN VOID *buffer,
	IN UINT32 size
	);

typedef
EFI_STATUS
(*EFI_SHIM_LOCK_HASH) (
	IN char *data,
	IN int datasize,
	PE_COFF_LOADER_IMAGE_CONTEXT *context,
	UINT8 *sha256hash,
	UINT8 *sha1hash
	);

typedef
EFI_STATUS
(*EFI_SHIM_LOCK_CONTEXT) (
	IN VOID *data,
	IN unsigned int datasize,
	PE_COFF_LOADER_IMAGE_CONTEXT *context
	);

typedef struct _SHIM_LOCK {
	EFI_SHIM_LOCK_VERIFY Verify;
	EFI_SHIM_LOCK_HASH Hash;
	EFI_SHIM_LOCK_CONTEXT Context;
} SHIM_LOCK;



#define SHIM_IMAGE_LOADER_GUID {0x1f492041, 0xfadb, 0x4e59, {0x9e, 0x57, 0x7c, 0xaf, 0xe7, 0x3a, 0x55, 0xab } }

typedef struct _SHIM_IMAGE_LOADER {
	EFI_IMAGE_LOAD LoadImage;
	EFI_IMAGE_START StartImage;
	EFI_EXIT Exit;
	EFI_IMAGE_UNLOAD UnloadImage;
} SHIM_IMAGE_LOADER;

typedef VOID (*shim_void_func_pf)(VOID);


/*
 * The two offset here are extract from the shim file which used in Ventoy.
 * nm BOOTX64.EFI | grep shim_load_image
 * nm BOOTX64.EFI | grep unhook_system_services
 * nm BOOTX64.EFI | grep uninstall_shim_protocols
 *
 * It means that they must be updated every time Ventoy update the shim file.
 *
 */
#define NM_SHIM_LOAD_IMAGE_OFFSET             0x2dc12
#define NM_UNHOOK_SYSTEM_SERVICES_OFFSET      0x2e278
#define NM_UNINSTALL_SHIM_PROTOCOLS_OFFSET    0x26264




#define VtoySleep(sec)      gBS->Stall(1000000 * (sec))
#define vLog(fmt, ...)      VtoyLog(fmt "\r\n", ##__VA_ARGS__)
#define vErr(fmt, ...)      VtoyLog(fmt "\r\n", ##__VA_ARGS__); VtoySleep(5)
#define vDbg(fmt, ...)      VtoyLog(fmt "\r\n", ##__VA_ARGS__); VtoySleep(2)

#define CheckFreePool(p) \
do { \
    if (p) { \
        FreePool(p); \
        (p) = NULL; \
    }\
} while (0)


#define VTOY_SHIM_POLICY_GUID    {0x90a29d14, 0x3968, 0x48fe, { 0x85, 0x81, 0x6b, 0x7f, 0x7d, 0xc4, 0x70, 0x55 }};

typedef VOID (EFIAPI *VTOY_BYPASS_SB)(VOID);
typedef VOID (EFIAPI *VTOY_CHECK_SB)(VOID);
typedef VOID (EFIAPI *VTOY_LAUNCHED)(VOID);
typedef struct _VTOY_SHIM{
	VTOY_BYPASS_SB ByPassSB;
	VTOY_BYPASS_SB CheckSB;
	VTOY_LAUNCHED Launched;
} VTOY_SHIM;

CONST UINT8 * ventoy_get_der_data(UINT32 *Len);

#endif

