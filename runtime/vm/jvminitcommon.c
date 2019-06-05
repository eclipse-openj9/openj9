/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "jvminitcommon.h"
#include "ut_j9vm.h"

/**
 *  Walks the J9VMDllLoadInfo table looking for the entry for a specific library name. 
 *  
 *  @param aPool the pool holding the DLL info
 *  @param dllName the name of the dll we are looking for
 *  @returns J9VMDllLoadInfo for the dll requested or NULL if not found
 */
J9VMDllLoadInfo *findDllLoadInfo(J9Pool* aPool, const char* dllName) {
	J9VMDllLoadInfo *anElement;
	pool_state aState;
	char key[SMALL_STRING_BUF_SIZE];			/* Plenty big enough. Needs to be at least DLLNAME_LEN + 4. */

	strncpy(key, dllName, SMALL_STRING_BUF_SIZE);

	anElement = (J9VMDllLoadInfo*)pool_startDo(aPool, &aState);
	while (anElement && (strcmp(anElement->dllName, key)!=0)) {
		anElement = (J9VMDllLoadInfo*)pool_nextDo(&aState);
	}

	/* If library not found and name does not end in major/minor version, prepend j9 and append version */
	if (anElement==NULL && strstr(dllName, J9_DLL_VERSION_STRING)==NULL) {
		memset(key, 0, SMALL_STRING_BUF_SIZE);
		strcpy(key, "j9");
		safeCat(key, dllName, SMALL_STRING_BUF_SIZE);
		safeCat(key, J9_DLL_VERSION_STRING, SMALL_STRING_BUF_SIZE);

		anElement = (J9VMDllLoadInfo*)pool_startDo(aPool, &aState);
		while (anElement && (strcmp(anElement->dllName, key)!=0)) {
			anElement = (J9VMDllLoadInfo*)pool_nextDo(&aState);
		}
	}

	return anElement;
}

/**
 * Frees the dll load table
 * 
 * @param table the dll table to free 
 */
void freeDllLoadTable(J9Pool* table) {
	if (table)
		pool_kill(table);
}

/**
 *  Used as safe strcat 
 *  
 *  @param buffer holding existing string we should cat to
 *  @param text string to be added to existing string
 *  @param length length of the buffer
 */
IDATA safeCat(char* buffer, const char* text, IDATA length) {
	IDATA currentLength = strlen(buffer);
	IDATA textLength = strlen(text);
	IDATA availableChars = length - currentLength - 1;
	if (availableChars > 0) {
		strncat(buffer, text, availableChars);
		buffer[length-1] = '\0';
	}
	if (textLength > availableChars) {
		return (textLength - availableChars);
	}
	return 0;
}

/**
 *  Creates a J9VMDllLoadInfo as an element of the pool. 
 *   
 *  @param portLibrary port library that can be used in the function
 *  @param aPool the pool into which the load info for the dll should be added.  Pool must zero the elements.
 *  @param name Name of the DLL
 *  @param flags flags to be associated with the dll
 *  @param methodPointer to be associated with the library. Should be NULL unless the NOT_A_LIBRARY flag is set
 *  @param verboseFlags the verbose flags to be used for tracing
 *  
 *  @returns J9VMDllLoadInfo for the dll specified or NULL on failure
 */
J9VMDllLoadInfo *createLoadInfo(J9PortLibrary* portLibrary, J9Pool* aPool, char* name, U_32 flags, void* methodPointer, UDATA verboseFlags) {
	J9VMDllLoadInfo* returnVal = NULL;
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* Pool must return zero'd elements */
	returnVal = (J9VMDllLoadInfo*) pool_newElement(aPool);

	if (!returnVal) {
		return NULL;
	}
	Assert_VM_notNull(name);
	JVMINIT_VERBOSE_INIT_TRACE1(verboseFlags, "Creating table entry for %s\n", name);
	returnVal->loadFlags = flags;
	strncpy(returnVal->dllName, name, (DLLNAME_LEN - 1));
	returnVal->dllName[(DLLNAME_LEN - 1)] = '\0';	/* Ensure last char is null as strncpy won't guarantee it */
	returnVal->j9vmdllmain = (flags & (NOT_A_LIBRARY | BUNDLED_COMP)) ? (IDATA  (*)(struct J9JavaVM*, IDATA, void*))methodPointer : NULL;
	return returnVal;
}

/**
 * This method indicates if the requested jniVersion is valid or not
 * @param jniVersion the version to be checked
 * @returns true if the version is valid
 */
UDATA 
jniVersionIsValid(UDATA jniVersion)
{
	return (jniVersion == JNI_VERSION_1_1) 
		   || (jniVersion == JNI_VERSION_1_2) 
		   || (jniVersion == JNI_VERSION_1_4) 
		   || (jniVersion == JNI_VERSION_1_6) 
		   || (jniVersion == JNI_VERSION_1_8)
#if JAVA_SPEC_VERSION >= 9
		   || (jniVersion == JNI_VERSION_9)
#endif /* JAVA_SPEC_VERSION >= 9 */
#if JAVA_SPEC_VERSION >= 10
		   || (jniVersion == JNI_VERSION_10)
#endif /* JAVA_SPEC_VERSION >= 10 */
		   ;
}
