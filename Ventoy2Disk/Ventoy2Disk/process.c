/*
* Rufus: The Reliable USB Formatting Utility
* Process search functionality
*
* Modified from Process Hacker:
*   https://github.com/processhacker2/processhacker2/
* Copyright © 2017-2019 Pete Batard <pete@akeo.ie>
* Copyright © 2017 dmex
* Copyright © 2009-2016 wj32
* Copyright (c) 2020, longpanda <admin@ventoy.net>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <Windows.h>
#include <time.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include <vds.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "fat_filelib.h"
#include "ff.h"
#include "process.h"
#include <Psapi.h>


OPENED_LIBRARIES_VARS;

STATIC WCHAR *_wHandleName = NULL;
static PVOID PhHeapHandle = NULL;


/*
* Convert an NT Status to an error message
*
* \param Status An operattonal status.
*
* \return An error message string.
*
*/
char* NtStatusError(NTSTATUS Status) {
    static char unknown[32];

    switch (Status) {
    case STATUS_SUCCESS:
        return "Operation Successful";
    case STATUS_UNSUCCESSFUL:
        return "Operation Failed";
    case STATUS_BUFFER_OVERFLOW:
        return "Buffer Overflow";
    case STATUS_NOT_IMPLEMENTED:
        return "Not Implemented";
    case STATUS_INFO_LENGTH_MISMATCH:
        return "Info Length Mismatch";
    case STATUS_INVALID_HANDLE:
        return "Invalid Handle.";
    case STATUS_INVALID_PARAMETER:
        return "Invalid Parameter";
    case STATUS_NO_MEMORY:
        return "Not Enough Quota";
    case STATUS_ACCESS_DENIED:
        return "Access Denied";
    case STATUS_BUFFER_TOO_SMALL:
        return "Buffer Too Small";
    case STATUS_OBJECT_TYPE_MISMATCH:
        return "Wrong Type";
    case STATUS_OBJECT_NAME_INVALID:
        return "Object Name Invalid";
    case STATUS_OBJECT_NAME_NOT_FOUND:
        return "Object Name not found";
    case STATUS_OBJECT_PATH_INVALID:
        return "Object Path Invalid";
    case STATUS_SHARING_VIOLATION:
        return "Sharing Violation";
    case STATUS_INSUFFICIENT_RESOURCES:
        return "Insufficient resources";
    case STATUS_NOT_SUPPORTED:
        return "Operation is not supported";
    default:
        safe_sprintf(unknown, "Unknown error 0x%08lx", Status);
        return unknown;
    }
}


static NTSTATUS PhCreateHeap(VOID)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (PhHeapHandle != NULL)
        return STATUS_ALREADY_COMPLETE;

    PF_INIT_OR_SET_STATUS(RtlCreateHeap, Ntdll);

    if (NT_SUCCESS(status)) {
        PhHeapHandle = pfRtlCreateHeap(HEAP_NO_SERIALIZE | HEAP_GROWABLE, NULL, 2 * MB, 1 * MB, NULL, NULL);
        if (PhHeapHandle == NULL)
            status = STATUS_UNSUCCESSFUL;
    }

    return status;
}

static NTSTATUS PhDestroyHeap(VOID)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (PhHeapHandle == NULL)
        return STATUS_ALREADY_COMPLETE;

    PF_INIT_OR_SET_STATUS(RtlDestroyHeap, Ntdll);

    if (NT_SUCCESS(status)) {
        if (pfRtlDestroyHeap(PhHeapHandle) == NULL) {
            PhHeapHandle = NULL;
        }
        else {
            status = STATUS_UNSUCCESSFUL;
        }
    }

    return status;
}

/**
* Allocates a block of memory.
*
* \param Size The number of bytes to allocate.
*
* \return A pointer to the allocated block of memory.
*
*/
static PVOID PhAllocate(SIZE_T Size)
{
    PF_INIT(RtlAllocateHeap, Ntdll);
    if (pfRtlAllocateHeap == NULL)
        return NULL;

    return pfRtlAllocateHeap(PhHeapHandle, 0, Size);
}

