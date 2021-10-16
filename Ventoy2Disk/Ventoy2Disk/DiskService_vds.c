/******************************************************************************
 * DiskService_vds.c
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 * Copyright (c) 2011-2020, Pete Batard <pete@akeo.ie>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#include <Windows.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include <vds.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"


#define INTF_ADVANCEDDISK  1
#define INTF_ADVANCEDDISK2  2
#define INTF_CREATEPARTITIONEX  3

/* 
 * Some code and functions in the file are copied from rufus.
 * https://github.com/pbatard/rufus
 */
#define VDS_SET_ERROR SetLastError
#define IVdsServiceLoader_LoadService(This, pwszMachineName, ppService) (This)->lpVtbl->LoadService(This, pwszMachineName, ppService)
#define IVdsServiceLoader_Release(This) (This)->lpVtbl->Release(This)
#define IVdsService_QueryProviders(This, masks, ppEnum) (This)->lpVtbl->QueryProviders(This, masks, ppEnum)
#define IVdsService_WaitForServiceReady(This) ((This)->lpVtbl->WaitForServiceReady(This))
#define IVdsService_CleanupObsoleteMountPoints(This) ((This)->lpVtbl->CleanupObsoleteMountPoints(This))
#define IVdsService_Refresh(This) ((This)->lpVtbl->Refresh(This))
#define IVdsService_Reenumerate(This) ((This)->lpVtbl->Reenumerate(This)) 
#define IVdsSwProvider_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define IVdsProvider_Release(This) (This)->lpVtbl->Release(This)
#define IVdsSwProvider_QueryPacks(This, ppEnum) (This)->lpVtbl->QueryPacks(This, ppEnum)
#define IVdsSwProvider_Release(This) (This)->lpVtbl->Release(This)
#define IVdsPack_QueryDisks(This, ppEnum) (This)->lpVtbl->QueryDisks(This, ppEnum)
#define IVdsDisk_GetProperties(This, pDiskProperties) (This)->lpVtbl->GetProperties(This, pDiskProperties)
#define IVdsDisk_Release(This) (This)->lpVtbl->Release(This)
#define IVdsDisk_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define IVdsAdvancedDisk_QueryPartitions(This, ppPartitionPropArray, plNumberOfPartitions) (This)->lpVtbl->QueryPartitions(This, ppPartitionPropArray, plNumberOfPartitions)
#define IVdsAdvancedDisk_DeletePartition(This, ullOffset, bForce, bForceProtected) (This)->lpVtbl->DeletePartition(This, ullOffset, bForce, bForceProtected)
#define IVdsAdvancedDisk_ChangeAttributes(This, ullOffset, para) (This)->lpVtbl->ChangeAttributes(This, ullOffset, para)
#define IVdsAdvancedDisk_CreatePartition(This, ullOffset, ullSize, para, ppAsync) (This)->lpVtbl->CreatePartition(This, ullOffset, ullSize, para, ppAsync)
#define IVdsAdvancedDisk_Clean(This, bForce, bForceOEM, bFullClean, ppAsync) (This)->lpVtbl->Clean(This, bForce, bForceOEM, bFullClean, ppAsync)
#define IVdsAdvancedDisk_Release(This) (This)->lpVtbl->Release(This)

#define IVdsAdvancedDisk2_ChangePartitionType(This, ullOffset, bForce, para) (This)->lpVtbl->ChangePartitionType(This, ullOffset, bForce, para)
#define IVdsAdvancedDisk2_Release(This) (This)->lpVtbl->Release(This)

