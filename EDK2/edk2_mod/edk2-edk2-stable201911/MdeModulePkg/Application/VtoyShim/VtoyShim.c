/******************************************************************************
 * VtoyShim.c
 *
 * Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PeCoffLib.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/RamDisk.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/Security.h>
#include <Protocol/Security2.h>
#include <IndustryStandard/PeImage.h>
#include <VtoyShim.h>

#define CUR_SBAT_VER    1

STATIC UINT8 gVtoyGrubSha256Hash[32] __attribute__((aligned(32)))  = {
    0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
    0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
    0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
    0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26
};

STATIC BOOLEAN gGrubLaunched = FALSE;
STATIC EFI_GUID gShimLockGUID = SHIM_LOCK_GUID;
STATIC EFI_SECURITY_FILE_AUTHENTICATION_STATE gSysSecFileAuth = NULL;
STATIC EFI_SECURITY2_FILE_AUTHENTICATION gSysSec2FileAuth = NULL;
STATIC BOOLEAN gVtoyByPassSB = FALSE; /* must be FALSE by default for revoke */
STATIC VTOY_SHIM gVtoyShimProtocol;
STATIC EFI_HANDLE gVtoyShimProtHandle;
STATIC SHIM_LOCK gShimLock;

STATIC EFI_EXIT_BOOT_SERVICES gSysExitBootServices = NULL;
STATIC EFI_GET_VARIABLE gSysGetVariable = NULL;

STATIC VOID EFIAPI HookSystemService(VOID);
STATIC VOID EFIAPI UnHookSystemService(VOID);


STATIC VOID EFIAPI VtoyLog(CONST CHAR16 *Format, ...)
{
    VA_LIST         Marker;
    CHAR16          Buffer[512];
    UINTN           BufLen = 0;

    Buffer[0] = 0;
    VA_START(Marker, Format);
    BufLen = UnicodeVSPrint(Buffer, sizeof(Buffer), Format, Marker);
    VA_END(Marker);

    if (gST->ConOut && gST->ConOut->OutputString)
    {
        gST->ConOut->OutputString(gST->ConOut, Buffer);
    }
}


STATIC VOID EFIAPI DumpDevicePath(const EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    CHAR16 *DPStr = NULL;

    DPStr = ConvertDevicePathToText(DevicePath, TRUE, TRUE);
    if (DPStr)
    {
        vLog(L"%s", DPStr);
        gBS->FreePool(DPStr);
    }
    else
    {
        vLog(L"NULL");
    }
}

STATIC VOID EFIAPI ShowSBWarning(const EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    vLog(L"\r\n=======================================================");
    vLog(L"=======================================================\r\n");

    DumpDevicePath(DevicePath);

    vLog(L"\r\n####### Security Boot Violation ##########\r\n");

    vLog(L"=======================================================");
    vLog(L"=======================================================");

    VtoySleep(5);
}


STATIC VOID * EFIAPI FindShimFuncAddr(UINT64 FuncOffset)
{
    EFI_STATUS Status;
    SHIM_IMAGE_LOADER *ImgLoader = NULL;
    EFI_GUID ShimImgLoaderGuid = SHIM_IMAGE_LOADER_GUID;

    Status = gBS->LocateProtocol(&ShimImgLoaderGuid, NULL, (VOID **)&ImgLoader);
    if (EFI_ERROR(Status) || !ImgLoader || !ImgLoader->LoadImage)
    {
        vLog(L"Failed to locate shim image loader protocol %lx %p", Status, ImgLoader);
        return NULL;
    }

    if (NM_SHIM_LOAD_IMAGE_OFFSET > FuncOffset)
    {
        return (UINT8 *)ImgLoader->LoadImage - (NM_SHIM_LOAD_IMAGE_OFFSET - FuncOffset);
    }
    else
    {
        return (UINT8 *)ImgLoader->LoadImage + (FuncOffset - NM_SHIM_LOAD_IMAGE_OFFSET);
    }
}

