/* file.c - SimpleFileIo Interface */
/*
 *  Copyright © 2014-2017 Pete Batard <pete@akeo.ie>
 *  Based on iPXE's efi_driver.c and efi_file.c:
 *  Copyright © 2011,2013 Michael Brown <mbrown@fensystems.co.uk>.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "driver.h"

/**
 * Get EFI file name (for debugging)
 *
 * @v file			EFI file
 * @ret Name		Name
 */
static const CHAR16 *
FileName(EFI_GRUB_FILE *File)
{
	EFI_STATUS Status;
	static CHAR16 Path[MAX_PATH];

	Status = Utf8ToUtf16NoAlloc(File->path, Path, sizeof(Path));
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not convert filename to UTF16");
		return NULL;
	}

	return Path;
}

/* Simple hook to populate the timestamp and directory flag when opening a file */
static INT32
InfoHook(const CHAR8 *name, const GRUB_DIRHOOK_INFO *Info, VOID *Data)
{
	EFI_GRUB_FILE *File = (EFI_GRUB_FILE *) Data;

	/* Look for a specific file */
	if (strcmpa(name, File->basename) != 0)
		return 0;

	File->IsDir = (BOOLEAN) (Info->Dir);
	if (Info->MtimeSet)
		File->Mtime = Info->Mtime;

	return 0;
}

/**
 * Open file
 *
 * @v This			File handle
 * @ret new			New file handle
 * @v Name			File name
 * @v Mode			File mode
 * @v Attributes	File attributes (for newly-created files)
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileOpen(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New,
		CHAR16 *Name, UINT64 Mode, UINT64 Attributes)
{
	EFI_STATUS Status;
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	EFI_GRUB_FILE *NewFile;

	// TODO: Use dynamic buffers?
	char path[MAX_PATH], clean_path[MAX_PATH], *dirname;
	INTN i, len;
	BOOLEAN AbsolutePath = (*Name == L'\\');

	PrintInfo(L"Open(" PERCENT_P L"%s, \"%s\")\n", (UINTN) This,
			IS_ROOT(File)?L" <ROOT>":L"", Name);

	/* Fail unless opening read-only */
	if (Mode != EFI_FILE_MODE_READ) {
		PrintWarning(L"File '%s' can only be opened in read-only mode\n", Name);
		return EFI_WRITE_PROTECTED;
	}

	/* Additional failures */
	if ((StrCmp(Name, L"..") == 0) && IS_ROOT(File)) {
		PrintInfo(L"Trying to open <ROOT>'s parent\n");
		return EFI_NOT_FOUND;
	}

	/* See if we're trying to reopen current (which the EFI Shell insists on doing) */
	if ((*Name == 0) || (StrCmp(Name, L".") == 0)) {
		PrintInfo(L"  Reopening %s\n", IS_ROOT(File)?L"<ROOT>":FileName(File));
		File->RefCount++;
		*New = This;
		PrintInfo(L"  RET: " PERCENT_P L"\n", (UINTN) *New);
		return EFI_SUCCESS;
	}

	/* If we have an absolute path, don't bother completing with the parent */
	if (AbsolutePath) {
		len = 0;
	} else {
		strcpya(path, File->path);
		len = strlena(path);
		/* Add delimiter if needed */
		if ((len == 0) || (path[len-1] != '/'))
			path[len++] = '/';
	}

	/* Copy the rest of the path (converted to UTF-8) */
	Status = Utf16ToUtf8NoAlloc(Name, &path[len], sizeof(path) - len);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not convert path to UTF-8");
		return Status;
	}
	/* Convert the delimiters */
	for (i = strlena(path) - 1 ; i >= len; i--) {
		if (path[i] == '\\')
			path[i] = '/';
	}

	/* We only want to handle with absolute paths */
	clean_path[0] = '/';
	/* Find out if we're dealing with root by removing the junk */
	CopyPathRelative(&clean_path[1], path, MAX_PATH - 1);
	if (clean_path[1] == 0) {
		/* We're dealing with the root */
		PrintInfo(L"  Reopening <ROOT>\n");
		*New = &File->FileSystem->RootFile->EfiFile;
		/* Must make sure that DirIndex is reset too (NB: no concurrent access!) */
		File->FileSystem->RootFile->DirIndex = 0;
		PrintInfo(L"  RET: " PERCENT_P L"\n", (UINTN) *New);
		return EFI_SUCCESS;
	}

	// TODO: eventually we should seek for already opened files and increase RefCount
	/* Allocate and initialise an instance of a file */
	Status = GrubCreateFile(&NewFile, File->FileSystem);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not instantiate file");
		return Status;
	}

	NewFile->path = AllocatePool(strlena(clean_path)+1);
	if (NewFile->path == NULL) {
		GrubDestroyFile(NewFile);
		PrintError(L"Could not instantiate path\n");
		return EFI_OUT_OF_RESOURCES;
	}
	strcpya(NewFile->path, clean_path);

	/* Isolate the basename and dirname */
	for (i = strlena(clean_path) - 1; i >= 0; i--) {
		if (clean_path[i] == '/') {
			clean_path[i] = 0;
			break;
		}
	}
	dirname = (i <= 0) ? "/" : clean_path;
	NewFile->basename = &NewFile->path[i+1];

	/* Find if we're working with a directory and fill the grub timestamp */
	Status = GrubDir(NewFile, dirname, InfoHook, (VOID *) NewFile);
	if (EFI_ERROR(Status)) {
		if (Status != EFI_NOT_FOUND)
			PrintStatusError(Status, L"Could not get file attributes for '%s'", Name);
		FreePool(NewFile->path);
		GrubDestroyFile(NewFile);
		return Status;
	}

	/* Finally we can call on GRUB open() if it's a regular file */
	if (!NewFile->IsDir) {
		Status = GrubOpen(NewFile);
		if (EFI_ERROR(Status)) {
			if (Status != EFI_NOT_FOUND)
				PrintStatusError(Status, L"Could not open file '%s'", Name);
			FreePool(NewFile->path);
			GrubDestroyFile(NewFile);
			return Status;
		}
	}

	NewFile->RefCount++;
	*New = &NewFile->EfiFile;

	PrintInfo(L"  RET: " PERCENT_P L"\n", (UINTN) *New);
	return EFI_SUCCESS;
}

/* Ex version */
static EFI_STATUS EFIAPI
FileOpenEx(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New, CHAR16 *Name,
	UINT64 Mode, UINT64 Attributes, EFI_FILE_IO_TOKEN *Token)
{
	return FileOpen(This, New, Name, Mode, Attributes);
}


/**
 * Close file
 *
 * @v This			File handle
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileClose(EFI_FILE_HANDLE This)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Close(" PERCENT_P L"|'%s') %s\n", (UINTN) This, FileName(File),
		IS_ROOT(File)?L"<ROOT>":L"");

	/* Nothing to do it this is the root */
	if (IS_ROOT(File))
		return EFI_SUCCESS;

	if (--File->RefCount == 0) {
		/* Close the file if it's a regular one */
		if (!File->IsDir)
			GrubClose(File);
		/* NB: basename points into File->path and does not need to be freed */
		if (File->path != NULL)
			FreePool(File->path);
		GrubDestroyFile(File);
	}

	return EFI_SUCCESS;
}

/**
 * Close and delete file
 *
 * @v This			File handle
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileDelete(EFI_FILE_HANDLE This)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintError(L"Cannot delete '%s'\n", FileName(File));

	/* Close file */
	FileClose(This);

	/* Warn of failure to delete */
	return EFI_WARN_DELETE_FAILURE;
}

/* GRUB uses a callback for each directory entry, whereas EFI uses repeated
 * firmware generated calls to FileReadDir() to get the info for each entry,
 * so we have to reconcile the twos. For now, we'll re-issue a call to GRUB
 * dir(), and run through all the entries (to find the one we
 * are interested in) multiple times. Maybe later we'll try to optimize this
 * by building a one-off chained list of entries that we can parse...
 */
