/*******************************************************************************
 * Copyright (c) 2006, 2017 IBM Corp. and others
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

#ifndef jvminitcommon_h
#define jvminitcommon_h

#include <string.h>
#include <stdlib.h>

#include "j9.h"
#include "jvminit.h"

#include "j9vmnls.h"
#if defined(J9VM_INTERP_VERBOSE)
/* verbose messages for displaying stack info */
#include "verbosenls.h"
#endif


#define XRUN_LEN (sizeof(VMOPT_XRUN)-1)
#define DLLNAME_LEN 32									/* this value should be consistent with that in J9VMDllLoadInfo */
#define SMALL_STRING_BUF_SIZE 64
#define MED_STRING_BUF_SIZE 128
#define LARGE_STRING_BUF_SIZE 256

J9VMDllLoadInfo *findDllLoadInfo(J9Pool* aPool, const char* dllName);
IDATA safeCat (char* buffer, const char* text, IDATA length);
void freeDllLoadTable (J9Pool* table);
void freeVMArgsArray (J9PortLibrary* portLibrary, J9VMInitArgs* j9vm_args);
J9VMDllLoadInfo *createLoadInfo (J9PortLibrary* portLibrary, J9Pool* aPool, char* name, U_32 flags, void* methodPointer, UDATA verboseFlags);
UDATA jniVersionIsValid (UDATA jniVersion);




#endif     /* jvminitcommon_h */
