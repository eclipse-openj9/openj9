/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#include "j9protos.h"
#include "util_internal.h"
#include "vm_api.h"
#include "j9vmnls.h"
#include "jvminit.h"

#if defined(J9VM_THR_LOCK_NURSERY)
#include "locknursery.h"


#define LOCKNURSERY_OPTIONS_SEPARATOR_CHAR	','
#define	LOCKNURSERY_OPTIONS_SEPARATOR		","
#define LOCKNURSERY_OPTION_MODE				"mode="
#define LOCKNURSERY_OPTION_NO_LOCKWORD		"noLockword="
#define LOCKNURSERY_OPTION_LOCKWORD			"lockword="
#define LOCKNURSERY_OPTION_WHAT				"what"
#define LOCKNURSERY_OPTION_NONE				"none"

#define LOCKNURSERY_MODE_DEFAULT				"default"
#define LOCKNURSERY_MODE_MINIMIZE_FOOTPRINT		"minimizeFootprint"
#define LOCKNURSERY_MODE_ALL					"all"

/**
 * The structures and methods for the hash table used to contain the lock nursery exceptions
 */
#define LOCKNURSERY_INITIAL_TABLE_SIZE 16

/**
 * method that returns the hash for an entry in the lockword exceptions table
 * @param entry the entry
 * @param userData data specified as userData when the hastable was created, in our case pointer to the port library
 *
 */
static UDATA exceptionHashFn(void *entry, void *userData)
{
	J9UTF8* hashtableEntry = (J9UTF8*) REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(*((J9UTF8**)entry));
	
	return computeHashForUTF8(J9UTF8_DATA(hashtableEntry),J9UTF8_LENGTH(hashtableEntry));
}

/**
 * method that compares two entries for equality
 * @param lhsEntry the first entry to compare
 * @param rhsEntry the second entry to compare
 * @param userData data specified as userData when the hastable was created, in our case pointer to the port library
 *
 * @returns 1 if the entries match, 0 otherwise
 */
static UDATA exceptionHashEqualFn(void *lhsEntry, void *rhsEntry, void *userData)
{
	J9UTF8* lhsHashtableEntry = (J9UTF8*)REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(*((J9UTF8**)lhsEntry));
	J9UTF8* rhsHashtableEntry = (J9UTF8*)REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(*((J9UTF8**)rhsEntry));
	
	return J9UTF8_DATA_EQUALS(J9UTF8_DATA(lhsHashtableEntry),J9UTF8_LENGTH(lhsHashtableEntry),J9UTF8_DATA(rhsHashtableEntry),J9UTF8_LENGTH(rhsHashtableEntry));
}

/**
 * method that is called when we are deleting and entry from the hashtable
 * @param entry the entry to be deleted
 * @param userData data specified as userData when the hastable was created, in our case pointer to the port library
 *
 */
static UDATA exceptionDoDelete (void *entry, void *userData){
	PORT_ACCESS_FROM_PORT((J9PortLibrary*) userData);
	
	j9mem_free_memory((void*)REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(*((J9UTF8**)entry)));
	return TRUE;
}

/**
 * method that is called to print out what information for the entry
 * @param entry for which the info should be displayed
 * @param userData data specified as userData when the hastable was created, in our case pointer to the port library
 *
 */
static UDATA exceptionPrintWhat (void *entry, void *userData){
	J9UTF8* entryName = (J9UTF8*) REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(*((J9UTF8**)entry));
	PORT_ACCESS_FROM_PORT((J9PortLibrary*) userData);

	if (IS_LOCKNURSERY_HASHTABLE_ENTRY_MASKED(*((J9UTF8**)entry))){
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_DOES_NOT_HAVE_LOCKWORD, J9UTF8_LENGTH(entryName), J9UTF8_DATA(entryName));
	} else {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_HAS_LOCKWORD, J9UTF8_LENGTH(entryName), J9UTF8_DATA(entryName));
	}
	return FALSE;
}

/**
 * This method should be called to clean up the lockword configuration
 *
 * @param jvm pointer to J9JavaVM that can be used by the method
 */
void
cleanupLockwordConfig(J9JavaVM* jvm)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);
	
	if (jvm->lockwordExceptions != NULL){
		hashTableForEachDo(jvm->lockwordExceptions, exceptionDoDelete , PORTLIB);
		hashTableFree(jvm->lockwordExceptions);
	}
	jvm->lockwordExceptions = NULL;
}

/**
 * This method parses a specific option
 *
 * @param option the option to be parsed
 * @param what pointer to boolean set if the what option is specified
 *
 * @returns JNI_OK on success
 */