#define IVdsCreatePartitionEx_CreatePartitionEx(This, ullOffset, ullSize, ulAlign, para, ppAsync) (This)->lpVtbl->CreatePartitionEx(This, ullOffset, ullSize, ulAlign, para, ppAsync)
#define IVdsCreatePartitionEx_Release(This) (This)->lpVtbl->Release(This)
#define IEnumVdsObject_Next(This, celt, ppObjectArray, pcFetched) (This)->lpVtbl->Next(This, celt, ppObjectArray, pcFetched)
#define IVdsPack_QueryVolumes(This, ppEnum) (This)->lpVtbl->QueryVolumes(This, ppEnum)
#define IVdsVolume_QueryInterface(This, riid, ppvObject) (This)->lpVtbl->QueryInterface(This, riid, ppvObject)
#define IVdsVolume_Release(This) (This)->lpVtbl->Release(This)
#define IVdsVolumeMF3_QueryVolumeGuidPathnames(This, pwszPathArray, pulNumberOfPaths) (This)->lpVtbl->QueryVolumeGuidPathnames(This,pwszPathArray,pulNumberOfPaths)
#define IVdsVolumeMF3_FormatEx2(This, pwszFileSystemTypeName, usFileSystemRevision, ulDesiredUnitAllocationSize, pwszLabel, Options, ppAsync) (This)->lpVtbl->FormatEx2(This, pwszFileSystemTypeName, usFileSystemRevision, ulDesiredUnitAllocationSize, pwszLabel, Options, ppAsync)
#define IVdsVolumeMF3_Release(This) (This)->lpVtbl->Release(This)
#define IVdsVolume_GetProperties(This, pVolumeProperties) (This)->lpVtbl->GetProperties(This,pVolumeProperties)
#define IVdsAsync_Cancel(This) (This)->lpVtbl->Cancel(This)
#define IVdsAsync_QueryStatus(This,pHrResult,pulPercentCompleted) (This)->lpVtbl->QueryStatus(This,pHrResult,pulPercentCompleted)
#define IVdsAsync_Wait(This,pHrResult,pAsyncOut) (This)->lpVtbl->Wait(This,pHrResult,pAsyncOut)
#define IVdsAsync_Release(This) (This)->lpVtbl->Release(This)

#define IUnknown_QueryInterface(This, a, b) (This)->lpVtbl->QueryInterface(This,a,b)
#define IUnknown_Release(This) (This)->lpVtbl->Release(This)

typedef BOOL(*VDS_Callback_PF)(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data);

STATIC IVdsService * VDS_InitService(void)
{
    HRESULT hr;
    IVdsServiceLoader *pLoader;
    IVdsService *pService;

    // Initialize COM
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0, NULL);

    // Create a VDS Loader Instance
    hr = CoCreateInstance(&CLSID_VdsLoader, NULL, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, &IID_IVdsServiceLoader, (void **)&pLoader);
    if (hr != S_OK) 
    {
        VDS_SET_ERROR(hr);
        Log("Could not create VDS Loader Instance: %u", LASTERR);
        return NULL;
    }

    // Load the VDS Service
    hr = IVdsServiceLoader_LoadService(pLoader, L"", &pService);
    IVdsServiceLoader_Release(pLoader);
    if (hr != S_OK) 
    {
        VDS_SET_ERROR(hr);
        Log("Could not load VDS Service: %u", LASTERR);
        return NULL;
    }

    // Wait for the Service to become ready if needed
    hr = IVdsService_WaitForServiceReady(pService);
    if (hr != S_OK) 
    {
        VDS_SET_ERROR(hr);
        Log("VDS Service is not ready: %u", LASTERR);
        return NULL;
    }

    Log("VDS init OK, service %p", pService);
    return pService;
}