static INT32
DirHook(const CHAR8 *name, const GRUB_DIRHOOK_INFO *DirInfo, VOID *Data)
{
	EFI_STATUS Status;
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	INT64 *Index = (INT64 *) &Info->FileSize;
	CHAR8 *filename = (CHAR8 *) (UINTN) Info->PhysicalSize;
	EFI_TIME Time = { 1970, 01, 01, 00, 00, 00, 0, 0, 0, 0, 0};

	// Eliminate '.' or '..'
	if ((name[0] ==  '.') && ((name[1] == 0) || ((name[1] == '.') && (name[2] == 0))))
		return 0;

	/* Ignore any entry that doesn't match our index */
	if ((*Index)-- != 0)
		return 0;

	strcpya(filename, name);

	Status = Utf8ToUtf16NoAlloc(filename, Info->FileName, (INTN)(Info->Size - sizeof(EFI_FILE_INFO)));
	if (EFI_ERROR(Status)) {
		if (Status != EFI_BUFFER_TOO_SMALL)
			PrintStatusError(Status, L"Could not convert directory entry to UTF-8");
		return (INT32) Status;
	}
	/* The Info struct size already accounts for the extra NUL */
	Info->Size = sizeof(*Info) + StrLen(Info->FileName) * sizeof(CHAR16);

	// Oh, and of course GRUB uses a 32 bit signed mtime value (seriously, wtf guys?!?)
	if (DirInfo->MtimeSet)
		GrubTimeToEfiTime(DirInfo->Mtime, &Time);
	CopyMem(&Info->CreateTime, &Time, sizeof(Time));
	CopyMem(&Info->LastAccessTime, &Time, sizeof(Time));
	CopyMem(&Info->ModificationTime, &Time, sizeof(Time));

	Info->Attribute = EFI_FILE_READ_ONLY;
	if (DirInfo->Dir)
		Info->Attribute |= EFI_FILE_DIRECTORY;

	return 0;
}

/**
 * Read directory entry
 *
 * @v file			EFI file
 * @v Len			Length to read
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS
FileReadDir(EFI_GRUB_FILE *File, UINTN *Len, VOID *Data)
{
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	EFI_STATUS Status;
	/* We temporarily repurpose the FileSize as a *signed* entry index */
	INT64 *Index = (INT64 *) &Info->FileSize;
	/* And PhysicalSize as a pointer to our filename */
	CHAR8 **basename = (CHAR8 **) &Info->PhysicalSize;
	CHAR8 path[MAX_PATH];
	EFI_GRUB_FILE *TmpFile = NULL;
	INTN len;

	/* Unless we can fit our maximum size, forget it */
	if (*Len < sizeof(EFI_FILE_INFO)) {
		*Len = MINIMUM_INFO_LENGTH;
		return EFI_BUFFER_TOO_SMALL;
	}

	/* Populate our Info template */
	ZeroMem(Data, *Len);
	Info->Size = *Len;
	*Index = File->DirIndex;
	strcpya(path, File->path);
	len = strlena(path);
	if (path[len-1] != '/')
		path[len++] = '/';
	*basename = &path[len];

	/* Invoke GRUB's directory listing */
	Status = GrubDir(File, File->path, DirHook, Data);
	if (*Index >= 0) {
		/* No more entries */
		*Len = 0;
		return EFI_SUCCESS;
	}

	if (EFI_ERROR(Status)) {
        if (Status == EFI_BUFFER_TOO_SMALL) {
            *Len = MINIMUM_INFO_LENGTH;
        } else {
		    PrintStatusError(Status, L"Directory listing failed");
        }
		return Status;
	}

	/* Our Index/FileSize must be reset */
	Info->FileSize = 0;
	Info->PhysicalSize = 0;

	/* For regular files, we still need to fill the size */
	if (!(Info->Attribute & EFI_FILE_DIRECTORY)) {
		/* Open the file and read its size */
		Status = GrubCreateFile(&TmpFile, File->FileSystem);
		if (EFI_ERROR(Status)) {
			PrintStatusError(Status, L"Unable to create temporary file");
			return Status;
		}
		TmpFile->path = path;

		Status = GrubOpen(TmpFile);
		if (EFI_ERROR(Status)) {
			// TODO: EFI_NO_MAPPING is returned for links...
			PrintStatusError(Status, L"Unable to obtain the size of '%s'", Info->FileName);
			/* Non fatal error */
		} else {
			Info->FileSize = GrubGetFileSize(TmpFile);
			Info->PhysicalSize = GrubGetFileSize(TmpFile);
			GrubClose(TmpFile);
		}
		GrubDestroyFile(TmpFile);
	}

	*Len = (UINTN) Info->Size;
	/* Advance to the next entry */
	File->DirIndex++;