EFI_STATUS EFIAPI LaunchRealGrub(EFI_HANDLE ImageHandle, CONST CHAR16 *FileName)
{
    EFI_STATUS Status;
    UINTN BufferSize = 0;
    CHAR16 *DevDpStr = NULL;
    CHAR16 *NewDpStr = NULL;
    EFI_HANDLE ChildHandle = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *Li = NULL;
    EFI_DEVICE_PATH_PROTOCOL *DeviceDP = NULL;
    EFI_DEVICE_PATH_PROTOCOL *TargetDp = NULL;

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&Li);
    if (EFI_ERROR(Status))
    {
        vLog(L"Failed to locate loaded image protocol %lx", Status);
        return Status;
    }

    DeviceDP = DevicePathFromHandle(Li->DeviceHandle);
    if (!DeviceDP || !IsDevicePathValid(DeviceDP, 0))
    {
        vLog(L"Failed to get device path of device handle %p", Li->DeviceHandle);
        Status = EFI_NOT_FOUND;
        goto END;
    }

    DevDpStr = ConvertDevicePathToText(DeviceDP, FALSE, TRUE);
    if (!DevDpStr)
    {
        vLog(L"Failed to convert device path to text");
        Status = EFI_OUT_OF_RESOURCES;
        goto END;
    }

    BufferSize = (StrLen(DevDpStr) + 64) * sizeof(CHAR16);
    NewDpStr = (CHAR16 *)AllocatePool(BufferSize);
    if (!NewDpStr)
    {
        vLog(L"Failed to alloc new device path string buffer size:%lu", BufferSize);
        Status = EFI_OUT_OF_RESOURCES;
        goto END;
    }

    UnicodeSPrint(NewDpStr, BufferSize, L"%s/EFI/BOOT/%s", DevDpStr, FileName);

    TargetDp = ConvertTextToDevicePath(NewDpStr);
    if (!TargetDp)
    {
        vLog(L"Failed to convert new text <%s> to device path", NewDpStr);
        Status = EFI_NOT_FOUND;
        goto END;
    }

    Status = gBS->LoadImage(FALSE, ImageHandle, TargetDp, NULL, 0, &ChildHandle);
    if (EFI_ERROR(Status))
    {
        vLog(L"Failed to LoadImage %lx", Status);
        goto END;
    }

    Status = gBS->StartImage(ChildHandle, NULL, NULL);
    if (EFI_ERROR(Status))
    {
        vLog(L"Failed to StartImage %lx", Status);
        gBS->UnloadImage(ChildHandle);
        goto END;
    }

END:

    CheckFreePool(DevDpStr);
    CheckFreePool(NewDpStr);
    CheckFreePool(TargetDp);

    return Status;
}



STATIC EFI_STATUS EFIAPI ReadAuthFile
(
    const EFI_DEVICE_PATH_PROTOCOL *DevicePathConst,
    VOID **Buffer,
    UINT32 *Size
)
{
    EFI_STATUS Status;
    UINTN TmpSize = 0;
    CHAR16 *DpStr = NULL;
	EFI_HANDLE Handle = NULL;
    EFI_DEVICE_PATH *DevPath = NULL;
    EFI_DEVICE_PATH *TmpPath = NULL;
	EFI_FILE_IO_INTERFACE *FileIO = NULL;
	EFI_FILE *File = NULL;
	EFI_FILE *Root = NULL;
    UINT8 *FileData = NULL;
    EFI_FILE_INFO *FInfo = NULL;
    UINT8 Buf[1024];

	DevPath	= TmpPath = DuplicateDevicePath(DevicePathConst);
    if (!DevPath)
    {
        Status = EFI_OUT_OF_RESOURCES;
        goto END;
    }

	Status = gBS->LocateDevicePath(&gEfiSimpleFileSystemProtocolGuid, &DevPath, &Handle);
    if (EFI_ERROR(Status))
    {
        vLog(L"Failed to locate simple file protocol %lx", Status);
        goto END;
    }

    DpStr = ConvertDevicePathToText(DevPath, FALSE, TRUE);
    if (!DpStr)
    {
        Status = EFI_OUT_OF_RESOURCES;
        goto END;
    }

    Status = gBS->HandleProtocol(Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileIO);
    if (EFI_ERROR(Status))
    {
        vLog(L"Failed to handle simple file protocol %lx", Status);
        goto END;
    }

    Status = FileIO->OpenVolume(Handle, &Root);
	if (EFI_ERROR(Status))
    {
		vLog(L"Failed to open drive volume (%lx)\n", Status);
		goto END;
	}

    Status = Root->Open(Root, &File, DpStr, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
		vLog(L"Failed to open file (%s) (%lx)\n", DpStr, Status);
		goto END;
	}

    FInfo = (EFI_FILE_INFO *)Buf;
    TmpSize = sizeof(Buf);
    ZeroMem(FInfo, sizeof(EFI_FILE_INFO));

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &TmpSize, FInfo);
    if (EFI_ERROR(Status) || FInfo->FileSize == 0 || FInfo->FileSize >= 0xFFFFFFFFUL)
    {
		vLog(L"Failed to open file (%s) (%lx) Size(%ld)\n", DpStr, Status, (UINTN)FInfo->FileSize);
		goto END;
	}

    FileData = AllocatePool(FInfo->FileSize);
    if (!FileData)
    {
        Status = EFI_OUT_OF_RESOURCES;
        goto END;
    }

    TmpSize = FInfo->FileSize;
    Status = File->Read(File, &TmpSize, FileData);
    if (EFI_ERROR(Status) || TmpSize != (UINTN)FInfo->FileSize)
    {
		vLog(L"Failed to read file (%lx) Read:%ld Size:%ld\n", Status, TmpSize, (UINTN)FInfo->FileSize);
		goto END;
	}