/**
* Frees a block of memory allocated with PhAllocate().
*
* \param Memory A pointer to a block of memory.
*
*/
static VOID PhFree(PVOID Memory)
{
    PF_INIT(RtlFreeHeap, Ntdll);

    if (pfRtlFreeHeap != NULL)
        pfRtlFreeHeap(PhHeapHandle, 0, Memory);
}

/**
* Enumerates all open handles.
*
* \param Handles A variable which receives a pointer to a structure containing information about
* all opened handles. You must free the structure using PhFree() when you no longer need it.
*
* \return An NTStatus indicating success or the error code.
*/
NTSTATUS PhEnumHandlesEx(PSYSTEM_HANDLE_INFORMATION_EX *Handles)
{
    static ULONG initialBufferSize = 0x10000;
    NTSTATUS status = STATUS_SUCCESS;
    PVOID buffer;
    ULONG bufferSize;

    PF_INIT_OR_SET_STATUS(NtQuerySystemInformation, Ntdll);
    if (!NT_SUCCESS(status))
        return status;

    bufferSize = initialBufferSize;
    buffer = PhAllocate(bufferSize);
    if (buffer == NULL)
        return STATUS_NO_MEMORY;

    while ((status = pfNtQuerySystemInformation(SystemExtendedHandleInformation,
        buffer, bufferSize, NULL)) == STATUS_INFO_LENGTH_MISMATCH) {
        PhFree(buffer);
        bufferSize *= 2;

        // Fail if we're resizing the buffer to something very large.
        if (bufferSize > PH_LARGE_BUFFER_SIZE)
            return STATUS_INSUFFICIENT_RESOURCES;

        buffer = PhAllocate(bufferSize);
        if (buffer == NULL)
            return STATUS_NO_MEMORY;
    }

    if (!NT_SUCCESS(status)) {
        PhFree(buffer);
        return status;
    }

    if (bufferSize <= 0x200000)
        initialBufferSize = bufferSize;
    *Handles = (PSYSTEM_HANDLE_INFORMATION_EX)buffer;

    return status;
}

/**
* Opens a process.
*
* \param ProcessHandle A variable which receives a handle to the process.
* \param DesiredAccess The desired access to the process.
* \param ProcessId The ID of the process.
*
* \return An NTStatus indicating success or the error code.
*/
NTSTATUS PhOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, HANDLE ProcessId)
{
    NTSTATUS status = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES objectAttributes;
    CLIENT_ID clientId;

    if ((LONG_PTR)ProcessId == (LONG_PTR)GetCurrentProcessId()) {
        *ProcessHandle = NtCurrentProcess();
        return 0;
    }

    PF_INIT_OR_SET_STATUS(NtOpenProcess, Ntdll);
    if (!NT_SUCCESS(status))
        return status;

    clientId.UniqueProcess = ProcessId;
    clientId.UniqueThread = NULL;

    InitializeObjectAttributes(&objectAttributes, NULL, 0, NULL, NULL);
    status = pfNtOpenProcess(ProcessHandle, DesiredAccess, &objectAttributes, &clientId);

    return status;
}