STATIC BOOL VDS_DiskCommProc(int intf, int DriveIndex, VDS_Callback_PF callback, UINT64 data)
{
    BOOL r = FALSE;
    HRESULT hr;
    ULONG ulFetched;
    IUnknown *pUnk = NULL;
    IEnumVdsObject *pEnum = NULL;    
    IVdsService *pService = NULL;
    wchar_t wPhysicalName[48];

    swprintf_s(wPhysicalName, ARRAYSIZE(wPhysicalName), L"\\\\?\\PhysicalDrive%d", DriveIndex);

    pService = VDS_InitService();
    if (!pService)
    {
        Log("Could not query VDS Service");
        goto out;
    }

    // Query the VDS Service Providers
    hr = IVdsService_QueryProviders(pService, VDS_QUERY_SOFTWARE_PROVIDERS, &pEnum);
    if (hr != S_OK) 
    {
        VDS_SET_ERROR(hr);
        Log("Could not query VDS Service Providers: 0x%lx %u", hr, LASTERR);
        goto out;
    }

    while (IEnumVdsObject_Next(pEnum, 1, &pUnk, &ulFetched) == S_OK) 
    {
        IVdsProvider *pProvider;
        IVdsSwProvider *pSwProvider;
        IEnumVdsObject *pEnumPack;
        IUnknown *pPackUnk;

        // Get VDS Provider
        hr = IUnknown_QueryInterface(pUnk, &IID_IVdsProvider, (void **)&pProvider);
        IUnknown_Release(pUnk);
        if (hr != S_OK) 
        {
            VDS_SET_ERROR(hr);
            Log("Could not get VDS Provider: %u", LASTERR);
            goto out;
        }

        // Get VDS Software Provider
        hr = IVdsSwProvider_QueryInterface(pProvider, &IID_IVdsSwProvider, (void **)&pSwProvider);
        IVdsProvider_Release(pProvider);
        if (hr != S_OK) 
        {
            VDS_SET_ERROR(hr);
            Log("Could not get VDS Software Provider: %u", LASTERR);
            goto out;
        }

        // Get VDS Software Provider Packs
        hr = IVdsSwProvider_QueryPacks(pSwProvider, &pEnumPack);
        IVdsSwProvider_Release(pSwProvider);
        if (hr != S_OK) 
        {
            VDS_SET_ERROR(hr);
            Log("Could not get VDS Software Provider Packs: %u", LASTERR);
            goto out;
        }

        // Enumerate Provider Packs
        while (IEnumVdsObject_Next(pEnumPack, 1, &pPackUnk, &ulFetched) == S_OK) 
        {
            IVdsPack *pPack;
            IEnumVdsObject *pEnumDisk;
            IUnknown *pDiskUnk;

            hr = IUnknown_QueryInterface(pPackUnk, &IID_IVdsPack, (void **)&pPack);
            IUnknown_Release(pPackUnk);
            if (hr != S_OK) 
            {
                VDS_SET_ERROR(hr);
                Log("Could not query VDS Software Provider Pack: %u", LASTERR);
                goto out;
            }

            // Use the pack interface to access the disks
            hr = IVdsPack_QueryDisks(pPack, &pEnumDisk);
            if (hr != S_OK) {
                VDS_SET_ERROR(hr);
                Log("Could not query VDS disks: %u", LASTERR);
                goto out;
            }

            // List disks
            while (IEnumVdsObject_Next(pEnumDisk, 1, &pDiskUnk, &ulFetched) == S_OK) 
            {
                VDS_DISK_PROP diskprop;
                IVdsDisk *pDisk;
                IVdsAdvancedDisk *pAdvancedDisk;
				IVdsAdvancedDisk2 *pAdvancedDisk2;
				IVdsCreatePartitionEx *pCreatePartitionEx;

                // Get the disk interface.
                hr = IUnknown_QueryInterface(pDiskUnk, &IID_IVdsDisk, (void **)&pDisk);
                if (hr != S_OK) {
                    VDS_SET_ERROR(hr);
                    Log("Could not query VDS Disk Interface: %u", LASTERR);
                    goto out;
                }

                // Get the disk properties
                hr = IVdsDisk_GetProperties(pDisk, &diskprop);
                if (hr != S_OK) {
                    VDS_SET_ERROR(hr);
                    Log("Could not query VDS Disk Properties: %u", LASTERR);
                    goto out;
                }

                // Isolate the disk we want
                if (_wcsicmp(wPhysicalName, diskprop.pwszName) != 0) 
                {
                    IVdsDisk_Release(pDisk);
                    continue;
                }

				if (intf == INTF_ADVANCEDDISK)
				{
					// Instantiate the AdvanceDisk interface for our disk.
					hr = IVdsDisk_QueryInterface(pDisk, &IID_IVdsAdvancedDisk, (void **)&pAdvancedDisk);
					IVdsDisk_Release(pDisk);
					if (hr != S_OK)
					{
						VDS_SET_ERROR(hr);
						Log("Could not access VDS Advanced Disk interface: %u", LASTERR);
						goto out;
					}
					else
					{
						Log("Callback %d process for disk <%S>", intf, diskprop.pwszName);
						r = callback(pAdvancedDisk, &diskprop, data);
					}
					IVdsAdvancedDisk_Release(pAdvancedDisk);
				}
				else if (intf == INTF_ADVANCEDDISK2)
				{
					// Instantiate the AdvanceDisk interface for our disk.
					hr = IVdsDisk_QueryInterface(pDisk, &IID_IVdsAdvancedDisk2, (void **)&pAdvancedDisk2);
					IVdsDisk_Release(pDisk);
					if (hr != S_OK)
					{
						VDS_SET_ERROR(hr);
						Log("Could not access VDS Advanced Disk2 interface: %u", LASTERR);
						goto out;
					}
					else
					{
						Log("Callback %d process for disk2 <%S>", intf, diskprop.pwszName);
						r = callback(pAdvancedDisk2, &diskprop, data);
					}
					IVdsAdvancedDisk2_Release(pAdvancedDisk2);
				}
				else if (intf == INTF_CREATEPARTITIONEX)
				{
					// Instantiate the CreatePartitionEx interface for our disk.
					hr = IVdsDisk_QueryInterface(pDisk, &IID_IVdsCreatePartitionEx, (void **)&pCreatePartitionEx);
					IVdsDisk_Release(pDisk);
					if (hr != S_OK)
					{
						VDS_SET_ERROR(hr);
						Log("Could not access VDS CreatePartitionEx interface: %u", LASTERR);
						goto out;
					}
					else
					{
						Log("Callback %d process for disk <%S>", intf, diskprop.pwszName);
						r = callback(pCreatePartitionEx, &diskprop, data);
					}
					IVdsCreatePartitionEx_Release(pCreatePartitionEx);
				}

                goto out;
            }
        }
    }

out:
    return r;
}