END:

    if (File)
    {
        File->Close(File);
    }

    if (Root)
    {
        Root->Close(Root);
    }

    CheckFreePool(TmpPath);
    CheckFreePool(DpStr);

    if (EFI_ERROR(Status))
    {
        CheckFreePool(FileData);
    }
    else
    {
        *Buffer = FileData;
        *Size = (UINT32)FInfo->FileSize;
    }

    return Status;
}

STATIC EFI_STATUS EFIAPI CheckVtoyGrub
(
    VOID *FileBuffer,
	UINTN FileSize
)
{
    UINTN Index = 0;
    EFI_STATUS Status = EFI_SECURITY_VIOLATION;
    PE_COFF_LOADER_IMAGE_CONTEXT Ctx;
    UINT8 Sha256Hash[64];
    UINT8 Sha1Hash[64];

    ZeroMem(&Ctx, sizeof(Ctx));
    ZeroMem(Sha1Hash, sizeof(Sha1Hash));
    ZeroMem(Sha256Hash, sizeof(Sha256Hash));

    Status = gShimLock.Context(FileBuffer, FileSize, &Ctx);
    if (EFI_ERROR(Status))
    {
        vErr(L"Cannot get shim context %lx", Status);
        goto END;
    }

    Status = gShimLock.Hash(FileBuffer, FileSize, &Ctx, Sha256Hash, Sha1Hash);
    if (EFI_ERROR(Status))
    {
        vErr(L"Cannot get shim hash %lx", Status);
        goto END;
    }

    if (CompareMem(Sha256Hash, gVtoyGrubSha256Hash, 32) != 0)
    {
        vErr(L"Ventoy hash check failed.");
        goto END;
    }

    Status = EFI_SUCCESS;

END:

    if (EFI_ERROR(Status))
    {
        vLog(L"\r\n###### Press Enter to reboot... ######");
        if (gST->ConIn)
        {
            gST->ConIn->Reset(gST->ConIn, FALSE);
            gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        }
        gRT->ResetSystem(EfiResetWarm, EFI_SECURITY_VIOLATION, 0, NULL);
    }

    return Status;
}


STATIC EFI_STATUS EFIAPI SecurityPolicyAuth
(
	const EFI_SECURITY_ARCH_PROTOCOL *This,
	UINT32 AuthenticationStatus,
	const EFI_DEVICE_PATH_PROTOCOL *DevicePathConst
)
{
    EFI_STATUS Status;
    UINT32 Size = 0;
    VOID *Buffer = NULL;

    /* Just return OK if the user choose to bypass SB */
    if (gVtoyByPassSB)
    {
        return EFI_SUCCESS;
    }

    if (!gGrubLaunched)
    {
        Status = ReadAuthFile(DevicePathConst, &Buffer, &Size);
        if (EFI_ERROR(Status))
        {
            return EFI_SECURITY_VIOLATION;
        }

        Status = CheckVtoyGrub(Buffer, Size);
        FreePool(Buffer);
        return Status;
    }

    /*
     * Step 1:
     * Use original UEFI firmware auth API.
     * If it's OK, it may be signed with Microsoft UEFI CA. (e.g. bootmgr/shim/...)
     */
    if (gSysSecFileAuth)
    {
        Status = gSysSecFileAuth(This, AuthenticationStatus, DevicePathConst);
        if (!EFI_ERROR(Status))
        {
            return EFI_SUCCESS;
        }
    }


    /*
     * Step 2:
     * Use shim verify API.
     * If it's OK, it may be signed with a MOK key. (e.g. Ventoy EFI files)
     */
    if (gShimLock.Verify)
    {
        Status = ReadAuthFile(DevicePathConst, &Buffer, &Size);
        if (!EFI_ERROR(Status))
        {
            Status = gShimLock.Verify(Buffer, Size);
            FreePool(Buffer);
            if (!EFI_ERROR(Status))
            {
                return EFI_SUCCESS;
            }
        }
    }

    ShowSBWarning(DevicePathConst);

    return EFI_SECURITY_VIOLATION;
}

