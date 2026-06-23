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

STATIC EFI_GUID gVtoySbatGUID = { 0xf755068a, 0xe04f, 0x452b, { 0x9d, 0x6d, 0x7c, 0x55, 0x96, 0xb3, 0xc0, 0x7d }};
STATIC EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *gDpToText = NULL;
STATIC EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *gTextToDp = NULL;
STATIC EFI_SECURITY_FILE_AUTHENTICATION_STATE gSysSecFileAuth = NULL;
STATIC EFI_SECURITY2_FILE_AUTHENTICATION gSysSec2FileAuth = NULL;
STATIC BOOLEAN gVtoyByPassSB = FALSE; /* must be FALSE by default for revoke */
STATIC VTOY_SHIM gVtoyShimProtocol;
STATIC EFI_HANDLE gVtoyShimProtHandle;
STATIC SHIM_LOCK *gShimLock = NULL;

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

    DPStr = gDpToText->ConvertDevicePathToText(DevicePath, TRUE, TRUE);
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

STATIC VOID EFIAPI ShowSBWarning(BOOLEAN Reboot, const EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
    UINTN Index = 0;

    vLog(L"\r\n=======================================================");
    vLog(L"=======================================================\r\n");

    DumpDevicePath(DevicePath);

    vLog(L"\r\n####### Security Boot Violation ##########\r\n");

    vLog(L"=======================================================");
    vLog(L"=======================================================");

    if (Reboot)
    {
        vLog(L"\r\n###### Press Enter to reboot... ######");
        gST->ConIn->Reset(gST->ConIn, FALSE);
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gRT->ResetSystem(EfiResetWarm, EFI_SECURITY_VIOLATION, 0, NULL);
    }
    else
    {
        VtoySleep(5);
    }
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

    DevDpStr = gDpToText->ConvertDevicePathToText(DeviceDP, FALSE, TRUE);
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

    TargetDp = gTextToDp->ConvertTextToDevicePath(NewDpStr);
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

    CheckBSFreePool(DevDpStr);
    CheckFreePool(NewDpStr);
    CheckBSFreePool(TargetDp);

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

    DpStr = gDpToText->ConvertDevicePathToText(DevPath, FALSE, TRUE);
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
    CheckBSFreePool(DpStr);

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


STATIC BOOLEAN VtoyCheckRevoke(VOID *Buffer, UINTN Size)
{
    UINT32 uiVer = 0;
    EFI_IMAGE_DOS_HEADER *DosHead = (EFI_IMAGE_DOS_HEADER *)Buffer;

    if (Size > sizeof(EFI_IMAGE_DOS_HEADER))
    {
        if (CompareMem(DosHead->e_res2, &gVtoySbatGUID, 16) == 0)
        {
            CopyMem(&uiVer, DosHead->e_res2 + 8, 4);
            if (uiVer < CUR_SBAT_VER)
            {
                vLog(L"Ventoy EFI file revoke (%u < %u)", uiVer, CUR_SBAT_VER);
                return FALSE;
            }
        }
    }

    return TRUE;
}

STATIC EFI_STATUS EFIAPI SecurityPolicyAuth
(
	const EFI_SECURITY_ARCH_PROTOCOL *This,
	UINT32 AuthenticationStatus,
	const EFI_DEVICE_PATH_PROTOCOL *DevicePathConst
)
{
    EFI_STATUS Status;
    BOOLEAN bRevokeChkOK = TRUE;
    UINT32 Size = 0;
    VOID *Buffer = NULL;

    /* Just return OK if the user choose to bypass SB */
    if (gVtoyByPassSB)
    {
        return EFI_SUCCESS;
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
    if (gShimLock && gShimLock->Verify)
    {
        Status = ReadAuthFile(DevicePathConst, &Buffer, &Size);
        if (!EFI_ERROR(Status))
        {
            Status = gShimLock->Verify(Buffer, Size);
            if (!EFI_ERROR(Status))
            {
                bRevokeChkOK = VtoyCheckRevoke(Buffer, Size);
                if (bRevokeChkOK)
                {
                    FreePool(Buffer);
                    return EFI_SUCCESS;
                }
            }
            FreePool(Buffer);
        }
    }

    ShowSBWarning(!bRevokeChkOK, DevicePathConst);

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
    BOOLEAN bRevokeChkOK = TRUE;

    /* Just return OK if the user choose to bypass SB */
    if (gVtoyByPassSB)
    {
        return EFI_SUCCESS;
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
    if (gShimLock && gShimLock->Verify)
    {
        if (FileBuffer && FileSize > 0 && FileSize < 0xFFFFFFFFUL)
        {
            Status = gShimLock->Verify(FileBuffer, (UINT32)FileSize);
            if (!EFI_ERROR(Status))
            {
                bRevokeChkOK = VtoyCheckRevoke(FileBuffer, FileSize);
                if (bRevokeChkOK)
                {
                    return EFI_SUCCESS;
                }
            }
        }
    }

    ShowSBWarning(!bRevokeChkOK, DevicePath);

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

    if (Security2 && gSysSec2FileAuth)
    {
        Security2->FileAuthentication = gSysSec2FileAuth;
        gSysSec2FileAuth = NULL;
    }

    if (Security && gSysSecFileAuth)
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

STATIC EFI_STATUS EFIAPI EnvInit(VOID)
{
    EFI_STATUS Status;
    EFI_GUID Guid = SHIM_LOCK_GUID;

    Status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, (VOID**)&gDpToText);
	if (EFI_ERROR(Status) || !gDpToText || !gDpToText->ConvertDevicePathToText)
    {
        vLog(L"Failed to locate PathToText Protocol %lx", Status);
        return Status;
    }

    Status = gBS->LocateProtocol(&gEfiDevicePathFromTextProtocolGuid, NULL, (VOID**)&gTextToDp);
	if (EFI_ERROR(Status) || !gTextToDp || !gTextToDp->ConvertTextToDevicePath)
    {
        vLog(L"Failed to locate PathFromText Protocol %lx", Status);
        return Status;
    }

    Status = gBS->LocateProtocol(&Guid, NULL, (VOID**)&gShimLock);
    if (EFI_ERROR(Status) || !gShimLock)
    {
        vLog(L"Failed to locate SHIM LOCK Protocol %lx", Status);
        return Status;
    }

    return EFI_SUCCESS;
}


EFI_STATUS EFIAPI VtoyShimEfiMain
(
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS Status;
    unhook_system_services_pf Func = NULL;

    /* If secure boot is not enabled, nothing needed, just launch Ventoy grub */
    if (!IsSecureBootEnabled())
    {
        return LaunchRealGrub(ImageHandle, REAL_GRUB_FILE);
    }

    Status = EnvInit();
	if (EFI_ERROR(Status))
    {
        vErr(L"Failed to prepare env");
        return Status;
    }

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
    Func = FindShimFuncAddr(NM_UNHOOK_SYSTEM_SERVICES_OFFSET);
    if (!Func)
    {
        vErr(L"Can not find shim unhook_system_services");
        Status = EFI_NOT_FOUND;
        goto END;
    }

    Func(); /* call shim unhook_system_services() */


    /* Hook the system security policy */
    Status = HookSecurityPolicy();
    if (EFI_ERROR(Status))
    {
        vErr(L"Failed to hook system security policy");
        goto END;
    }

    /* Finally launch Ventoy grub */
    Status = LaunchRealGrub(ImageHandle, REAL_GRUB_FILE);

END:

    /* UnHook system security policy */
    UnHookSecurityPolicy();

    UnInstallVtoyShimProtocol();

    return Status;
}

