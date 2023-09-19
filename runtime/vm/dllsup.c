/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "j9user.h"
#include "j9.h"
#include "j9protos.h"
#include "jni.h"
#include "j9port.h"
#include "jvminit.h"
#include "vm_internal.h"
#include "j2sever.h"

IDATA
shutdownDLL(J9JavaVM *vm, UDATA descriptor, UDATA shutdownDueToExit)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint (JNICALL * j9OnUnLoadFunc)(J9JavaVM *, void*);

	if(0 == j9sl_lookup_name ( descriptor , "JVM_OnUnload", (void *) &j9OnUnLoadFunc, "iLL")) {
		/* if the function returns non zero then the library doesn't want to be shutdown */
		if ((j9OnUnLoadFunc) ((J9JavaVM*)vm, (void *) shutdownDueToExit)) return -2;
	}

	if (!shutdownDueToExit) {
		if (j9sl_close_shared_library(descriptor)) {
			return -1;
		}
	}

	return 0;
}

UDATA
loadJ9DLLWithPath(J9JavaVM *vm, J9VMDllLoadInfo *info, char *dllName)
{
	BOOLEAN loadFailed = FALSE;
	char *localBuffer = NULL;
	char *dllDirectory = NULL;

	UDATA openFlags = (info->loadFlags & XRUN_LIBRARY) ? J9PORT_SLOPEN_DECORATE | J9PORT_SLOPEN_LAZY : J9PORT_SLOPEN_DECORATE;

	PORT_ACCESS_FROM_JAVAVM(vm);

#if defined(J9VM_OPT_SIDECAR)


	/* VM is in a subdirectory iff j2seRootDirectory is set. */
	if ((NULL != vm->alternateJitDir) && (0 == strcmp(dllName, J9_JIT_DLL_NAME))) {
		dllDirectory = vm->alternateJitDir;
	} else if(vm->j2seRootDirectory) {
		dllDirectory = vm->j2seRootDirectory;
	}

	if (NULL != dllDirectory) {
		UDATA superSeparatorIndex = -1;
		UDATA bufferSize;

		if((info->loadFlags & XRUN_LIBRARY) && (J2SE_LAYOUT(vm) & J2SE_LAYOUT_VM_IN_SUBDIR)) {
			/* load the DLL from the parent of the j2seRootDir - find the last dir separator and declare that the end. */
			superSeparatorIndex = strrchr(dllDirectory, DIR_SEPARATOR) - dllDirectory;
			bufferSize = superSeparatorIndex + 1 + sizeof(DIR_SEPARATOR_STR) + strlen(dllName);  /* +1 for NUL */
		} else {
			bufferSize = j9str_printf(PORTLIB, NULL, 0, "%s%s%s",
				dllDirectory, DIR_SEPARATOR_STR, dllName);
		}
		localBuffer = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);

		if(!localBuffer) {
			setErrorJ9dll(PORTLIB, info, "cannot allocate memory in loadJ9DLL", FALSE);
			info->loadFlags |= FAILED_TO_LOAD;
			return FALSE;
		}

		if (superSeparatorIndex != -1) {  /* will be set if we need to clip right after parent dir - do NOT strcpy dirDirectory as it might be longer than the DLL name, resulting in a buffer overflow */
			memcpy(localBuffer, dllDirectory, superSeparatorIndex + 1);
			localBuffer[superSeparatorIndex+1] = (char) 0;
			strcat(localBuffer, dllName);
		} else {
			j9str_printf(PORTLIB, localBuffer, bufferSize, "%s%s%s",
				dllDirectory, DIR_SEPARATOR_STR, dllName);
		}	
	} else {
		localBuffer = dllName;
	}

	if ( j9sl_open_shared_library ( localBuffer, &(info->descriptor), openFlags ) ) {
		loadFailed = TRUE;
	}

	if (NULL != localBuffer) {
		j9mem_free_memory(localBuffer);
	}

	if(loadFailed && (info->loadFlags & XRUN_LIBRARY)) {
		if ( ! j9sl_open_shared_library ( dllName, &(info->descriptor), openFlags ) ) {
			loadFailed = FALSE;
		}
	}
#else
	if ( j9sl_open_shared_library ( dllName, &(info->descriptor), openFlags ) ) {
		loadFailed = TRUE;
	}
#endif

	return loadFailed;
}

UDATA
loadJ9DLL(J9JavaVM *vm, J9VMDllLoadInfo *info)
{
	BOOLEAN loadFailed = FALSE;

	PORT_ACCESS_FROM_JAVAVM(vm);

	loadFailed = loadJ9DLLWithPath(vm, info, info->dllName);

	if ( loadFailed && info->loadFlags & ALTERNATE_LIBRARY_NAME ) {
		if ( !(loadFailed = loadJ9DLLWithPath(vm, info, info->alternateDllName)) ) {
			info->loadFlags |= ALTERNATE_LIBRARY_USED;
		}
	}	

	if ( loadFailed ) {
		if ( !(info->loadFlags & SILENT_NO_DLL) ) {
			const char *errorStr = j9error_last_error_message();
			char *buffer = j9mem_allocate_memory(strlen(errorStr) + 1, OMRMEM_CATEGORY_VM);
			if (NULL != buffer) {
				strcpy(buffer, errorStr);
				setErrorJ9dll(PORTLIB, info, buffer, TRUE);
			} else {
				setErrorJ9dll(PORTLIB, info, "cannot allocate memory in loadJ9DLL", FALSE);
			}
		}
		info->loadFlags |= FAILED_TO_LOAD;
		return FALSE;
	} else {
		info->loadFlags |= LOADED;
		return TRUE;
	}
}

void
setErrorJ9dll(J9PortLibrary *portLib, J9VMDllLoadInfo *info, const char *error, BOOLEAN errorIsAllocated)
{
	/* If fatalErrorStr already points to a heap-allocated string, we need to free it before we can point it to a new string. */
	if ((NULL != info->fatalErrorStr) && J9_ARE_ANY_BITS_SET(info->loadFlags, FREE_ERROR_STRING)) {
		PORT_ACCESS_FROM_PORT(portLib);
		j9mem_free_memory((char *)info->fatalErrorStr);
	}

	info->fatalErrorStr = error;
	if ((NULL != error) && errorIsAllocated) {
		info->loadFlags |= FREE_ERROR_STRING; /* Indicates that buffer should be freed later. */
	} else {
		info->loadFlags &= ~FREE_ERROR_STRING;
	}
}