//	PrintInfo(L"  Entry[%d]: '%s' %s\n", File->DirIndex-1, Info->FileName,
//			(Info->Attribute&EFI_FILE_DIRECTORY)?L"<DIR>":L"");

	return EFI_SUCCESS;
}

/**
 * Read from file
 *
 * @v This			File handle
 * @v Len			Length to read
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileRead(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Read(" PERCENT_P L"|'%s', %d) %s\n", (UINTN) This, FileName(File),
			*Len, File->IsDir?L"<DIR>":L"");

	/* If this is a directory, then fetch the directory entries */
	if (File->IsDir)
		return FileReadDir(File, Len, Data);

	return GrubRead(File, Data, Len);
}

/* Ex version */
static EFI_STATUS EFIAPI
FileReadEx(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return FileRead(This, &(Token->BufferSize), Token->Buffer);
}

/**
 * Write to file
 *
 * @v This			File handle
 * @v Len			Length to write
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileWrite(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintError(L"Cannot write to '%s'\n", FileName(File));
	return EFI_WRITE_PROTECTED;
}

/* Ex version */
static EFI_STATUS EFIAPI
FileWriteEx(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return FileWrite(This, &(Token->BufferSize), Token->Buffer);
}

/**
 * Set file position
 *
 * @v This			File handle
 * @v Position		New file position
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileSetPosition(EFI_FILE_HANDLE This, UINT64 Position)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	UINT64 FileSize;

	PrintInfo(L"SetPosition(" PERCENT_P L"|'%s', %lld) %s\n", (UINTN) This,
		FileName(File), Position, (File->IsDir)?L"<DIR>":L"");

	/* If this is a directory, reset the Index to the start */
	if (File->IsDir) {
		if (Position != 0)
			return EFI_INVALID_PARAMETER;
		File->DirIndex = 0;
		return EFI_SUCCESS;
	}

	/* Fail if we attempt to seek past the end of the file (since
	 * we do not support writes).
	 */
	FileSize = GrubGetFileSize(File);
	if (Position > FileSize) {
		PrintError(L"'%s': Cannot seek to #%llx of %llx\n",
				FileName(File), Position, FileSize);
		return EFI_UNSUPPORTED;
	}

	/* Set position */
	GrubSetFileOffset(File, Position);
	PrintDebug(L"'%s': Position set to %llx\n",
			FileName(File), Position);

	return EFI_SUCCESS;
}