/**
* Query processes with open handles to a file, volume or disk.
*
* \param VolumeOrFileHandle The handle to the target.
* \param Information The returned list of processes.
*
* \return An NTStatus indicating success or the error code.
*/
NTSTATUS PhQueryProcessesUsingVolumeOrFile(HANDLE VolumeOrFileHandle,
    PFILE_PROCESS_IDS_USING_FILE_INFORMATION *Information)
{
    static ULONG initialBufferSize = 16 * KB;
    NTSTATUS status = STATUS_SUCCESS;
    PVOID buffer;
    ULONG bufferSize;
    IO_STATUS_BLOCK isb;

    PF_INIT_OR_SET_STATUS(NtQueryInformationFile, NtDll);
    if (!NT_SUCCESS(status))
        return status;

    bufferSize = initialBufferSize;
    buffer = PhAllocate(bufferSize);
    if (buffer == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

    while ((status = pfNtQueryInformationFile(VolumeOrFileHandle, &isb, buffer, bufferSize,
        FileProcessIdsUsingFileInformation)) == STATUS_INFO_LENGTH_MISMATCH) {
        PhFree(buffer);
        bufferSize *= 2;
        // Fail if we're resizing the buffer to something very large.
        if (bufferSize > 64 * MB)
            return STATUS_INSUFFICIENT_RESOURCES;
        buffer = PhAllocate(bufferSize);
    }

    if (!NT_SUCCESS(status)) {
        PhFree(buffer);
        return status;
    }

    if (bufferSize <= 64 * MB)
        initialBufferSize = bufferSize;
    *Information = (PFILE_PROCESS_IDS_USING_FILE_INFORMATION)buffer;

    return status;
}

/**
* Query the full commandline that was used to create a process.
* This can be helpful to differentiate between service instances (svchost.exe).
* Taken from: https://stackoverflow.com/a/14012919/1069307
*
* \param hProcess A handle to a process.
*
* \return A Unicode commandline string, or NULL on error.
*         The returned string must be freed by the caller.
*/
static PWSTR GetProcessCommandLine(HANDLE hProcess)
{
    PWSTR wcmdline = NULL;
    BOOL wow;
    DWORD pp_offset, cmd_offset;
    NTSTATUS status = STATUS_SUCCESS;
    SYSTEM_INFO si;
    PBYTE peb = NULL, pp = NULL;

    // Determine if 64 or 32-bit processor
    GetNativeSystemInfo(&si);
    if ((si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) || (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)) {
        pp_offset = 0x20;
        cmd_offset = 0x70;
    }
    else {
        pp_offset = 0x10;
        cmd_offset = 0x40;
    }

    // PEB and Process Parameters (we only need the beginning of these structs)
    peb = (PBYTE)calloc(pp_offset + 8, 1);
    if (peb == NULL)
        goto out;
    pp = (PBYTE)calloc(cmd_offset + 16, 1);
    if (pp == NULL)
        goto out;

    IsWow64Process(GetCurrentProcess(), &wow);
    if (wow) {
        // 32-bit process running on a 64-bit OS
        PROCESS_BASIC_INFORMATION_WOW64 pbi = { 0 };
        ULONGLONG params;
        UNICODE_STRING_WOW64* ucmdline;

        PF_INIT_OR_OUT(NtWow64QueryInformationProcess64, NtDll);
        PF_INIT_OR_OUT(NtWow64ReadVirtualMemory64, NtDll);

        status = pfNtWow64QueryInformationProcess64(hProcess, 0, &pbi, sizeof(pbi), NULL);
        if (!NT_SUCCESS(status))
            goto out;

        status = pfNtWow64ReadVirtualMemory64(hProcess, pbi.PebBaseAddress, peb, pp_offset + 8, NULL);
        if (!NT_SUCCESS(status))
            goto out;

        // Read Process Parameters from the 64-bit address space
        params = (ULONGLONG)*((ULONGLONG*)(peb + pp_offset));
        status = pfNtWow64ReadVirtualMemory64(hProcess, params, pp, cmd_offset + 16, NULL);
        if (!NT_SUCCESS(status))
            goto out;

        ucmdline = (UNICODE_STRING_WOW64*)(pp + cmd_offset);
        wcmdline = (PWSTR)calloc(ucmdline->Length + 1, sizeof(WCHAR));
        if (wcmdline == NULL)
            goto out;
        status = pfNtWow64ReadVirtualMemory64(hProcess, ucmdline->Buffer, wcmdline, ucmdline->Length, NULL);
        if (!NT_SUCCESS(status)) {
            safe_free(wcmdline);
            goto out;
        }
    }
    else {
        // 32-bit process on a 32-bit OS, or 64-bit process on a 64-bit OS
        PROCESS_BASIC_INFORMATION pbi = { 0 };
        PBYTE* params;
        UNICODE_STRING* ucmdline;

        PF_INIT_OR_OUT(NtQueryInformationProcess, NtDll);

        status = pfNtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), NULL);
        if (!NT_SUCCESS(status))
            goto out;

        // Read PEB
        if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, peb, pp_offset + 8, NULL))
            goto out;

        // Read Process Parameters
        params = (PBYTE*)*(LPVOID*)(peb + pp_offset);
        if (!ReadProcessMemory(hProcess, params, pp, cmd_offset + 16, NULL))
            goto out;

        ucmdline = (UNICODE_STRING*)(pp + cmd_offset);
        // In the absolute, someone could craft a process with dodgy attributes to try to cause an overflow
        ucmdline->Length = min(ucmdline->Length, 512);
        wcmdline = (PWSTR)calloc(ucmdline->Length + 1, sizeof(WCHAR));
        if (!ReadProcessMemory(hProcess, ucmdline->Buffer, wcmdline, ucmdline->Length, NULL)) {
            safe_free(wcmdline);
            goto out;
        }
    }

