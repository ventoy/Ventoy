#include <Windows.h>
#include <Shlobj.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "DiskService.h"
#include "VentoyJson.h"

extern void CLISetReserveSpace(int MB);

#define CLI_OP_INSTALL      0
#define CLI_OP_UPDATE       1
#define CLI_OP_LIST         2
#define CLI_OP_HELP         3
#define CLI_OP_VERSION      4
#define CLI_OP_SAFEINSTALL  5

static void CLIPrint(const char *Fmt, ...)
{
    va_list Arg;
    char szBuf[1024];
    va_start(Arg, Fmt);
    vsnprintf_s(szBuf, sizeof(szBuf), sizeof(szBuf) - 1, Fmt, Arg);
    va_end(Arg);
    printf("%s\n", szBuf);
}

typedef struct CLI_CFG
{
    int op;
    int PartStyle;
    int ReserveMB;
    BOOL USBCheck;
    BOOL NonDest;
    int fstype;
}CLI_CFG;

BOOL g_CLI_Mode = FALSE;
static int g_CLI_OP;
static int g_CLI_PhyDrive;

static PHY_DRIVE_INFO* g_CLI_PhyDrvInfo = NULL;

static int CLI_GetPhyDriveInfo(int PhyDrive, PHY_DRIVE_INFO* pInfo)
{
    BOOL bRet;
    DWORD dwBytes;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR PhyDrivePath[128];
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR* pDevDesc;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR diskAlignment;

    safe_sprintf(PhyDrivePath, "\\\\.\\PhysicalDrive%d", PhyDrive);
    Handle = CreateFileA(PhyDrivePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    Log("Create file Handle:%p %s status:%u", Handle, PhyDrivePath, LASTERR);

    if (Handle == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    bRet = DeviceIoControl(Handle,
        IOCTL_DISK_GET_LENGTH_INFO, NULL,
        0,
        &LengthInfo,
        sizeof(LengthInfo),
        &dwBytes,
        NULL);
    if (!bRet)
    {
        Log("DeviceIoControl IOCTL_DISK_GET_LENGTH_INFO failed error:%u", LASTERR);
        return 1;
    }

    Log("PHYSICALDRIVE%d size %llu bytes", PhyDrive, (ULONGLONG)LengthInfo.Length.QuadPart);

    Query.PropertyId = StorageDeviceProperty;
    Query.QueryType = PropertyStandardQuery;

    bRet = DeviceIoControl(Handle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &Query,
        sizeof(Query),
        &DevDescHeader,
        sizeof(STORAGE_DESCRIPTOR_HEADER),
        &dwBytes,
        NULL);
    if (!bRet)
    {
        Log("DeviceIoControl1 error:%u dwBytes:%u", LASTERR, dwBytes);
        return 1;
    }

    if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
        Log("Invalid DevDescHeader.Size:%u", DevDescHeader.Size);
        return 1;
    }

    pDevDesc = (STORAGE_DEVICE_DESCRIPTOR*)malloc(DevDescHeader.Size);
    if (!pDevDesc)
    {
        Log("failed to malloc error:%u len:%u", LASTERR, DevDescHeader.Size);
        return 1;
    }

    bRet = DeviceIoControl(Handle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &Query,
        sizeof(Query),
        pDevDesc,
        DevDescHeader.Size,
        &dwBytes,
        NULL);
    if (!bRet)
    {
        Log("DeviceIoControl2 error:%u dwBytes:%u", LASTERR, dwBytes);
        free(pDevDesc);
        return 1;
    }


    memset(&Query, 0, sizeof(STORAGE_PROPERTY_QUERY));
    Query.PropertyId = StorageAccessAlignmentProperty;
    Query.QueryType = PropertyStandardQuery;
    memset(&diskAlignment, 0, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR));

    bRet = DeviceIoControl(Handle,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &Query,
        sizeof(STORAGE_PROPERTY_QUERY),
        &diskAlignment,
        sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
        &dwBytes,
        NULL);
    if (!bRet)
    {
        Log("DeviceIoControl3 error:%u dwBytes:%u", LASTERR, dwBytes);
    }


    pInfo->PhyDrive = PhyDrive;
    pInfo->SizeInBytes = LengthInfo.Length.QuadPart;
    pInfo->DeviceType = pDevDesc->DeviceType;
    pInfo->RemovableMedia = pDevDesc->RemovableMedia;
    pInfo->BusType = pDevDesc->BusType;

    pInfo->BytesPerLogicalSector = diskAlignment.BytesPerLogicalSector;
    pInfo->BytesPerPhysicalSector = diskAlignment.BytesPerPhysicalSector;

    if (pDevDesc->VendorIdOffset)
    {
        safe_strcpy(pInfo->VendorId, (char*)pDevDesc + pDevDesc->VendorIdOffset);
        TrimString(pInfo->VendorId);
    }

    if (pDevDesc->ProductIdOffset)
    {
        safe_strcpy(pInfo->ProductId, (char*)pDevDesc + pDevDesc->ProductIdOffset);
        TrimString(pInfo->ProductId);
    }

    if (pDevDesc->ProductRevisionOffset)
    {
        safe_strcpy(pInfo->ProductRev, (char*)pDevDesc + pDevDesc->ProductRevisionOffset);
        TrimString(pInfo->ProductRev);
    }

    if (pDevDesc->SerialNumberOffset)
    {
        safe_strcpy(pInfo->SerialNumber, (char*)pDevDesc + pDevDesc->SerialNumberOffset);
        TrimString(pInfo->SerialNumber);
    }

    free(pDevDesc);

    CHECK_CLOSE_HANDLE(Handle);

    return 0;
}