/**
 * Get file position
 *
 * @v This			File handle
 * @ret Position	New file position
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileGetPosition(EFI_FILE_HANDLE This, UINT64 *Position)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"GetPosition(" PERCENT_P L"|'%s', %lld)\n", (UINTN) This, FileName(File));

	if (File->IsDir)
		*Position = File->DirIndex;
	else
		*Position = GrubGetFileOffset(File);
	return EFI_SUCCESS;
}

/**
 * Get file information
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileGetInfo(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN *Len, VOID *Data)
{
	EFI_STATUS Status;
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	EFI_FILE_SYSTEM_INFO *FSInfo = (EFI_FILE_SYSTEM_INFO *) Data;
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *VLInfo = (EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *)Data;
	EFI_TIME Time;
	CHAR8* label;
	UINTN tmpLen;

	PrintInfo(L"GetInfo(" PERCENT_P L"|'%s', %d) %s\n", (UINTN) This,
		FileName(File), *Len, File->IsDir?L"<DIR>":L"");

	/* Determine information to return */
	if (CompareMem(Type, &gEfiFileInfoGuid, sizeof(*Type)) == 0) {

		/* Fill file information */
		PrintExtra(L"Get regular file information\n");
		if (*Len < sizeof(EFI_FILE_INFO)) {
			*Len = MINIMUM_INFO_LENGTH;
			return EFI_BUFFER_TOO_SMALL;
		}

		ZeroMem(Data, sizeof(EFI_FILE_INFO));

		Info->Attribute = EFI_FILE_READ_ONLY;
		GrubTimeToEfiTime(File->Mtime, &Time);
		CopyMem(&Info->CreateTime, &Time, sizeof(Time));
		CopyMem(&Info->LastAccessTime, &Time, sizeof(Time));
		CopyMem(&Info->ModificationTime, &Time, sizeof(Time));

		if (File->IsDir) {
			Info->Attribute |= EFI_FILE_DIRECTORY;
		} else {
			Info->FileSize = GrubGetFileSize(File);
			Info->PhysicalSize = GrubGetFileSize(File);
		}

		tmpLen = (UINTN)(Info->Size - sizeof(EFI_FILE_INFO) - 1);
		Status = Utf8ToUtf16NoAllocUpdateLen(File->basename, Info->FileName, &tmpLen);
		if (EFI_ERROR(Status)) {
			if (Status != EFI_BUFFER_TOO_SMALL) {
				PrintStatusError(Status, L"Could not convert basename to UTF-16");
            } else {
                *Len = MINIMUM_INFO_LENGTH;
            }
			return Status;
		}

		/* The Info struct size already accounts for the extra NUL */
		Info->Size = sizeof(EFI_FILE_INFO) + tmpLen;
		*Len = (INTN)Info->Size;
		return EFI_SUCCESS;

	} else if (CompareMem(Type, &gEfiFileSystemInfoGuid, sizeof(*Type)) == 0) {

		/* Get file system information */
		PrintExtra(L"Get file system information\n");
		if (*Len < sizeof(EFI_FILE_SYSTEM_INFO)) {
			*Len = MINIMUM_FS_INFO_LENGTH;
			return EFI_BUFFER_TOO_SMALL;
		}

		ZeroMem(Data, sizeof(EFI_FILE_INFO));
		FSInfo->Size = *Len;
		FSInfo->ReadOnly = 1;
		/* NB: This should really be cluster size, but we don't have access to that */
		if (File->FileSystem->BlockIo2 != NULL) {
			FSInfo->BlockSize = File->FileSystem->BlockIo2->Media->BlockSize;
		} else {
			FSInfo->BlockSize = File->FileSystem->BlockIo->Media->BlockSize;
		}
		if (FSInfo->BlockSize  == 0) {
			PrintWarning(L"Corrected Media BlockSize\n");
			FSInfo->BlockSize = 512;
		}
		if (File->FileSystem->BlockIo2 != NULL) {
			FSInfo->VolumeSize = (File->FileSystem->BlockIo2->Media->LastBlock + 1) *
				FSInfo->BlockSize;
		} else {
			FSInfo->VolumeSize = (File->FileSystem->BlockIo->Media->LastBlock + 1) *
				FSInfo->BlockSize;
		}
		/* No idea if we can easily get this for GRUB, and the device is RO anyway */
		FSInfo->FreeSpace = 0;

		Status = GrubLabel(File, &label);
		if (EFI_ERROR(Status)) {
			PrintStatusError(Status, L"Could not read disk label");
			FSInfo->VolumeLabel[0] = 0;
			*Len = sizeof(EFI_FILE_SYSTEM_INFO);
		} else {
			tmpLen = (INTN)(FSInfo->Size - sizeof(EFI_FILE_SYSTEM_INFO) - 1);
			Status = Utf8ToUtf16NoAllocUpdateLen(label, FSInfo->VolumeLabel, &tmpLen);
			if (EFI_ERROR(Status)) {
				if (Status != EFI_BUFFER_TOO_SMALL) {
					PrintStatusError(Status, L"Could not convert label to UTF-16");
                } else {
                    *Len = MINIMUM_FS_INFO_LENGTH;
                }
				return Status;
			}
			FSInfo->Size = sizeof(EFI_FILE_SYSTEM_INFO) - 1 + tmpLen;
			*Len = (INTN)FSInfo->Size;
		}
		return EFI_SUCCESS;

	} else if (CompareMem(Type, &gEfiFileSystemVolumeLabelInfoIdGuid, sizeof(*Type)) == 0) {

		/* Get the volume label */
		Status = GrubLabel(File, &label);
		if (EFI_ERROR(Status)) {
			PrintStatusError(Status, L"Could not read disk label");
		}
		else {
			Status = Utf8ToUtf16NoAllocUpdateLen(label, VLInfo->VolumeLabel, Len);
			if (EFI_ERROR(Status)) {
				if (Status != EFI_BUFFER_TOO_SMALL)
					PrintStatusError(Status, L"Could not convert label to UTF-16");
				return Status;
			}
		}
		return EFI_SUCCESS;

	} else {

		Print(L"'%s': Cannot get information of type ", FileName(File));
		PrintGuid(Type);
		Print(L"\n");
		return EFI_UNSUPPORTED;

	}
}