STATIC EFI_STATUS EFIAPI Security2PolicyAuth
(
	const EFI_SECURITY2_ARCH_PROTOCOL *This,
	const EFI_DEVICE_PATH_PROTOCOL *DevicePath,
	VOID *FileBuffer,
	UINTN FileSize,
	BOOLEAN	BootPolicy
)
{
    EFI_STATUS Status;

    /* Just return OK if the user choose to bypass SB */
    if (gVtoyByPassSB)
    {
        return EFI_SUCCESS;
    }

    if (!gGrubLaunched)
    {
        return CheckVtoyGrub(FileBuffer, FileSize);
    }

    /*
     * Step 1:
     * Use original UEFI firmware auth API.
     * If it's OK, it may be signed with Microsoft UEFI CA. (e.g. bootmgr/shim/...)
     */
    if (gSysSec2FileAuth)
    {
        Status = gSysSec2FileAuth(This, DevicePath, FileBuffer, FileSize, BootPolicy);
        if (!EFI_ERROR(Status))
        {
            return EFI_SUCCESS;
        }
    }


    /*
     * Step 2:
     * Use shim verify API.
     * If it's OK, it may be signed with a MOK key. (e.g. Ventoy EFI files)
     */
    if (gShimLock.Verify)
    {
        if (FileBuffer && FileSize > 0 && FileSize < 0xFFFFFFFFUL)
        {
            Status = gShimLock.Verify(FileBuffer, (UINT32)FileSize);
            if (!EFI_ERROR(Status))
            {
                return EFI_SUCCESS;
            }
        }
    }

    ShowSBWarning(DevicePath);

    return EFI_SECURITY_VIOLATION;
}


STATIC EFI_STATUS EFIAPI HookSecurityPolicy(VOID)
{
	EFI_STATUS Status;
	EFI_STATUS Status2;
	EFI_SECURITY_ARCH_PROTOCOL *Security = NULL;
	EFI_SECURITY2_ARCH_PROTOCOL *Security2 = NULL;

	Status = gBS->LocateProtocol(&gEfiSecurityArchProtocolGuid, NULL, (VOID **)&Security);
	Status2 = gBS->LocateProtocol(&gEfiSecurity2ArchProtocolGuid, NULL, (VOID **)&Security2);
    if (EFI_ERROR(Status) && EFI_ERROR(Status2))
    {
        vLog(L"Failed to locate security or security2 protocol. %lx %lx %p %p",
             Status, Status2, Security, Security2);
        return EFI_NOT_FOUND;
    }

    if (Security2)
    {
        gSysSec2FileAuth = Security2->FileAuthentication;
        Security2->FileAuthentication = Security2PolicyAuth;
    }

    if (Security)
    {
        gSysSecFileAuth = Security->FileAuthenticationState;
        Security->FileAuthenticationState = SecurityPolicyAuth;
    }

	return EFI_SUCCESS;
}

STATIC VOID EFIAPI UnHookSecurityPolicy(VOID)
{
	EFI_STATUS Status;
	EFI_STATUS Status2;
	EFI_SECURITY_ARCH_PROTOCOL *Security = NULL;
	EFI_SECURITY2_ARCH_PROTOCOL *Security2 = NULL;

    if (!gSysSec2FileAuth && !gSysSecFileAuth)
    {
        return;
    }

	Status = gBS->LocateProtocol(&gEfiSecurityArchProtocolGuid, NULL, (VOID **)&Security);
	Status2 = gBS->LocateProtocol(&gEfiSecurity2ArchProtocolGuid, NULL, (VOID **)&Security2);
    if (EFI_ERROR(Status) && EFI_ERROR(Status2))
    {
        vLog(L"Failed to locate security or security2 protocol. %lx %lx %p %p",
             Status, Status2, Security, Security2);
        return;
    }

    if (Security2 && gSysSec2FileAuth && Security2->FileAuthentication == Security2PolicyAuth)
    {
        Security2->FileAuthentication = gSysSec2FileAuth;
        gSysSec2FileAuth = NULL;
    }

    if (Security && gSysSecFileAuth && Security->FileAuthenticationState == SecurityPolicyAuth)
    {
        Security->FileAuthenticationState = gSysSecFileAuth;
        gSysSecFileAuth = NULL;
    }
}