static BOOL CLI_ConfirmDestructiveOperation(PHY_DRIVE_INFO *pDrvInfo)
{
    char Answer[16];

    if (g_NoNeedInputYes)
        return TRUE;

    CLIPrint("WARNING: ALL DATA on PhyDrive %d (%s %s, %d GB) will be LOST.",
             pDrvInfo->PhyDrive, pDrvInfo->VendorId, pDrvInfo->ProductId,
             GetHumanReadableGBSize(pDrvInfo->SizeInBytes));
    printf("Continue? (y/n): ");
    fflush(stdout);
    if (!fgets(Answer, sizeof(Answer), stdin) || (Answer[0] != 'y' && Answer[0] != 'Y'))
    { CLIPrint("Aborted."); return FALSE; }

    CLIPrint("WARNING: ALL DATA on PhyDrive %d will be LOST. Double-check.", pDrvInfo->PhyDrive);
    printf("Continue? (y/n): ");
    fflush(stdout);
    if (!fgets(Answer, sizeof(Answer), stdin) || (Answer[0] != 'y' && Answer[0] != 'Y'))
    { CLIPrint("Aborted."); return FALSE; }

    return TRUE;
}

static int CLI_CheckParam(int argc, char** argv, PHY_DRIVE_INFO* pDrvInfo, CLI_CFG *pCfg)
{
    int i;
    int fstype = VTOY_FS_EXFAT;
    int op = -1;
    char* opt = NULL;
    int PhyDrive = -1;
    int PartStyle = 0;
    int ReserveMB = 0;
    BOOL USBCheck = TRUE;
    BOOL NonDest = FALSE;
    MBR_HEAD MBR;
    UINT64 Part2GPTAttr = 0;
    UINT64 Part2StartSector = 0;

    for (i = 0; i < argc; i++)
    {
        opt = argv[i];
        if (_stricmp(opt, "/HELP") == 0 || _stricmp(opt, "/?") == 0)
        {
            op = CLI_OP_HELP;
        }
        else if (_stricmp(opt, "/V") == 0)
        {
            op = CLI_OP_VERSION;
        }
        else if (_stricmp(opt, "/L") == 0)
        {
            op = CLI_OP_LIST;
        }
        else if (_stricmp(opt, "/SI") == 0)
        {
            op = CLI_OP_SAFEINSTALL;
        }
        else if (_stricmp(opt, "/I") == 0)
        {
            op = CLI_OP_INSTALL;
        }
        else if (_stricmp(opt, "/U") == 0)
        {
            op = CLI_OP_UPDATE;
        }
        else if (_stricmp(opt, "/GPT") == 0)
        {
            PartStyle = 1;
        }
        else if (_stricmp(opt, "/NoSB") == 0)
        {
            g_SecureBoot = FALSE;
        }
        else if (_stricmp(opt, "/SB") == 0)
        {
            g_SecureBoot = TRUE;
        }
        else if (_stricmp(opt, "/Y") == 0)
        {
            g_NoNeedInputYes = 1;
        }
        else if (_stricmp(opt, "/NoUSBCheck") == 0)
        {
            USBCheck = FALSE;
        }
        else if (_stricmp(opt, "/NonDest") == 0)
        {
            NonDest = TRUE;
        }
        else if (_strnicmp(opt, "/Drive:", 7) == 0)
        {
            Log("Get PhyDrive by logical drive %C:", opt[7]);
            PhyDrive = GetPhyDriveByLogicalDrive(opt[7], NULL);
        }
        else if (_strnicmp(opt, "/PhyDrive:", 10) == 0)
        {
            PhyDrive = (int)strtol(opt + 10, NULL, 10);
        }
        else if (_strnicmp(opt, "/R:", 3) == 0)
        {
            ReserveMB = (int)strtol(opt + 3, NULL, 10);
        }
        else if (_strnicmp(opt, "/FS:", 4) == 0)
        {
            if (_stricmp(opt + 4, "NTFS") == 0)
            {
                fstype = VTOY_FS_NTFS;
            }
            else if (_stricmp(opt + 4, "FAT32") == 0)
            {
                fstype = VTOY_FS_FAT32;
            }
            else if (_stricmp(opt + 4, "UDF") == 0)
            {
                fstype = VTOY_FS_UDF;
            }
        }
        else if (_strnicmp(opt, "/Label:", 7) == 0)
        {
            if (strlen(opt + 7) > 32)
            {
                CLIPrint("[ERROR] /Label: value too long (max 32 chars)");
                return 1;
            }
            safe_strcpy(g_VolumeLabel, opt + 7);
        }
    }

    if (op == CLI_OP_HELP || op == CLI_OP_VERSION)
    {
        pCfg->op = op;
        return 0;
    }

    if (op < 0 || PhyDrive < 0)
    {
        Log("[ERROR] Invalid parameters %d %d", op, PhyDrive);
        return 1;
    }

    Log("Ventoy CLI op:%d PhyDrive:%d %s SecureBoot:%d ReserveSpace:%dMB USBCheck:%u FS:%s NonDest:%d",
        op, PhyDrive, PartStyle ? "GPT" : "MBR",
        g_SecureBoot, ReserveMB, USBCheck, GetVentoyFsFmtNameByTypeA(fstype), NonDest
        );

    if (CLI_GetPhyDriveInfo(PhyDrive, pDrvInfo))
    {
        Log("[ERROR] Failed to get phydrive%d info", PhyDrive);
        return 1;
    }

    Log("PhyDrive:%d BusType:%-4s Removable:%u Size:%dGB(%llu) Name:%s %s",
        pDrvInfo->PhyDrive, GetBusTypeString(pDrvInfo->BusType), pDrvInfo->RemovableMedia,
        GetHumanReadableGBSize(pDrvInfo->SizeInBytes), pDrvInfo->SizeInBytes,
        pDrvInfo->VendorId, pDrvInfo->ProductId);

    if (op == CLI_OP_LIST)
    {
        GetLettersBelongPhyDrive(PhyDrive, pDrvInfo->DriveLetters, sizeof(pDrvInfo->DriveLetters));
    }

    if (IsVentoyPhyDrive(PhyDrive, pDrvInfo->SizeInBytes, &MBR, &Part2StartSector, &Part2GPTAttr))
    {
        memcpy(&(pDrvInfo->MBR), &MBR, sizeof(MBR));
        pDrvInfo->PartStyle = (MBR.PartTbl[0].FsFlag == 0xEE) ? 1 : 0;
        pDrvInfo->Part2GPTAttr = Part2GPTAttr;
        GetVentoyVerInPhyDrive(pDrvInfo, Part2StartSector, pDrvInfo->VentoyVersion, sizeof(pDrvInfo->VentoyVersion), &(pDrvInfo->SecureBootSupport));
        Log("PhyDrive %d is Ventoy Disk ver:%s SecureBoot:%u", pDrvInfo->PhyDrive, pDrvInfo->VentoyVersion, pDrvInfo->SecureBootSupport);

        GetVentoyFsNameInPhyDrive(pDrvInfo);

        if (pDrvInfo->VentoyVersion[0] == 0)
        {
            pDrvInfo->VentoyVersion[0] = '?';
            Log("Unknown Ventoy Version");
        }
    }

    if (op == CLI_OP_INSTALL && NonDest)
    {
        GetLettersBelongPhyDrive(PhyDrive, pDrvInfo->DriveLetters, sizeof(pDrvInfo->DriveLetters));
    }

    pCfg->op = op;
    pCfg->PartStyle = PartStyle;
    pCfg->ReserveMB = ReserveMB;
    pCfg->USBCheck = USBCheck;
    pCfg->NonDest = NonDest;
    pCfg->fstype = fstype;

    return 0;
}

