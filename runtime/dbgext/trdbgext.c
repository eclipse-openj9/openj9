/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include <string.h>

#include "j9dbgext.h"

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
UDATA dbgTrInitialize (void);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void dbgTrPrint (const char *args);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


#ifdef J9VM_INTERP_NATIVE_SUPPORT

void (JNICALL *dbgjit_TrInitialize)(J9JavaVM *local_vm,
              J9PortLibrary *dbgPortLib,
              void (*dbgjit_Print)(const char *s, ...),
              void (*dbgjit_ReadMemory)(UDATA address, void *structure, UDATA size, UDATA *bytesRead),
              UDATA (*dbgjit_GetExpression)(const char* args),
              void* (*dbgjit_Malloc)(UDATA size, void *originalAddress),
              void* (*dbgjit_MallocAndRead)(UDATA size, void *remoteAddress),
              void (*dbgjit_Free)(void * addr),
              void* (*dbgjit_FindPatternInRange)(U_8* pattern, UDATA patternLength, UDATA patternAlignment,
                            U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched)
              );
void (JNICALL *dbgjit_TrPrint)(const char *name, void *addr, UDATA argCount, const char* args);

#endif

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
UDATA 
dbgTrInitialize(void)
{
	void * remoteVM = 0;
	J9JavaVM * localVM = 0;
	UDATA jitd_handle = 0;
	static UDATA isTrInitialized = FALSE;

	PORT_ACCESS_FROM_PORT(dbgGetPortLibrary());

	if (isTrInitialized) {
		return TRUE;
	}
	
	remoteVM = dbgSniffForJavaVM();
	if (remoteVM) {
		localVM = dbgReadJavaVM(remoteVM);
	}

	if (j9sl_open_shared_library(J9_JIT_DLL_NAME, &jitd_handle, TRUE) != 0) {
		return FALSE;
	}
	if (j9sl_lookup_name(jitd_handle, "dbgjit_TrInitialize", (UDATA *) &dbgjit_TrInitialize, "PPPPPPPPP") != 0) {
		return FALSE;
	}
	if (j9sl_lookup_name(jitd_handle, "dbgjit_TrPrint", (UDATA *) &dbgjit_TrPrint, "PPiP") != 0) {
		return FALSE;
	}
		
	dbgjit_TrInitialize(localVM, PORTLIB, &dbgPrint, &dbgReadMemory, &dbgGetExpression, &dbgMalloc, &dbgMallocAndRead, &dbgFree, &dbgFindPatternInRange);
	return isTrInitialized = TRUE;
}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


void 
dbgext_trprint(const char *args) 
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (!dbgTrInitialize()) {
		dbgPrint("*** JIT Warning: problem with %s\n", J9_JIT_DLL_NAME);
	}
	dbgTrPrint(args);
#else
	dbgPrint("No JIT in this build\n");
#endif
}


#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/***
 * wrapper of dbgext_trprint;
 * to be called from !trprint extension
 */
static void dbgTrPrint(const char *args)
{      
	/***
	 * processing arguments
	 * - first argument is a string (name)
	 * - second argument is an address (pointer to TR's class)
	 */
#define MAX_ARGS 2
#define NAME_LEN 64
	UDATA argCount;
	UDATA argValues[MAX_ARGS];
	char name[NAME_LEN];  /* 1st argument */
	size_t len;
	void *addr;	/* 2nd argument */
	
	argCount = dbgParseArgs(args, argValues, MAX_ARGS);
	
	/* 1st argument is a string */
	if (argCount >= 1) {
		len = (argCount == 1) ? strlen(args) : strchr(args, ',') - args;
		if (len > NAME_LEN-1) {
			dbgPrint("*** JIT Error: string argument is too long!\n");
			return;
		}
		strncpy(name, args, len);
		name[len] = 0;
	}
	
	/* 2nd argument is an address */
	addr = (argCount >= 2) ? (void*) argValues[1] : NULL;
#undef NAME_LEN	
#undef MAX_ARGS	

	/* leave the rest of the arguments unprocessed and passed as 'args' */
	dbgjit_TrPrint((const char*) name, addr, argCount, args);	
}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */



