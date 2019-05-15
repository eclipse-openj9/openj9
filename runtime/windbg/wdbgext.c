/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stdlib.h>

#include "wdbgext.h"
#include "j9port.h"

// code from http://www.wdj.com/archive/0912/feature.html
//
// note that the name of the following variable is important,
// because the macros in <j9dbgexts.h> assume it is ExtensionApis.
	WINDBG_EXTENSION_APIS ExtensionApis;
USHORT MjVersion;
USHORT MnVersion;

// ExtensionApiVersion() is called by the debugger to insure that
// your routines were written to a compatible interface version.
#ifdef J9VM_ENV_DATA64
EXT_API_VERSION Version = { 3, 5, EXT_API_VERSION_NUMBER64, 0 };
#else
EXT_API_VERSION Version = { 3, 5, EXT_API_VERSION_NUMBER, 0 };
#endif



LPEXT_API_VERSION
   WDBGAPI
   ExtensionApiVersion()
{
	return &Version;
} 
// WinDbgExtensionDllInit() is called when the debugger loads
// your extension DLL.
void
   WDBGAPI
   WinDbgExtensionDllInit(
						  PWINDBG_EXTENSION_APIS lpExtensionApis,
						  USHORT                 MajorVersion,
						  USHORT                 MinorVersion
						 )
{
	ExtensionApis = *lpExtensionApis;   // structure copy
	MjVersion = MajorVersion;
	MnVersion = MinorVersion;
}

void dbgReadMemory (UDATA address, void *structure, UDATA size, UDATA *bytesRead)
{
	*bytesRead = 0;
	ReadMemory(address,structure,(U_32)size,(U_32*)bytesRead);
}
UDATA dbgGetExpression (const char* args)
{
	return GetExpression(args);
}

/*
 * See dbgFindPatternInRange
 */
void* 
dbgFindPattern(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA* bytesSearched) 
{
#if defined(J9VM_ENV_DATA64)
	*bytesSearched = 0;
	return NULL;
#else
	return dbgFindPatternInRange(pattern, patternLength, patternAlignment, startSearchFrom, (UDATA)-1 - (UDATA)startSearchFrom, bytesSearched);
#endif
}

/*
 * Prints message to the debug print screen
 */
void 
dbgWriteString (const char* message)
{
	dprintf("%s", message);
}

I_32
dbg_j9port_create_library(J9PortLibrary *portLib, J9PortLibraryVersion *version, UDATA size)
{
	return j9port_create_library(portLib, version, size);
}