static int Ventoy_CLI_NonDestInstall(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG* pCfg)
{
    int rc;
    int TryId = 1;

    Log("Ventoy_CLI_NonDestInstall start ...");

    if (pDrvInfo->BytesPerLogicalSector == 4096 && pDrvInfo->BytesPerPhysicalSector == 4096)
    {
        Log("Ventoy does not support 4k native disk.");
        rc = 1;
        goto out;
    }

    if (!PartResizePreCheck(NULL))
    {
        Log("#### Part Resize PreCheck Failed ####");
        rc = 1;
        goto out;
    }

    rc = PartitionResizeForVentoy(pDrvInfo);

out:
    Log("Ventoy_CLI_NonDestInstall [%s]", rc == 0 ? "SUCCESS" : "FAILED");

    return rc;
}


static int Ventoy_CLI_Install(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG *pCfg)
{
    int rc; 
    int TryId = 1;    

    Log("Ventoy_CLI_Install start ...");
    
    if (pDrvInfo->BytesPerLogicalSector == 4096 && pDrvInfo->BytesPerPhysicalSector == 4096)
    {
        Log("Ventoy does not support 4k native disk.");
        rc = 1;
        goto out;
    }

    if (pCfg->ReserveMB > 0)
    {
        CLISetReserveSpace(pCfg->ReserveMB);
    }

    SetVentoyFsType(pCfg->fstype);

    rc = InstallVentoy2PhyDrive(pDrvInfo, pCfg->PartStyle, TryId++);
    if (rc)
    {
        Log("This time install failed, clean disk by disk, wait 3s and retry...");
        DISK_CleanDisk(pDrvInfo->PhyDrive);

        Sleep(3000);

        Log("Now retry to install...");
        rc = InstallVentoy2PhyDrive(pDrvInfo, pCfg->PartStyle, TryId++);

        if (rc)
        {
            Log("This time install failed, clean disk by diskpart, wait 5s and retry...");
            DSPT_CleanDisk(pDrvInfo->PhyDrive);

            Sleep(5000);

            Log("Now retry to install...");
            rc = InstallVentoy2PhyDrive(pDrvInfo, pCfg->PartStyle, TryId++);
        }
    }

    SetVentoyFsType(VTOY_FS_EXFAT);

out:
    Log("Ventoy_CLI_Install [%s]", rc == 0 ? "SUCCESS" : "FAILED");

    return rc;
}

