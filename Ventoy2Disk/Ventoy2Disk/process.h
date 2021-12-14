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

#include <windows.h>
#include <winnt.h>
#include <winternl.h>

#pragma once

#define PH_LARGE_BUFFER_SIZE			(256 * 1024 * 1024)

#define STATUS_SUCCESS					((NTSTATUS)0x00000000L)
#define STATUS_ALREADY_COMPLETE			((NTSTATUS)0x000000FFL)
#define STATUS_UNSUCCESSFUL				((NTSTATUS)0x80000001L)
#define STATUS_BUFFER_OVERFLOW			((NTSTATUS)0x80000005L)
#define STATUS_NOT_IMPLEMENTED			((NTSTATUS)0xC0000002L)
#define STATUS_INFO_LENGTH_MISMATCH		((NTSTATUS)0xC0000004L)
//#define STATUS_INVALID_HANDLE			((NTSTATUS)0xC0000008L)
#define STATUS_ACCESS_DENIED			((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL			((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_TYPE_MISMATCH		((NTSTATUS)0xC0000024L)
#define STATUS_OBJECT_NAME_INVALID		((NTSTATUS)0xC0000033L)
#define STATUS_OBJECT_NAME_NOT_FOUND	((NTSTATUS)0xC0000034L)
#define STATUS_OBJECT_PATH_INVALID		((NTSTATUS)0xC0000039L)
#define STATUS_SHARING_VIOLATION		((NTSTATUS)0xC0000043L)
#define STATUS_INSUFFICIENT_RESOURCES	((NTSTATUS)0xC000009AL)
#define STATUS_NOT_SUPPORTED			((NTSTATUS)0xC00000BBL)

#define SystemExtendedHandleInformation    64
#define FileProcessIdsUsingFileInformation 47

// MinGW doesn't know this one yet
#if !defined(PROCESSOR_ARCHITECTURE_ARM64)
#define PROCESSOR_ARCHITECTURE_ARM64       12
#endif

