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
#ifndef wdbgext_h
#define wdbgext_h

// Include file for WinDbg kernel debugger extensions.
// Note that you will typically NOT want to simply include
// <wdbgexts.h> in your projects because of dependencies
// on some Windows definitions.  Include this file instead.
//

// Include global DDK header
//#include <ntddk.h>

// Include base definitions required by WDBGEXTS.H
#include <windef.h>

// Some definitions from <winbase.h> that <wdbgexts.h> depends on
#define WINBASEAPI DECLSPEC_IMPORT
WINBASEAPI HLOCAL WINAPI LocalAlloc(UINT uFlags, SIZE_T uBytes);
WINBASEAPI HLOCAL WINAPI LocalFree(HLOCAL hMem);
#define LMEM_FIXED          0x0000
#define LMEM_ZEROINIT       0x0040
#define LPTR                (LMEM_FIXED | LMEM_ZEROINIT)

#define CopyMemory RtlCopyMemory
#define ZeroMemory RtlZeroMemory

// Include <wdbgexts.h> itself
// wdbgexts.h has an unused variable 'li' on line 526. Suppress this warning.
#pragma warning(push)
#pragma warning(disable : 4101)
#include <wdbgexts.h>
#pragma warning(pop)

// The following four variables are defined in wdjexts.c
extern WINDBG_EXTENSION_APIS ExtensionApis;
extern USHORT MjVersion;
extern USHORT MnVersion;
extern EXT_API_VERSION Version;

// The following structures are taken from the source file
// ddk\src\krnldbg\kdapis\windbgkd.h.  That file could be included
// here, but it's hard to specify on the INCLUDE path.
//
// KPROCESSOR_STATE is the structure of memory accessible with the
// ReadControlSpace() API.  DBGKD_GET_VERSION is the structure 
// returned by the IG_GET_KERNEL_VERSION Ioctl().
typedef struct _DESCRIPTOR {
	WORD    Pad;
	WORD    Limit;
	DWORD   Base;
} KDESCRIPTOR, *PKDESCRIPTOR;
typedef struct _KSPECIAL_REGISTERS {
	DWORD Cr0;
	DWORD Cr2;
	DWORD Cr3;
	DWORD Cr4;
	DWORD KernelDr0;
	DWORD KernelDr1;
	DWORD KernelDr2;
	DWORD KernelDr3;
	DWORD KernelDr6;
	DWORD KernelDr7;
	KDESCRIPTOR Gdtr;
	KDESCRIPTOR Idtr;
	WORD   Tr;
	WORD   Ldtr;
	DWORD Reserved[6];
} KSPECIAL_REGISTERS, *PKSPECIAL_REGISTERS;
typedef struct _KPROCESSOR_STATE {
	struct _CONTEXT ContextFrame;
	struct _KSPECIAL_REGISTERS SpecialRegisters;
} KPROCESSOR_STATE, *PKPROCESSOR_STATE;

typedef struct _DBGKD_GET_VERSION {
	WORD    MajorVersion;   // 0xF == Free, 0xC == Checked
	WORD    MinorVersion;   // 1381 for NT4.0, 1057 for NT3.51
	WORD    ProtocolVersion;
	WORD    Flags;          // DBGKD_VERS_FLAG_XXX
	DWORD   KernBase;       // load address of ntoskrnl.exe
	DWORD   PsLoadedModuleList;
	WORD    MachineType;
	WORD    ThCallbackStack;
	WORD    NextCallback;
	WORD    FramePointer;
	DWORD   KiCallUserMode;
	DWORD   KeUserCallbackDispatcher;
	DWORD   BreakpointWithStatus;
	DWORD   Reserved4;
} DBGKD_GET_VERSION, *PDBGKD_GET_VERSION;
#define DBGKD_VERS_FLAG_MP      0x0001

#undef DECLARE_API
#include "j9dbgext.h"

/* this is a debugger extension so include the common interface it extends */
#include "dbgext_api.h"

LPEXT_API_VERSION WDBGAPI ExtensionApiVersion();
void WDBGAPI WinDbgExtensionDllInit( PWINDBG_EXTENSION_APIS lpExtensionApis, USHORT MajorVersion, USHORT MinorVersion );


#endif     /* wdbgext_h */