STATIC VOID EFIAPI VtoyByPassSB(VOID)
{
    gVtoyByPassSB = TRUE;
}

STATIC VOID EFIAPI VtoyCheckSB(VOID)
{
    gVtoyByPassSB = FALSE;
}

STATIC VOID EFIAPI VtoyLaunched(VOID)
{
    gGrubLaunched = TRUE;
}

STATIC VOID EFIAPI UnInstallVtoyShimProtocol(VOID)
{
    EFI_GUID Guid = VTOY_SHIM_POLICY_GUID;

    if (gVtoyShimProtHandle)
    {
        gBS->UninstallProtocolInterface(gVtoyShimProtHandle, &Guid, &gVtoyShimProtocol);
        gVtoyShimProtHandle = NULL;
    }
}

STATIC EFI_STATUS EFIAPI InstallVtoyShimProtocol(VOID)
{
	EFI_STATUS Status;
    EFI_GUID Guid = VTOY_SHIM_POLICY_GUID;
    VTOY_SHIM *Prot = NULL;

    gVtoyShimProtocol.ByPassSB = VtoyByPassSB;
    gVtoyShimProtocol.CheckSB = VtoyCheckSB;
    gVtoyShimProtocol.Launched = VtoyLaunched;

    Status = gBS->LocateProtocol(&Guid, NULL, (VOID**)&Prot);
    if (!EFI_ERROR(Status))
    {
        vLog(L"Ventoy shim already loaded, cannot be nested.");
        return EFI_ALREADY_STARTED;
    }

    Status = gBS->InstallProtocolInterface(&gVtoyShimProtHandle, &Guid,
                    EFI_NATIVE_INTERFACE, &gVtoyShimProtocol);
    if (EFI_ERROR(Status))
    {
        vLog(L"Failed to install protocol %lx", Status);
    }

    return Status;
}

STATIC BOOLEAN EFIAPI IsSecureBootEnabled(VOID)
{
    UINT8 SecureBoot = 0;
	UINTN DataSize;
	EFI_STATUS Status;

	DataSize = sizeof(SecureBoot);
	Status = gST->RuntimeServices->GetVariable(L"SecureBoot", &gEfiGlobalVariableGuid, NULL,
				     &DataSize, &SecureBoot);
	if (EFI_ERROR(Status))
    {
        return FALSE;
    }

	return SecureBoot ? TRUE : FALSE;
}

STATIC BOOLEAN EFIAPI IsSetupMode(VOID)
{
    UINT8 SetupMode = 0;
	UINTN DataSize;
	EFI_STATUS Status;

	DataSize = sizeof(SetupMode);
	Status = gST->RuntimeServices->GetVariable(L"SetupMode", &gEfiGlobalVariableGuid, NULL,
				     &DataSize, &SetupMode);
	if (EFI_ERROR(Status))
    {
        return FALSE;
    }

	return SetupMode ? TRUE : FALSE;
}