static int Ventoy_CLI_Update(PHY_DRIVE_INFO* pDrvInfo, CLI_CFG* pCfg)
{
    int rc; 
    int TryId = 1;
    
    Log("Ventoy_CLI_Update start ...");

    rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
    if (rc)
    {
        Log("This time update failed, now wait and retry...");
        Sleep(4000);

        //Try2
        Log("Now retry to update...");
        rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
        if (rc)
        {
            //Try3
            Sleep(1000);
            Log("Now retry to update...");
            rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
            if (rc)
            {
                //Try4 is dangerous ...
                Sleep(3000);
                Log("Now retry to update...");
                rc = UpdateVentoy2PhyDrive(pDrvInfo, TryId++);
            }
        }
    }

    Log("Ventoy_CLI_Update [%s]", rc == 0 ? "SUCCESS" : "FAILED");

    return rc;
}

void CLI_UpdatePercent(int Pos)
{
    int Len;
    FILE* File = NULL;
    CHAR szBuf[128];

    Len = (int)sprintf_s(szBuf, sizeof(szBuf), "%d", Pos * 100 / PT_FINISH);
    fopen_s(&File, VENTOY_CLI_PERCENT, "w+");
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fwrite("\n", 1, 1, File);
        fclose(File);
    }
}

static void CLI_WriteDoneFile(int ret)
{
    FILE* File = NULL;

    fopen_s(&File, VENTOY_CLI_DONE, "w+");
    if (File)
    {
        if (ret == 0)
        {
            fwrite("0\n", 1, 2, File);
        }
        else
        {
            fwrite("1\n", 1, 2, File);
        }
        fclose(File);
    }
}

