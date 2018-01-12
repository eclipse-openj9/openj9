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
#if defined(WIN32) || defined (WIN64)
#include <windows.h>
#endif

#include "j9.h"
#include "dfix.h"
#include "dfix_internal.h"
#include "ut_j9dfix.h"

void
fixup(void* returnAddress, void* resolved)
{
	U_8* callsite = ((U_8*)returnAddress) - 5;
	IDATA oldDisplacement, newDisplacement, *displacementPtr;
	DWORD oldProtectionBits = 0;
	BOOL rcProtect = FALSE;

	/* Mark the code pages read/write */
	rcProtect = VirtualProtect(callsite, 5, PAGE_EXECUTE_READWRITE, &oldProtectionBits);
	if (!rcProtect) {
		Assert_DFIX_FixupFailed();
		return;
	}

	/**
	  On x86 the instruction looks like:
		00c01454 e847ffffff      call    dfixtest!zero_memory (00c013a0)
		00c01459 83c408          add     esp,								<-- returnAddress points here
	*/

	displacementPtr = (IDATA*)(callsite+1);
	oldDisplacement = *displacementPtr;
	newDisplacement = ((U_8*)resolved) - callsite - 5 /* instructions in this sequence*/ ;
	*displacementPtr = newDisplacement;

	/* Restore the original protection bits */
	rcProtect = VirtualProtect(callsite, 5, oldProtectionBits, &oldProtectionBits);
	if (!rcProtect) {
		Assert_DFIX_FixupFailed();
		return;
	}

	printf(
		"fixup(returnAddress=0x%p, callsite=0x%p (opcode=%x) resolved=0x%p)\n",
		returnAddress, callsite, *callsite, resolved);

}

#if defined(WIN32) || defined (WIN64)
BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (DLL_PROCESS_ATTACH == fdwReason) {
		if (!resolve_dfix_zero_memory()) {
			return FALSE;
		}
		if (!resolve_dfix_memcpy()) {
			return FALSE;
		}
	}
	return TRUE;
}
#endif