/**
 * Set file information
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileSetInfo(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN Len, VOID *Data)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	Print(L"Cannot set information of type ");
	PrintGuid(Type);
	Print(L" for file '%s'\n", FileName(File));

	return EFI_WRITE_PROTECTED;
}

/**
 * Flush file modified data
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileFlush(EFI_FILE_HANDLE This)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Flush(" PERCENT_P L"|'%s')\n", (UINTN) This, FileName(File));
	return EFI_SUCCESS;
}

/* Ex version */
static EFI_STATUS EFIAPI
FileFlushEx(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN *Token)
{
	return FileFlush(This);
}

/**
 * Open root directory
 *
 * @v This			EFI simple file system
 * @ret Root		File handle for the root directory
 * @ret Status		EFI status code
 */
EFI_STATUS EFIAPI
FileOpenVolume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This, EFI_FILE_HANDLE *Root)
{
	EFI_FS *FSInstance = _CR(This, EFI_FS, FileIoInterface);

	PrintInfo(L"OpenVolume\n");
	*Root = &FSInstance->RootFile->EfiFile;

	return EFI_SUCCESS;
}

/**
 * Install the EFI simple file system protocol
 * If successful this call instantiates a new FS#: drive, that is made
 * available on the next 'map -r'. Note that all this call does is add
 * the FS protocol. OpenVolume won't be called until a process tries
 * to access a file or the root directory on the volume.
 */
EFI_STATUS
FSInstall(EFI_FS *This, EFI_HANDLE ControllerHandle)
{
	EFI_STATUS Status;

	/* Check if it's a filesystem we can handle */
	if (!GrubFSProbe(This))
		return EFI_UNSUPPORTED;

	PrintInfo(L"FSInstall: %s\n", This->DevicePathString);

	/* Initialize the root handle */
	Status = GrubCreateFile(&This->RootFile, This);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not create root file");
		return Status;
	}

	/* Setup the EFI part */
	This->RootFile->EfiFile.Revision = EFI_FILE_PROTOCOL_REVISION2;
	This->RootFile->EfiFile.Open = FileOpen;
	This->RootFile->EfiFile.Close = FileClose;
	This->RootFile->EfiFile.Delete = FileDelete;
	This->RootFile->EfiFile.Read = FileRead;
	This->RootFile->EfiFile.Write = FileWrite;
	This->RootFile->EfiFile.GetPosition = FileGetPosition;
	This->RootFile->EfiFile.SetPosition = FileSetPosition;
	This->RootFile->EfiFile.GetInfo = FileGetInfo;
	This->RootFile->EfiFile.SetInfo = FileSetInfo;
	This->RootFile->EfiFile.Flush = FileFlush;
	This->RootFile->EfiFile.OpenEx = FileOpenEx;
	This->RootFile->EfiFile.ReadEx = FileReadEx;
	This->RootFile->EfiFile.WriteEx = FileWriteEx;
	This->RootFile->EfiFile.FlushEx = FileFlushEx;

	/* Setup the other attributes */
	This->RootFile->path = "/";
	This->RootFile->basename = &This->RootFile->path[1];
	This->RootFile->IsDir = TRUE;

	/* Install the simple file system protocol. */
	Status = BS->InstallMultipleProtocolInterfaces(&ControllerHandle,
			&gEfiSimpleFileSystemProtocolGuid, &This->FileIoInterface,
			NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not install simple file system protocol");
		return Status;
	}

	return EFI_SUCCESS;
}

/* Uninstall EFI simple file system protocol */
VOID
FSUninstall(EFI_FS *This, EFI_HANDLE ControllerHandle)
{
	PrintInfo(L"FSUninstall: %s\n", This->DevicePathString);

	BS->UninstallMultipleProtocolInterfaces(ControllerHandle,
			&gEfiSimpleFileSystemProtocolGuid, &This->FileIoInterface,
			NULL);
}