out:
    free(peb);
    free(pp);
    return wcmdline;
}


static int GetDevicePathName(PHY_DRIVE_INFO *pPhyDrive, WCHAR *wDevPath)
{
    int i;
    CHAR PhyDrive[128];
    CHAR DevPath[MAX_PATH] = { 0 };

    safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", pPhyDrive->PhyDrive);

    if (0 == QueryDosDeviceA(PhyDrive + 4, DevPath, sizeof(DevPath)))
    {
        Log("QueryDosDeviceA failed error:%u", GetLastError());
        strcpy_s(DevPath, sizeof(DevPath), "???");
    }
    else
    {
        Log("QueryDosDeviceA success %s", DevPath);
    }

    for (i = 0; DevPath[i] && i < MAX_PATH; i++)
    {
        wDevPath[i] = DevPath[i];
    }

    return 0;
}


static __inline DWORD GetModuleFileNameExU(HANDLE hProcess, HMODULE hModule, char* lpFilename, DWORD nSize)
{
    DWORD ret = 0, err = ERROR_INVALID_DATA;
    // coverity[returned_null]
    walloc(lpFilename, nSize);
    ret = GetModuleFileNameExW(hProcess, hModule, wlpFilename, nSize);
    err = GetLastError();
    if ((ret != 0)
        && ((ret = wchar_to_utf8_no_alloc(wlpFilename, lpFilename, nSize)) == 0)) {
        err = GetLastError();
    }
    wfree(lpFilename);
    SetLastError(err);
    return ret;
}

int FindProcessOccupyDisk(HANDLE hDrive, PHY_DRIVE_INFO *pPhyDrive)
{
    WCHAR wDevPath[MAX_PATH] = { 0 };
    const char *access_rights_str[8] = { "n", "r", "w", "rw", "x", "rx", "wx", "rwx" };    
    NTSTATUS status = STATUS_SUCCESS;
    PSYSTEM_HANDLE_INFORMATION_EX handles = NULL;
    POBJECT_NAME_INFORMATION buffer = NULL;
    ULONG_PTR i;
    ULONG_PTR pid[2];
    ULONG_PTR last_access_denied_pid = 0;
    ULONG bufferSize;
    USHORT wHandleNameLen;
    HANDLE dupHandle = NULL;
    HANDLE processHandle = NULL;
    BOOLEAN bFound = FALSE, bGotCmdLine, verbose = TRUE;
    ULONG access_rights = 0;
    DWORD size;
    char cmdline[MAX_PATH] = { 0 };
    wchar_t wexe_path[MAX_PATH], *wcmdline;
    int cur_pid;
	time_t starttime, curtime;


    Log("FindProcessOccupyDisk for PhyDrive %d", pPhyDrive->PhyDrive);

    GetDevicePathName(pPhyDrive, wDevPath);
    _wHandleName = wDevPath;


    PF_INIT_OR_SET_STATUS(NtQueryObject, Ntdll);
    PF_INIT_OR_SET_STATUS(NtDuplicateObject, NtDll);
    PF_INIT_OR_SET_STATUS(NtClose, NtDll);

    if (NT_SUCCESS(status))
        status = PhCreateHeap();

    if (NT_SUCCESS(status))
        status = PhEnumHandlesEx(&handles);

    if (!NT_SUCCESS(status)) {
        Log("Warning: Could not enumerate process handles: %s", NtStatusError(status));
        goto out;
    }

    pid[0] = (ULONG_PTR)0;
    cur_pid = 1;

    wHandleNameLen = (USHORT)wcslen(_wHandleName);

    bufferSize = 0x200;
    buffer = PhAllocate(bufferSize);
    if (buffer == NULL)
        goto out;

	Log("handles->NumberOfHandles = %lu", (ULONG)handles->NumberOfHandles);

	if (handles->NumberOfHandles > 10000)
	{
		goto out;
	}

	starttime = time(NULL);

	for (i = 0; i < handles->NumberOfHandles; i++) {
        ULONG attempts = 8;
        PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handleInfo =
            (i < handles->NumberOfHandles) ? &handles->Handles[i] : NULL;

		//limit the search time
		if ((i % 100) == 0)
		{
			curtime = time(NULL);
			if (curtime - starttime > 10)
			{
				break;
			}
		}

        if ((dupHandle != NULL) && (processHandle != NtCurrentProcess())) {
            pfNtClose(dupHandle);
            dupHandle = NULL;
        }

        // Update the current handle's process PID and compare against last
        // Note: Be careful about not trying to overflow our list!
        pid[cur_pid] = (handleInfo != NULL) ? handleInfo->UniqueProcessId : -1;

        if (pid[0] != pid[1]) {
            cur_pid = (cur_pid + 1) % 2;

            // If we're switching process and found a match, print it
            if (bFound) {
                Log("* [%06u] %s (%s)", (UINT32)pid[cur_pid], cmdline, access_rights_str[access_rights & 0x7]);
                bFound = FALSE;
                access_rights = 0;
            }

            // Close the previous handle
            if (processHandle != NULL) {
                if (processHandle != NtCurrentProcess())
                    pfNtClose(processHandle);
                processHandle = NULL;
            }
        }

        // Exit loop condition
        if (i >= handles->NumberOfHandles)
            break;

        // Don't bother with processes we can't access
        if (handleInfo->UniqueProcessId == last_access_denied_pid)
            continue;

        // Filter out handles that aren't opened with Read (bit 0), Write (bit 1) or Execute (bit 5) access
        if ((handleInfo->GrantedAccess & 0x23) == 0)
            continue;

        // Open the process to which the handle we are after belongs, if not already opened
        if (pid[0] != pid[1]) {
            status = PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                (HANDLE)handleInfo->UniqueProcessId);
            // There exists some processes we can't access
            if (!NT_SUCCESS(status)) {
                //Log("SearchProcess: Could not open process %ld: %s",
                //    handleInfo->UniqueProcessId, NtStatusError(status));
                processHandle = NULL;
                if (status == STATUS_ACCESS_DENIED) {
                    last_access_denied_pid = handleInfo->UniqueProcessId;
                }
                continue;
            }            
        }

        // Now duplicate this handle onto our own process, so that we can access its properties
        if (processHandle == NtCurrentProcess()) {
            continue;            
        }
        else {
            status = pfNtDuplicateObject(processHandle, (HANDLE)handleInfo->HandleValue,
                NtCurrentProcess(), &dupHandle, 0, 0, 0);
            if (!NT_SUCCESS(status))
                continue;
        }

        // Filter non-storage handles. We're not interested in them and they make NtQueryObject() freeze
        if (GetFileType(dupHandle) != FILE_TYPE_DISK)
            continue;

        // A loop is needed because the I/O subsystem likes to give us the wrong return lengths...
        do {
            ULONG returnSize;
            // TODO: We might potentially still need a timeout on ObjectName queries, as PH does...
            status = pfNtQueryObject(dupHandle, ObjectNameInformation, buffer, bufferSize, &returnSize);
            if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH ||
                status == STATUS_BUFFER_TOO_SMALL) {
                Log("SearchProcess: Realloc from %d to %d", bufferSize, returnSize);
                bufferSize = returnSize;
                PhFree(buffer);
                buffer = PhAllocate(bufferSize);
            }
            else {
                break;
            }
        } while (--attempts);
        if (!NT_SUCCESS(status)) {
            Log("SearchProcess: NtQueryObject failed for handle %X of process %ld: %s",
                handleInfo->HandleValue, handleInfo->UniqueProcessId, NtStatusError(status));
            continue;
        }

        // we are looking for a partial match and the current length is smaller
        if (wHandleNameLen > buffer->Name.Length)
            continue;

        // Match against our target string
        if (wcsncmp(_wHandleName, buffer->Name.Buffer, wHandleNameLen) != 0)
            continue;

        // If we are here, we have a process accessing our target!
        bFound = TRUE;

        // Keep a mask of all the access rights being used
        access_rights |= handleInfo->GrantedAccess;
        // The Executable bit is in a place we don't like => reposition it
        if (access_rights & 0x20)
            access_rights = (access_rights & 0x03) | 0x04;

        // If this is the very first process we find, print a header
        if (cmdline[0] == 0)
            Log("WARNING: The following process(es) or service(s) are accessing %S:", _wHandleName);

        // Where possible, try to get the full command line
        bGotCmdLine = FALSE;
        size = MAX_PATH;
        wcmdline = GetProcessCommandLine(processHandle);
        if (wcmdline != NULL) {
            bGotCmdLine = TRUE;
            wchar_to_utf8_no_alloc(wcmdline, cmdline, sizeof(cmdline));
            free(wcmdline);
        }

        // If we couldn't get the full commandline, try to get the executable path
        if (!bGotCmdLine)
            bGotCmdLine = (GetModuleFileNameExU(processHandle, 0, cmdline, MAX_PATH - 1) != 0);

        // The above may not work on Windows 7, so try QueryFullProcessImageName (Vista or later)
        if (!bGotCmdLine) {
            bGotCmdLine = QueryFullProcessImageNameW(processHandle, 0, wexe_path, &size);
            if (bGotCmdLine)
                wchar_to_utf8_no_alloc(wexe_path, cmdline, sizeof(cmdline));
        }

        // Still nothing? Try GetProcessImageFileName. Note that GetProcessImageFileName uses
        // '\Device\Harddisk#\Partition#\' instead drive letters
        if (!bGotCmdLine) {
            bGotCmdLine = (GetProcessImageFileNameW(processHandle, wexe_path, MAX_PATH) != 0);
            if (bGotCmdLine)
                wchar_to_utf8_no_alloc(wexe_path, cmdline, sizeof(cmdline));
        }

        // Complete failure => Just craft a default process name that includes the PID
        if (!bGotCmdLine) {
            safe_sprintf(cmdline, "Unknown_Process_0x%llx", (ULONGLONG)handleInfo->UniqueProcessId);
        }
    }

out:
    if (cmdline[0] != 0)
        Log("You should close these applications before attempting to reformat the drive.");
    else
        Log("NOTE: Could not identify the process(es) or service(s) accessing %S", _wHandleName);

	if (buffer)
		PhFree(buffer);

	if (handles)
		PhFree(handles);

    PhDestroyHeap();

    return 0;
}