STATIC BOOL VDS_CallBack_CleanDisk(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data)
{    
    HRESULT hr, hr2;
    ULONG completed;
    IVdsAsync* pAsync;
	IVdsAdvancedDisk *pAdvancedDisk = (IVdsAdvancedDisk *)pInterface;

    (void)pDiskProp;
    (void)data;

    hr = IVdsAdvancedDisk_Clean(pAdvancedDisk, TRUE, TRUE, FALSE, &pAsync);
    while (SUCCEEDED(hr)) 
    {
        hr = IVdsAsync_QueryStatus(pAsync, &hr2, &completed);
        if (SUCCEEDED(hr)) 
        {
            hr = hr2;
            if (hr == S_OK)
            {
                Log("Disk clean QueryStatus OK");
                break;
            }
            else if (hr == VDS_E_OPERATION_PENDING)
            {
                hr = S_OK;
            }
            else
            {
                Log("QueryStatus invalid status:%u", hr);
            }
        }
        Sleep(500);
    }

    if (hr != S_OK) 
    {
        VDS_SET_ERROR(hr);
        Log("Could not clean disk 0x%lx err:%u", hr, LASTERR);
        return FALSE;
    }

    return TRUE;
}

BOOL VDS_CleanDisk(int DriveIndex)
{
	BOOL ret = VDS_DiskCommProc(INTF_ADVANCEDDISK, DriveIndex, VDS_CallBack_CleanDisk, 0);
    Log("VDS_CleanDisk %d ret:%d", DriveIndex, ret);
    return ret;
}

STATIC BOOL VDS_CallBack_DeletePartition(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data)
{
    BOOL r = FALSE;
    HRESULT hr;
    VDS_PARTITION_PROP* prop_array = NULL;
    LONG i, prop_array_size;
    ULONG PartNumber = (ULONG)data;
	IVdsAdvancedDisk *pAdvancedDisk = (IVdsAdvancedDisk *)pInterface;

    if (PartNumber == 0)
    {
        Log("Deleting ALL partitions from disk '%S':", pDiskProp->pwszName);
    }
    else
    {
		Log("Deleting partition(%ld) from disk '%S':", PartNumber, pDiskProp->pwszName);
    }

    // Query the partition data, so we can get the start offset, which we need for deletion
    hr = IVdsAdvancedDisk_QueryPartitions(pAdvancedDisk, &prop_array, &prop_array_size);
    if (hr == S_OK) 
    {
        for (i = 0; i < prop_array_size; i++) 
        {
            if (PartNumber == 0 || PartNumber == prop_array[i].ulPartitionNumber)
            {
                Log("* Partition %d (offset: %lld, size: %llu) delete it.",
                    prop_array[i].ulPartitionNumber, prop_array[i].ullOffset, (ULONGLONG)prop_array[i].ullSize);
            }
            else
            {
                Log("  Partition %d (offset: %lld, size: %llu) skip it.",
                    prop_array[i].ulPartitionNumber, prop_array[i].ullOffset, (ULONGLONG)prop_array[i].ullSize);
                continue;
            }

            hr = IVdsAdvancedDisk_DeletePartition(pAdvancedDisk, prop_array[i].ullOffset, TRUE, TRUE);
            if (hr != S_OK) 
            {
                r = FALSE;
                VDS_SET_ERROR(hr);
                Log("Could not delete partitions: %u", LASTERR);
            }
            else 
            {
                Log("Delete this partitions success");
            }
        }
        r = TRUE;
    }
    else 
    {
        Log("No partition to delete on disk '%S'", pDiskProp->pwszName);
        r = TRUE;
    }
    CoTaskMemFree(prop_array);

    return r;
}

