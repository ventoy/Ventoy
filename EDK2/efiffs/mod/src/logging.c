/* logging.c - EFI logging */
/*
 *  Copyright © 2014-2017 Pete Batard <pete@akeo.ie>
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

/* Not defined in gnu-efi yet */
#define SHELL_VARIABLE_GUID { \
	0x158def5a, 0xf656, 0x419c, { 0xb0, 0x27, 0x7a, 0x31, 0x92, 0xc0, 0x79, 0xd2 } \
}
extern EFI_GUID gShellVariableGuid;
EFI_GUID ShellVariable = SHELL_VARIABLE_GUID;

static UINTN PrintNone(IN CONST CHAR16 *fmt, ... ) { return 0; }
Print_t PrintError = PrintNone;
Print_t PrintWarning = PrintNone;
Print_t PrintInfo = PrintNone;
Print_t PrintDebug = PrintNone;
Print_t PrintExtra = PrintNone;
Print_t* PrintTable[] = { &PrintError, &PrintWarning, &PrintInfo,
		&PrintDebug, &PrintExtra };

/* Global driver verbosity level */
#if !defined(DEFAULT_LOGLEVEL)
#define DEFAULT_LOGLEVEL FS_LOGLEVEL_NONE
#endif
UINTN LogLevel = DEFAULT_LOGLEVEL;

/**
 * Print status
 *
 * @v Status		EFI status code
 */
VOID
PrintStatus(EFI_STATUS Status)
{
#if defined(__MAKEWITH_GNUEFI)
	CHAR16 StatusString[64];
	StatusToString(StatusString, Status);
	// Make sure the Status is unsigned 32 bits
	Print(L": [%d] %s\n", (Status & 0x7FFFFFFF), StatusString);
#else
	Print(L": [%d]\n", (Status & 0x7FFFFFFF));
#endif
}

int g_fs_name_nocase = 0;
/*
 * You can control the verbosity of the driver output by setting the shell environment
 * variable FS_LOGGING to one of the values defined in the FS_LOGLEVEL constants
 */
VOID
SetLogging(VOID)
{
	EFI_STATUS Status;
	CHAR16 LogVar[4];
	UINTN i, LogVarSize = sizeof(LogVar);

    i = LogVarSize;
	Status = RT->GetVariable(L"FS_NAME_NOCASE", &ShellVariable, NULL, &i, LogVar);
    if (Status == EFI_SUCCESS)
        g_fs_name_nocase = 1;

	Status = RT->GetVariable(L"FS_LOGGING", &ShellVariable, NULL, &LogVarSize, LogVar);
	if (Status == EFI_SUCCESS)
		LogLevel = Atoi(LogVar);

	for (i=0; i<ARRAYSIZE(PrintTable); i++)
		*PrintTable[i] = (i < LogLevel)?(Print_t)Print:(Print_t)PrintNone;

	PrintExtra(L"LogLevel = %d\n", LogLevel);
}