PHY_DRIVE_INFO* CLI_PhyDrvInfo(void)
{
    return g_CLI_PhyDrvInfo;
}

/*
 * Ventoy2Disk.exe VTOYCLI CMD [OPTION] { /Drive:F: | /PhyDrive:N }
 *
 * CMD: /I /SI /U /L /V /HELP /?
 * OPTION: /GPT /NoSB /SB /R:MB /FS:TYPE /Label:NAME /NoUSBCheck /NonDest /Y
 *
 * Run with /HELP for full usage.
 */
int VentoyCLIMain(int argc, char** argv)
{
    int ret = 1;
    PHY_DRIVE_INFO* pDrvInfo = NULL;
    CLI_CFG CliCfg;

    DeleteFileA(VENTOY_CLI_PERCENT);
    DeleteFileA(VENTOY_CLI_DONE);

    {
        HANDLE hOut   = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hIn    = GetStdHandle(STD_INPUT_HANDLE);
        DWORD  outType = (hOut && hOut != INVALID_HANDLE_VALUE) ? GetFileType(hOut) : FILE_TYPE_UNKNOWN;
        DWORD  inType  = (hIn  && hIn  != INVALID_HANDLE_VALUE) ? GetFileType(hIn)  : FILE_TYPE_UNKNOWN;

        if (outType == FILE_TYPE_PIPE || outType == FILE_TYPE_DISK)
        {
            /* Launched with redirected stdout (pipe capture or "> file") —
             * hook C runtime stdout to the inherited Win32 handle. */
            int fd = _open_osfhandle((intptr_t)hOut, _O_WRONLY | _O_TEXT);
            if (fd >= 0) { FILE *fp = _fdopen(fd, "w"); if (fp) { *stdout = *fp; setvbuf(stdout, NULL, _IONBF, 0); } }
        }
        else if (AttachConsole(ATTACH_PARENT_PROCESS))
        {
            /* Launched from an interactive console (e.g. cmd.exe). */
            FILE *fp;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            if (inType != FILE_TYPE_PIPE && inType != FILE_TYPE_DISK)
                freopen_s(&fp, "CONIN$", "r", stdin);
        }

        if (inType == FILE_TYPE_PIPE || inType == FILE_TYPE_DISK)
        {
            int fd = _open_osfhandle((intptr_t)hIn, _O_RDONLY | _O_TEXT);
            if (fd >= 0) { FILE *fp = _fdopen(fd, "r"); if (fp) { *stdin = *fp; } }
        }
    }

    g_CLI_PhyDrvInfo = pDrvInfo = (PHY_DRIVE_INFO*)malloc(sizeof(PHY_DRIVE_INFO));
    if (!pDrvInfo)
    {
        goto end;
    }
    memset(pDrvInfo, 0, sizeof(PHY_DRIVE_INFO));

    if (CLI_CheckParam(argc, argv, pDrvInfo, &CliCfg))
    {
        goto end;
    }

    if (CliCfg.op == CLI_OP_HELP)
    {
        CLIPrint("Usage: Ventoy2Disk.exe VTOYCLI [CMD] [OPTION] { /Drive:F: | /PhyDrive:N }");
        CLIPrint("");
        CLIPrint("CMD (one required):");
        CLIPrint("  /I          Force install (overwrites existing; no prompts)");
        CLIPrint("  /SI         Safe install (fails if Ventoy present; prompts for confirmation)");
        CLIPrint("  /U          Update Ventoy on disk");
        CLIPrint("  /L          List Ventoy info on disk (read-only)");
        CLIPrint("  /V          Print local Ventoy version and exit");
        CLIPrint("  /HELP /?    Show this help");
        CLIPrint("");
        CLIPrint("OPTION (optional):");
        CLIPrint("  /GPT           GPT partition style (default: MBR; install only)");
        CLIPrint("  /NoSB          Disable secure boot support (default: enabled)");
        CLIPrint("  /SB            Enable secure boot support explicitly");
        CLIPrint("  /R:SIZE_MB     Reserve SIZE_MB megabytes at end of disk");
        CLIPrint("  /FS:TYPE       Filesystem: exFAT (default), NTFS, FAT32, UDF");
        CLIPrint("  /Label:NAME    Volume label for partition 1 (default: Ventoy)");
        CLIPrint("  /NoUSBCheck    Allow installation on non-USB drives");
        CLIPrint("  /NonDest       Non-destructive install (resize existing partition)");
        CLIPrint("  /Y             Auto-confirm prompts (for scripted/silent use)");
        CLIPrint("");
        CLIPrint("Drive (required for all CMDs except /V):");
        CLIPrint("  /Drive:F:      Select by logical drive letter");
        CLIPrint("  /PhyDrive:N    Select by physical drive number");
        ret = 0;
        goto end;
    }

    if (CliCfg.op == CLI_OP_VERSION)
    {
        CLIPrint("%s", GetLocalVentoyVersion());
        ret = 0;
        goto end;
    }

    if (CliCfg.op == CLI_OP_LIST)
    {
        if (pDrvInfo->VentoyVersion[0] == 0 || pDrvInfo->VentoyVersion[0] == '?')
        {
            CLIPrint("Ventoy Version in Disk : N/A");
            ret = 1;
        }
        else
        {
            CLIPrint("Ventoy Version in Disk : %s", pDrvInfo->VentoyVersion);
            CLIPrint("Disk Partition Style   : %s", pDrvInfo->PartStyle ? "GPT" : "MBR");
            CLIPrint("Secure Boot Support    : %s", pDrvInfo->SecureBootSupport ? "YES" : "NO");
            CLIPrint("Filesystem Type        : %s", pDrvInfo->VentoyFsType);
            CLIPrint("Disk Size              : %d GB", GetHumanReadableGBSize(pDrvInfo->SizeInBytes));
            CLIPrint("Bus Type               : %s", GetBusTypeString(pDrvInfo->BusType));
            ret = 0;
        }
        goto end;
    }

    if (CliCfg.op == CLI_OP_SAFEINSTALL)
    {
        if (pDrvInfo->VentoyVersion[0] != 0 && pDrvInfo->VentoyVersion[0] != '?')
        {
            CLIPrint("[ERROR] PhyDrive %d already has Ventoy %s.",
                     pDrvInfo->PhyDrive, pDrvInfo->VentoyVersion);
            CLIPrint("        Use /U to update, or /I to force-reinstall.");
            ret = 1;
            goto end;
        }
        if (!CLI_ConfirmDestructiveOperation(pDrvInfo))
        {
            ret = 0;
            goto end;
        }
        CliCfg.op = CLI_OP_INSTALL;
    }

    //Check USB type for install
    if (CliCfg.op == CLI_OP_INSTALL && CliCfg.USBCheck)
    {
        if (pDrvInfo->BusType != BusTypeUsb)
        {
            Log("[ERROR] PhyDrive %d is NOT USB type",  pDrvInfo->PhyDrive);
            goto end;
        }
    }

    if (CliCfg.op == CLI_OP_INSTALL)
    {
        if (CliCfg.NonDest)
        {
            ret = Ventoy_CLI_NonDestInstall(pDrvInfo, &CliCfg);
        }
        else
        {
            AlertSuppressInit();
            SetAlertPromptHookEnable(TRUE);
            ret = Ventoy_CLI_Install(pDrvInfo, &CliCfg);
        }
    }
    else
    {
        if (pDrvInfo->VentoyVersion[0] == 0)
        {
            Log("[ERROR] No Ventoy information detected in PhyDrive %d, so can not do update", pDrvInfo->PhyDrive);
            goto end;
        }

        ret = Ventoy_CLI_Update(pDrvInfo, &CliCfg);
    }

end:
    CHECK_FREE(pDrvInfo);

    CLI_UpdatePercent(PT_FINISH);
    CLI_WriteDoneFile(ret);

    return ret;
}