BOOL VDS_DeleteAllPartitions(int DriveIndex)
{
	BOOL ret = VDS_DiskCommProc(INTF_ADVANCEDDISK, DriveIndex, VDS_CallBack_DeletePartition, 0);
    Log("VDS_DeleteAllPartitions %d ret:%d", DriveIndex, ret);
    return ret;
}

BOOL VDS_DeleteVtoyEFIPartition(int DriveIndex)
{
	BOOL ret = VDS_DiskCommProc(INTF_ADVANCEDDISK, DriveIndex, VDS_CallBack_DeletePartition, 2);
    Log("VDS_DeleteVtoyEFIPartition %d ret:%d", DriveIndex, ret);
    return ret;
}

STATIC BOOL VDS_CallBack_ChangeEFIAttr(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data)
{
    BOOL r = FALSE;
    HRESULT hr;
    VDS_PARTITION_PROP* prop_array = NULL;
    LONG i, prop_array_size;
    CHANGE_ATTRIBUTES_PARAMETERS AttrPara;
	IVdsAdvancedDisk *pAdvancedDisk = (IVdsAdvancedDisk *)pInterface;

    // Query the partition data, so we can get the start offset, which we need for deletion
    hr = IVdsAdvancedDisk_QueryPartitions(pAdvancedDisk, &prop_array, &prop_array_size);
    if (hr == S_OK)
    {
        for (i = 0; i < prop_array_size; i++)
        {
            if (prop_array[i].ullSize == VENTOY_EFI_PART_SIZE &&
                prop_array[i].PartitionStyle == VDS_PST_GPT &&
                memcmp(prop_array[i].Gpt.name, L"VTOYEFI", 7 * 2) == 0)
            {
                Log("* Partition %d (offset: %lld, size: %llu, Attr:0x%llx)", prop_array[i].ulPartitionNumber,
                    prop_array[i].ullOffset, (ULONGLONG)prop_array[i].ullSize, prop_array[i].Gpt.attributes);

                if (prop_array[i].Gpt.attributes == data)
                {
                    Log("Attribute match, No need to change.");
                    r = TRUE;
                }
                else
                {
                    AttrPara.style = VDS_PST_GPT;
                    AttrPara.GptPartInfo.attributes = data;
                    hr = IVdsAdvancedDisk_ChangeAttributes(pAdvancedDisk, prop_array[i].ullOffset, &AttrPara);
                    if (hr == S_OK)
                    {
                        r = TRUE;
                        Log("Change this partitions attribute success");
                    }
                    else
                    {
                        r = FALSE;
                        VDS_SET_ERROR(hr);
                        Log("Could not change partitions attr: %u", LASTERR);
                    }
                }
                break;
            }
        }
    }
    else
    {
        Log("No partition found on disk '%S'", pDiskProp->pwszName);
    }
    CoTaskMemFree(prop_array);

    return r;
}

BOOL VDS_ChangeVtoyEFIAttr(int DriveIndex, UINT64 Attr)
{
	BOOL ret = VDS_DiskCommProc(INTF_ADVANCEDDISK, DriveIndex, VDS_CallBack_ChangeEFIAttr, Attr);
    Log("VDS_ChangeVtoyEFIAttr %d ret:%d", DriveIndex, ret);
    return ret;
}



STATIC BOOL VDS_CallBack_ChangeEFIType(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data)
{
	BOOL r = FALSE;
	HRESULT hr;
	IVdsAdvancedDisk2 *pAdvancedDisk2 = (IVdsAdvancedDisk2 *)pInterface;
	VDS_PARA *VdsPara = (VDS_PARA *)(ULONG)data;
	CHANGE_PARTITION_TYPE_PARAMETERS para;

	para.style = VDS_PST_GPT;
	memcpy(&(para.GptPartInfo.partitionType), &VdsPara->Type, sizeof(GUID));

	hr = IVdsAdvancedDisk2_ChangePartitionType(pAdvancedDisk2, VdsPara->Offset, TRUE, &para);
	if (hr == S_OK)
	{
		r = TRUE;
	}
	else
	{
		Log("Failed to change partition type 0x%lx", hr);
	}

	return r;
}


