/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#ifndef WINDOWS_H
#define WINDOWS_H
typedef void *HANDLE;
typedef int CRITICAL_SECTION;
typedef int DWORD;
typedef int PROCESS_INFORMATION;
typedef int PDWORD;
typedef int DWORD64;
typedef int PDWORD64;
typedef int PSYMBOL_INFO;
typedef int PIMAGEHLP_LINE64;
typedef int PIMAGEHLP_MODULE64;
typedef int BOOL;
typedef int PSTR;
typedef int PWSTR;
typedef int LPAPI_VERSION;
typedef int LPSTACKFRAME64;
typedef int PREAD_PROCESS_MEMORY_ROUTINE64;
typedef int PGET_MODULE_BASE_ROUTINE64;
typedef int PTRANSLATE_ADDRESS_ROUTINE64;
typedef int PFUNCTION_TABLE_ACCESS_ROUTINE64;
typedef int PGET_MODULE_BASE_ROUTINE64;
typedef int PTRANSLATE_ADDRESS_ROUTINE64;
typedef int PVOID;
typedef int HINSTANCE;
typedef LPAPI_VERSION (*IMAGEHLPAPIVERSION)(void);
typedef BOOL (__stdcall *SYMINITIALIZE) (HANDLE, PSTR, BOOL);
typedef BOOL (__stdcall *SYMINITIALIZEW) (HANDLE, PWSTR, BOOL);
typedef BOOL (__stdcall *SYMCLEANUP)(HANDLE hProcess);
typedef BOOL (__stdcall *SYMCLEANUP) (HANDLE);
typedef DWORD (__stdcall *SYMGETOPTIONS) (void);
typedef DWORD (__stdcall *SYMSETOPTIONS) (DWORD);
typedef BOOL (__stdcall *SYMFROMADDR) (HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL (__stdcall *STACKWALK64) ( DWORD
          ,HANDLE
          ,HANDLE
          ,LPSTACKFRAME64
          ,PVOID
          ,PREAD_PROCESS_MEMORY_ROUTINE64
          ,PFUNCTION_TABLE_ACCESS_ROUTINE64
          ,PGET_MODULE_BASE_ROUTINE64
          ,PTRANSLATE_ADDRESS_ROUTINE64
           );
typedef int SOCKADDR;
typedef int SOCKET;
typedef int SOCKADDR_IN;
typedef void *KAFFINITY;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int INT;
typedef unsigned int UINT;

#endif