STATIC EFI_STATUS EFIAPI ShimEfiMain
(
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS Status;
    SHIM_LOCK *ShimLock = NULL;
    shim_void_func_pf Func1 = NULL;
    shim_void_func_pf Func2 = NULL;

    /* We must be launched by shim */
    Status = gBS->LocateProtocol(&gShimLockGUID, NULL, (VOID**)&ShimLock);
    if (EFI_ERROR(Status) || !ShimLock)
    {
        vErr(L"Failed to locate SHIM LOCK Protocol %lx", Status);
        return Status;
    }

    /* Backup shim Lock because we will remove it later  */
    gShimLock.Verify = ShimLock->Verify;
    gShimLock.Hash = ShimLock->Hash;
    gShimLock.Context = ShimLock->Context;

    Status = InstallVtoyShimProtocol();
    if (EFI_ERROR(Status))
    {
        vErr(L"Failed to install ventoy shim protocol");
        return Status;
    }


    /*
     * IMPORTANT: All recent shim implementations hook the UEFI Boot Services
     * (e.g. LoadImage, StartImage) to enforce signature verification.
     *
     * We must restore the original system service pointers here. If we fail to do this,
     * we will be unable to launch Ventoy-signed EFI binaries or any other unsigned
     * EFI applications later, even when the user has explicitly opted to disable
     * all Secure Boot validation checks.
     *
     * To the best of my knowledge, there is no official way to remove these hooks.
     * This is a tricky hack that relies on shim's internal implementation details.
     * It may break in future versions of shim, and a better approach may exist.
     *
     */
    Func1 = FindShimFuncAddr(NM_UNHOOK_SYSTEM_SERVICES_OFFSET);
    Func2 = FindShimFuncAddr(NM_UNINSTALL_SHIM_PROTOCOLS_OFFSET);
    if (!Func1 || !Func2)
    {
        vErr(L"Can not find shim func %p %p", Func1, Func2);
        Status = EFI_NOT_FOUND;
        goto END;
    }

    Func1(); /* call shim unhook_system_services() */
    Func2(); /* call shim uninstall_shim_protocols() */

    HookSystemService();

    /* Hook the system security policy */
    Status = HookSecurityPolicy();
    if (EFI_ERROR(Status))
    {
        vErr(L"Failed to hook system security policy");
        goto END;
    }


    /* Finally launch Ventoy grub */
    Status = LaunchRealGrub(ImageHandle, REAL_GRUB_FILE);
    if (EFI_ERROR(Status))
    {
        vErr(L"Failed to finally launch real grub %s", REAL_GRUB_FILE);
        goto END;
    }

END:

    /* UnHook system security policy */
    UnHookSecurityPolicy();

    UnInstallVtoyShimProtocol();

    UnHookSystemService();

    return Status;
}

EFI_STATUS EFIAPI VtoyGetVariable
(
    IN     CHAR16                      *VariableName,
    IN     EFI_GUID                    *VendorGuid,
    OUT    UINT32                      *Attributes,    OPTIONAL
    IN OUT UINTN                       *DataSize,
    OUT    VOID                        *Data           OPTIONAL
)
{
    BOOLEAN bChk = FALSE;
    EFI_STATUS Status;

    if (gVtoyByPassSB && VariableName && VendorGuid && DataSize && Data && (*DataSize) > 0)
    {
        bChk = TRUE;
    }

    Status = gSysGetVariable(VariableName, VendorGuid, Attributes, DataSize, Data);
    if (bChk && (!EFI_ERROR(Status)))
    {
        if (CompareMem(&gShimLockGUID, VendorGuid, 16) == 0 &&
            StrCmp(VariableName, L"MokSBState") == 0)
        {
            *(UINT8 *)Data = 1;
        }
    }

    return Status;
}

STATIC VOID EFIAPI UnHookSystemService(VOID)
{
    if (gSysExitBootServices)
    {
        gBS->ExitBootServices = gSysExitBootServices;
        gSysExitBootServices = NULL;
    }

    if (gSysGetVariable)
    {
        gST->RuntimeServices->GetVariable = gSysGetVariable;
        gSysGetVariable = NULL;
    }
}


STATIC EFI_STATUS EFIAPI VtoyExitBootServices
(
    IN  EFI_HANDLE  ImageHandle,
    IN  UINTN       MapKey
)
{
    EFI_EXIT_BOOT_SERVICES SysExitBS;

    /* UnHookSystemService will set gSysExitBootServices NULL */
    SysExitBS = gSysExitBootServices;

    UnHookSecurityPolicy();
    UnInstallVtoyShimProtocol();
    UnHookSystemService();

    return SysExitBS(ImageHandle, MapKey);
}

STATIC VOID EFIAPI HookSystemService(VOID)
{
    gSysExitBootServices = gBS->ExitBootServices;
    gBS->ExitBootServices = VtoyExitBootServices;

    gSysGetVariable = gST->RuntimeServices->GetVariable;
    gST->RuntimeServices->GetVariable = VtoyGetVariable;
}

EFI_STATUS EFIAPI VtoyShimEfiMain
(
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    BOOLEAN IsSetup = FALSE;
    BOOLEAN IsSecureBoot = FALSE;
    EFI_STATUS Status;

    IsSetup = IsSetupMode();
    IsSecureBoot = IsSecureBootEnabled();

    if (!IsSecureBoot || IsSetup)
    {
        /* If secure boot is not enabled or in SetupMode, nothing needed, just launch Ventoy grub */
        Status = LaunchRealGrub(ImageHandle, REAL_GRUB_FILE);
        if (EFI_ERROR(Status))
        {
            vErr(L"Failed to launch %s", REAL_GRUB_FILE);
        }
    }
    else
    {
        Status = ShimEfiMain(ImageHandle, SystemTable);
    }

    return Status;
}