BOOL VDS_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset)
{
	VDS_PARA Para;	
	GUID EspPartType = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };

	memcpy(&(Para.Type), &EspPartType, sizeof(GUID));
	Para.Offset = Offset;

	BOOL ret = VDS_DiskCommProc(INTF_ADVANCEDDISK2, DriveIndex, VDS_CallBack_ChangeEFIType, (ULONG)&Para);
	Log("VDS_ChangeVtoyEFI2ESP %d ret:%d", DriveIndex, ret);
	return ret;
}

BOOL VDS_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset)
{
	VDS_PARA Para;
	GUID WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };

	memcpy(&(Para.Type), &WindowsDataPartType, sizeof(GUID));
	Para.Offset = Offset;

	BOOL ret = VDS_DiskCommProc(INTF_ADVANCEDDISK2, DriveIndex, VDS_CallBack_ChangeEFIType, (ULONG)&Para);
	Log("VDS_ChangeVtoyEFI2ESP %d ret:%d", DriveIndex, ret);
	return ret;
}


STATIC BOOL VDS_CallBack_CreateVtoyEFI(void *pInterface, VDS_DISK_PROP *pDiskProp, UINT64 data)
{
    HRESULT hr, hr2;
    ULONG completed;
    IVdsAsync* pAsync;
	CREATE_PARTITION_PARAMETERS para;
	IVdsCreatePartitionEx *pCreatePartitionEx = (IVdsCreatePartitionEx *)pInterface;
    VDS_PARA *VdsPara = (VDS_PARA *)(ULONG)data;

    (void)pDiskProp;

    memset(&para, 0, sizeof(para));
    para.style = VDS_PST_GPT;
    memcpy(&(para.GptPartInfo.partitionType), &VdsPara->Type, sizeof(GUID));
    memcpy(&(para.GptPartInfo.partitionId), &VdsPara->Id, sizeof(GUID));
	para.GptPartInfo.attributes = VdsPara->Attr;
	memcpy(para.GptPartInfo.name, VdsPara->Name, sizeof(WCHAR)* VdsPara->NameLen);

	hr = IVdsCreatePartitionEx_CreatePartitionEx(pCreatePartitionEx, VdsPara->Offset, VENTOY_EFI_PART_SIZE, 512, &para, &pAsync);
    while (SUCCEEDED(hr))
    {
        hr = IVdsAsync_QueryStatus(pAsync, &hr2, &completed);
        if (SUCCEEDED(hr))
        {
            hr = hr2;
            if (hr == S_OK)
            {
				Log("Disk create partition QueryStatus OK, %lu%%", completed);
                break;
            }
            else if (hr == VDS_E_OPERATION_PENDING)
            {
				Log("Disk partition finish: %lu%%", completed);
                hr = S_OK;
            }
            else
            {
                Log("QueryStatus invalid status:0x%lx", hr);
            }
        }
        Sleep(1000);
    }

    if (hr != S_OK)
    {
        VDS_SET_ERROR(hr);
        Log("Could not create partition, err:0x%lx", LASTERR);
        return FALSE;
    }

    return TRUE;
}

BOOL VDS_CreateVtoyEFIPart(int DriveIndex, UINT64 Offset)
{
    VDS_PARA Para;
    GUID WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };
	GUID EspPartType = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };

	Log("VDS_CreateVtoyEFIPart %u Offset:%llu Sector:%llu", DriveIndex, Offset, Offset / 512);

    memset(&Para, 0, sizeof(Para));
    Para.Attr = 0x8000000000000000ULL;
    Para.Offset = Offset;
    memcpy(Para.Name, L"VTOYEFI", 7 * 2);
	Para.NameLen = 7;
	memcpy(&(Para.Type), &EspPartType, sizeof(GUID));
    CoCreateGuid(&(Para.Id));

	BOOL ret = VDS_DiskCommProc(INTF_CREATEPARTITIONEX, DriveIndex, VDS_CallBack_CreateVtoyEFI, (ULONG)&Para);
    Log("VDS_CreateVtoyEFIPart %d ret:%d", DriveIndex, ret);
    return ret; 
}