static UDATA
parseLockwordOption(J9JavaVM* jvm, char* option, BOOLEAN* what)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);

	/* first check if this is a mode specifier.  This will override any previous modes specified */
	if (strncmp(option,LOCKNURSERY_OPTION_MODE,strlen(LOCKNURSERY_OPTION_MODE))==0){
		char* modeString = strstr(option,"=") + 1;
		if (strcmp(modeString,LOCKNURSERY_MODE_DEFAULT)==0){
			jvm->lockwordMode =LOCKNURSERY_ALGORITHM_ALL_BUT_ARRAY;
			return JNI_OK;
		} else if (strcmp(modeString,LOCKNURSERY_MODE_MINIMIZE_FOOTPRINT)==0){
			jvm->lockwordMode = LOCKNURSERY_ALGORITHM_MINIMAL_AND_SYNCHRONIZED_METHODS_AND_INNER_LOCK_CANDIDATES;
			return JNI_OK;
		} else if (strcmp(modeString,LOCKNURSERY_MODE_ALL)==0){
			jvm->lockwordMode = LOCKNURSERY_ALGORITHM_ALL_INHERIT;
			return JNI_OK;
		} else {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_LOCKNURSERY_CONFIG_BAD_MODE, modeString);
			return J9VMDLLMAIN_FAILED;
		}
	}

	/* now check to see if se are being asked to display the options */
	if (strcmp(option,LOCKNURSERY_OPTION_WHAT)==0){
		*what = TRUE;
		return JNI_OK;
	}

	/* now check to see if se are being asked to reset the options */
	if (strcmp(option,LOCKNURSERY_OPTION_NONE)==0){
		cleanupLockwordConfig(jvm);
		return JNI_OK;
	}

	/* create the hashtable that holds the exceptions if it has not yet been created */
	if (jvm->lockwordExceptions == NULL){
		/* need to create the hashtable that holds the exceptions */
		jvm->lockwordExceptions = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB),
						J9_GET_CALLSITE(),
						LOCKNURSERY_INITIAL_TABLE_SIZE,
						sizeof(char*),
						0,
						0,
						OMRMEM_CATEGORY_VM,
						exceptionHashFn,
						exceptionHashEqualFn,
						NULL,
						PORTLIB);

		if (jvm->lockwordExceptions == NULL){
			/* we failed the allocation */
			return JNI_ENOMEM;
		}
	}

	/* now check if this indicates that we should exclude or include a lockword */
	if ((strncmp(option,LOCKNURSERY_OPTION_NO_LOCKWORD,strlen(LOCKNURSERY_OPTION_NO_LOCKWORD))==0)||
		(strncmp(option,LOCKNURSERY_OPTION_LOCKWORD,strlen(LOCKNURSERY_OPTION_LOCKWORD))==0))	{
		char* className = strstr(option,"=") + 1;
		size_t classNameLength = strlen(className);
		J9UTF8 *hashtableEntry;
		J9UTF8 **oldEntry;


		hashtableEntry = (J9UTF8*) j9mem_allocate_memory(classNameLength+2, OMRMEM_CATEGORY_VM);
		if (hashtableEntry == NULL){
			return JNI_ENOMEM;
		}
		memcpy((char*) &J9UTF8_DATA(hashtableEntry)[0],className,classNameLength);
		J9UTF8_SET_LENGTH(hashtableEntry, (U_16) classNameLength);

		/* now mask the entry */
		if (strncmp(option,LOCKNURSERY_OPTION_NO_LOCKWORD,strlen(LOCKNURSERY_OPTION_NO_LOCKWORD))==0){
			hashtableEntry = (J9UTF8*) SET_LOCKNURSERY_HASHTABLE_ENTRY_MASK(hashtableEntry);
		}

		/* now add the new entry, removing any existing one if it exists */
		oldEntry = hashTableFind(jvm->lockwordExceptions, &hashtableEntry);
		if (oldEntry != NULL){
			void* tempOldEntry = (void*) REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(*oldEntry);
			hashTableRemove(jvm->lockwordExceptions, &hashtableEntry);
			j9mem_free_memory(tempOldEntry);
		}

		if (NULL == hashTableAdd(jvm->lockwordExceptions, &hashtableEntry)) {
			j9mem_free_memory((void*)REMOVE_LOCKNURSERY_HASHTABLE_ENTRY_MASK(hashtableEntry));
			return JNI_ENOMEM;
		}

		return JNI_OK;
	}

	j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_LOCKNURSERY_CONFIG_BAD_OPTION, option);
	return J9VMDLLMAIN_FAILED;
}


/**
 * This method parses a string containing lockword options
 *
 * @param options string containing the options specified on the command line
 * @param what pointer to boolean set if the what option is specified
 *
 * @returns JNI_OK on success
 */
UDATA
parseLockwordConfig(J9JavaVM* jvm, char* options, BOOLEAN* what)
{
	UDATA result = JNI_OK;
	char* nextOption = NULL;
	char* cursor = options;
	PORT_ACCESS_FROM_JAVAVM(jvm);

	/* parse out each of the options */
	while((result == JNI_OK)&&(strstr(cursor, LOCKNURSERY_OPTIONS_SEPARATOR)!= NULL)) {
		nextOption = scan_to_delim(PORTLIB, &cursor, LOCKNURSERY_OPTIONS_SEPARATOR_CHAR);
		if (NULL == nextOption) {
			result = JNI_ERR;
		} else {
			result = parseLockwordOption(jvm,nextOption,what);
			j9mem_free_memory(nextOption);
		}
	}
	if (result == JNI_OK) {
		result = parseLockwordOption(jvm,cursor,what);
	}

	return result;
}

/**
 * This method is called to output the -Xlockword:what info
 *
 * @param jvm pointer to J9JavaVM that can be used by the method
 */
void
printLockwordWhat(J9JavaVM* jvm)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);

	j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_HEADER1);
	j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_HEADER2);
	if (LOCKNURSERY_ALGORITHM_ALL_INHERIT == jvm->lockwordMode) {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_MODE,LOCKNURSERY_MODE_ALL);
	} else if (LOCKNURSERY_ALGORITHM_MINIMAL_AND_SYNCHRONIZED_METHODS_AND_INNER_LOCK_CANDIDATES == jvm->lockwordMode) {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_MODE,LOCKNURSERY_MODE_MINIMIZE_FOOTPRINT);
	} else {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_LOCKNURSERY_CONFIG_WHAT_MODE,LOCKNURSERY_MODE_DEFAULT);
	}

	if (jvm->lockwordExceptions != NULL){
		hashTableForEachDo(jvm->lockwordExceptions, exceptionPrintWhat , PORTLIB);
	}
}

#endif /* defined(J9VM_THR_LOCK_NURSERY) */