#define NtCurrentProcess() ((HANDLE)(LONG_PTR)-1)

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
    PVOID Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX
{
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

#if defined(_MSC_VER)
typedef struct _OBJECT_NAME_INFORMATION
{
    UNICODE_STRING Name;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef struct _OBJECT_TYPE_INFORMATION
{
    UNICODE_STRING TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccessMask;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    UCHAR TypeIndex; // since WINBLUE
    CHAR ReservedByte;
    ULONG PoolType;
    ULONG DefaultPagedPoolCharge;
    ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

#define ObjectNameInformation  1
#endif
#define ObjectTypesInformation 3

typedef struct _OBJECT_TYPES_INFORMATION
{
    ULONG NumberOfTypes;
} OBJECT_TYPES_INFORMATION, *POBJECT_TYPES_INFORMATION;

typedef struct _PROCESS_BASIC_INFORMATION_WOW64
{
    PVOID Reserved1[2];
    // MinGW32 screws us with a sizeof(PVOID64) of 4 instead of 8 => Use ULONGLONG instead
    ULONGLONG PebBaseAddress;
    PVOID Reserved2[4];
    ULONG_PTR UniqueProcessId[2];
    PVOID Reserved3[2];
} PROCESS_BASIC_INFORMATION_WOW64;

typedef struct _UNICODE_STRING_WOW64
{
    USHORT Length;
    USHORT MaximumLength;
    ULONGLONG Buffer;
} UNICODE_STRING_WOW64;

typedef struct _FILE_PROCESS_IDS_USING_FILE_INFORMATION
{
    ULONG NumberOfProcessIdsInList;
    ULONG_PTR ProcessIdList[1];
} FILE_PROCESS_IDS_USING_FILE_INFORMATION, *PFILE_PROCESS_IDS_USING_FILE_INFORMATION;

#define ALIGN_UP_BY(Address, Align) (((ULONG_PTR)(Address) + (Align) - 1) & ~((Align) - 1))
#define ALIGN_UP(Address, Type) ALIGN_UP_BY(Address, sizeof(Type))

#define PH_FIRST_OBJECT_TYPE(ObjectTypes) \
    (POBJECT_TYPE_INFORMATION)((PCHAR)(ObjectTypes)+ALIGN_UP(sizeof(OBJECT_TYPES_INFORMATION), ULONG_PTR))

#define PH_NEXT_OBJECT_TYPE(ObjectType) \
    (POBJECT_TYPE_INFORMATION)((PCHAR)(ObjectType)+sizeof(OBJECT_TYPE_INFORMATION)+\
    ALIGN_UP(ObjectType->TypeName.MaximumLength, ULONG_PTR))

// Heaps

typedef struct _RTL_HEAP_ENTRY
{
    SIZE_T Size;
    USHORT Flags;
    USHORT AllocatorBackTraceIndex;
    union
    {
        struct
        {
            SIZE_T Settable;
            ULONG Tag;
        } s1;
        struct
        {
            SIZE_T CommittedSize;
            PVOID FirstBlock;
        } s2;
    } u;
} RTL_HEAP_ENTRY, *PRTL_HEAP_ENTRY;

#define RTL_HEAP_BUSY (USHORT)0x0001
#define RTL_HEAP_SEGMENT (USHORT)0x0002
#define RTL_HEAP_SETTABLE_VALUE (USHORT)0x0010
#define RTL_HEAP_SETTABLE_FLAG1 (USHORT)0x0020
#define RTL_HEAP_SETTABLE_FLAG2 (USHORT)0x0040
#define RTL_HEAP_SETTABLE_FLAG3 (USHORT)0x0080
#define RTL_HEAP_SETTABLE_FLAGS (USHORT)0x00e0
#define RTL_HEAP_UNCOMMITTED_RANGE (USHORT)0x0100
#define RTL_HEAP_PROTECTED_ENTRY (USHORT)0x0200

typedef struct _RTL_HEAP_TAG
{
    ULONG NumberOfAllocations;
    ULONG NumberOfFrees;
    SIZE_T BytesAllocated;
    USHORT TagIndex;
    USHORT CreatorBackTraceIndex;
    WCHAR TagName[24];
} RTL_HEAP_TAG, *PRTL_HEAP_TAG;

typedef struct _RTL_HEAP_INFORMATION
{
    PVOID BaseAddress;
    ULONG Flags;
    USHORT EntryOverhead;
    USHORT CreatorBackTraceIndex;
    SIZE_T BytesAllocated;
    SIZE_T BytesCommitted;
    ULONG NumberOfTags;
    ULONG NumberOfEntries;
    ULONG NumberOfPseudoTags;
    ULONG PseudoTagGranularity;
    ULONG Reserved[5];
    PRTL_HEAP_TAG Tags;
    PRTL_HEAP_ENTRY Entries;
} RTL_HEAP_INFORMATION, *PRTL_HEAP_INFORMATION;

typedef struct _RTL_PROCESS_HEAPS
{
    ULONG NumberOfHeaps;
    RTL_HEAP_INFORMATION Heaps[1];
} RTL_PROCESS_HEAPS, *PRTL_PROCESS_HEAPS;

typedef NTSTATUS(NTAPI *PRTL_HEAP_COMMIT_ROUTINE)(
    _In_ PVOID Base,
    _Inout_ PVOID *CommitAddress,
    _Inout_ PSIZE_T CommitSize
    );

#if defined(_MSC_VER)
typedef struct _RTL_HEAP_PARAMETERS
{
    ULONG Length;
    SIZE_T SegmentReserve;
    SIZE_T SegmentCommit;
    SIZE_T DeCommitFreeBlockThreshold;
    SIZE_T DeCommitTotalFreeThreshold;
    SIZE_T MaximumAllocationSize;
    SIZE_T VirtualMemoryThreshold;
    SIZE_T InitialCommit;
    SIZE_T InitialReserve;
    PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
    SIZE_T Reserved[2];
} RTL_HEAP_PARAMETERS, *PRTL_HEAP_PARAMETERS;
#endif

#define HEAP_SETTABLE_USER_VALUE 0x00000100
#define HEAP_SETTABLE_USER_FLAG1 0x00000200
#define HEAP_SETTABLE_USER_FLAG2 0x00000400
#define HEAP_SETTABLE_USER_FLAG3 0x00000800
#define HEAP_SETTABLE_USER_FLAGS 0x00000e00

#define HEAP_CLASS_0 0x00000000 // Process heap
#define HEAP_CLASS_1 0x00001000 // Private heap
#define HEAP_CLASS_2 0x00002000 // Kernel heap
#define HEAP_CLASS_3 0x00003000 // GDI heap
#define HEAP_CLASS_4 0x00004000 // User heap
#define HEAP_CLASS_5 0x00005000 // Console heap
#define HEAP_CLASS_6 0x00006000 // User desktop heap
#define HEAP_CLASS_7 0x00007000 // CSR shared heap
#define HEAP_CLASS_8 0x00008000 // CSR port heap
#define HEAP_CLASS_MASK 0x0000f000

// Privileges

#define SE_MIN_WELL_KNOWN_PRIVILEGE (2L)
#define SE_CREATE_TOKEN_PRIVILEGE (2L)
#define SE_ASSIGNPRIMARYTOKEN_PRIVILEGE (3L)
#define SE_LOCK_MEMORY_PRIVILEGE (4L)
#define SE_INCREASE_QUOTA_PRIVILEGE (5L)
#define SE_MACHINE_ACCOUNT_PRIVILEGE (6L)
#define SE_TCB_PRIVILEGE (7L)
#define SE_SECURITY_PRIVILEGE (8L)
#define SE_TAKE_OWNERSHIP_PRIVILEGE (9L)
#define SE_LOAD_DRIVER_PRIVILEGE (10L)
#define SE_SYSTEM_PROFILE_PRIVILEGE (11L)
#define SE_SYSTEMTIME_PRIVILEGE (12L)
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE (13L)
#define SE_INC_BASE_PRIORITY_PRIVILEGE (14L)
#define SE_CREATE_PAGEFILE_PRIVILEGE (15L)
#define SE_CREATE_PERMANENT_PRIVILEGE (16L)
#define SE_BACKUP_PRIVILEGE (17L)
#define SE_RESTORE_PRIVILEGE (18L)
#define SE_SHUTDOWN_PRIVILEGE (19L)
#define SE_DEBUG_PRIVILEGE (20L)
#define SE_AUDIT_PRIVILEGE (21L)
#define SE_SYSTEM_ENVIRONMENT_PRIVILEGE (22L)
#define SE_CHANGE_NOTIFY_PRIVILEGE (23L)
#define SE_REMOTE_SHUTDOWN_PRIVILEGE (24L)
#define SE_UNDOCK_PRIVILEGE (25L)
#define SE_SYNC_AGENT_PRIVILEGE (26L)
#define SE_ENABLE_DELEGATION_PRIVILEGE (27L)
#define SE_MANAGE_VOLUME_PRIVILEGE (28L)
#define SE_IMPERSONATE_PRIVILEGE (29L)
#define SE_CREATE_GLOBAL_PRIVILEGE (30L)
#define SE_TRUSTED_CREDMAN_ACCESS_PRIVILEGE (31L)
#define SE_RELABEL_PRIVILEGE (32L)
#define SE_INC_WORKING_SET_PRIVILEGE (33L)
#define SE_TIME_ZONE_PRIVILEGE (34L)
#define SE_CREATE_SYMBOLIC_LINK_PRIVILEGE (35L)
#define SE_MAX_WELL_KNOWN_PRIVILEGE SE_CREATE_SYMBOLIC_LINK_PRIVILEGE



/*
* typedefs for the function prototypes. Use the something like:
*   PF_DECL(FormatEx);
* which translates to:
*   FormatEx_t pfFormatEx = NULL;
* in your code, to declare the entrypoint and then use:
*   PF_INIT(FormatEx, Fmifs);
* which translates to:
*   pfFormatEx = (FormatEx_t) GetProcAddress(GetDLLHandle("fmifs"), "FormatEx");
* to make it accessible.
*/
#define         MAX_LIBRARY_HANDLES 32
extern HMODULE  OpenedLibrariesHandle[MAX_LIBRARY_HANDLES];
extern UINT16   OpenedLibrariesHandleSize;
#define         OPENED_LIBRARIES_VARS HMODULE OpenedLibrariesHandle[MAX_LIBRARY_HANDLES]; UINT16 OpenedLibrariesHandleSize = 0
#define         CLOSE_OPENED_LIBRARIES while(OpenedLibrariesHandleSize > 0) FreeLibrary(OpenedLibrariesHandle[--OpenedLibrariesHandleSize])
static __inline HMODULE GetLibraryHandle(char* szLibraryName) {
    HMODULE h = NULL;
    if ((h = GetModuleHandleA(szLibraryName)) == NULL) {
        if (OpenedLibrariesHandleSize >= MAX_LIBRARY_HANDLES) {
            Log("Error: MAX_LIBRARY_HANDLES is too small\n");
        }
        else {
            h = LoadLibraryA(szLibraryName);
            if (h != NULL)
                OpenedLibrariesHandle[OpenedLibrariesHandleSize++] = h;
        }
    }
    return h;
}
#define PF_TYPE(api, ret, proc, args)		typedef ret (api *proc##_t)args
#define PF_DECL(proc)						static proc##_t pf##proc = NULL
#define PF_TYPE_DECL(api, ret, proc, args)	PF_TYPE(api, ret, proc, args); PF_DECL(proc)
#define PF_INIT(proc, name)					if (pf##proc == NULL) pf##proc = \
    (proc##_t) GetProcAddress(GetLibraryHandle(#name), #proc)
#define PF_INIT_OR_OUT(proc, name)			do {PF_INIT(proc, name);         \
if (pf##proc == NULL) { Log("Unable to locate %s() in %s.dll: %d\n", \
#proc, #name, GetLastError()); goto out;}    } while (0)

#define PF_INIT_OR_SET_STATUS(proc, name)	do {PF_INIT(proc, name);         \
if ((pf##proc == NULL) && (NT_SUCCESS(status))) status = STATUS_NOT_IMPLEMENTED; \
    } while (0)

/* Custom application errors */
#define FAC(f)                         ((f)<<16)
#define APPERR(err)                    (APPLICATION_ERROR_MASK|(err))
#define ERROR_INCOMPATIBLE_FS          0x1201
#define ERROR_CANT_QUICK_FORMAT        0x1202
#define ERROR_INVALID_CLUSTER_SIZE     0x1203
#define ERROR_INVALID_VOLUME_SIZE      0x1204
#define ERROR_CANT_START_THREAD        0x1205
#define ERROR_BADBLOCKS_FAILURE        0x1206
#define ERROR_ISO_SCAN                 0x1207
#define ERROR_ISO_EXTRACT              0x1208
#define ERROR_CANT_REMOUNT_VOLUME      0x1209
#define ERROR_CANT_PATCH               0x120A
#define ERROR_CANT_ASSIGN_LETTER       0x120B
#define ERROR_CANT_MOUNT_VOLUME        0x120C
#define ERROR_BAD_SIGNATURE            0x120D
#define ERROR_CANT_DOWNLOAD            0x120E


#define KB                   1024LL
#define MB                1048576LL
#define GB             1073741824LL
#define TB          1099511627776LL

#ifndef _WINTERNL_
typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
#endif



PF_TYPE_DECL(NTAPI, PVOID, RtlCreateHeap, (ULONG, PVOID, SIZE_T, SIZE_T, PVOID, PRTL_HEAP_PARAMETERS));
PF_TYPE_DECL(NTAPI, PVOID, RtlDestroyHeap, (PVOID));
PF_TYPE_DECL(NTAPI, PVOID, RtlAllocateHeap, (PVOID, ULONG, SIZE_T));
PF_TYPE_DECL(NTAPI, BOOLEAN, RtlFreeHeap, (PVOID, ULONG, PVOID));

PF_TYPE_DECL(NTAPI, NTSTATUS, NtQuerySystemInformation, (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtQueryInformationFile, (HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtQueryInformationProcess, (HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtWow64QueryInformationProcess64, (HANDLE, ULONG, PVOID, ULONG, PULONG));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtWow64ReadVirtualMemory64, (HANDLE, ULONGLONG, PVOID, ULONG64, PULONG64));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtQueryObject, (HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtDuplicateObject, (HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtOpenProcess, (PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, CLIENT_ID*));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtOpenProcessToken, (HANDLE, ACCESS_MASK, PHANDLE));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtAdjustPrivilegesToken, (HANDLE, BOOLEAN, PTOKEN_PRIVILEGES, ULONG, PTOKEN_PRIVILEGES, PULONG));
PF_TYPE_DECL(NTAPI, NTSTATUS, NtClose, (HANDLE));


#define safe_free(p) do {free((void*)p); p = NULL;} while(0)

#define wchar_to_utf8_no_alloc(wsrc, dest, dest_size) \
    WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, dest, dest_size, NULL, NULL)
#define utf8_to_wchar_no_alloc(src, wdest, wdest_size) \
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wdest, wdest_size)

#define sfree(p) do {if (p != NULL) {free((void*)(p)); p = NULL;}} while(0)
#define wconvert(p)     wchar_t* w ## p = utf8_to_wchar(p)
#define walloc(p, size) wchar_t* w ## p = (p == NULL)?NULL:(wchar_t*)calloc(size, sizeof(wchar_t))
#define wfree(p) sfree(w ## p)

