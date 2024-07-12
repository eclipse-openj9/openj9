/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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

#include "j9.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "jvminit.h"

#define ENSUREHASHED_INITIAL_TABLE_SIZE 16

extern "C" {

/**
 * @brief Function that returns the hash for an entry in the ensureHashed classes table.
 *
 * @param entry the entry
 * @param userData data specified as userData when the hashtable was created, in our case pointer to the port library
 * @returns the hash
 */
static UDATA
ensureHashedHashFn(void *entry, void *userData)
{
	J9UTF8 *hashtableEntry = *(J9UTF8 **)entry;

	return computeHashForUTF8(J9UTF8_DATA(hashtableEntry), J9UTF8_LENGTH(hashtableEntry));
}

/**
 * @brief Function that compares two entries for equality.
 *
 * @param lhsEntry the first entry to compare
 * @param rhsEntry the second entry to compare
 * @param userData data specified as userData when the hashtable was created, in our case pointer to the port library
 * @returns 1 if the entries match, 0 otherwise
 */
static UDATA
ensureHashedHashEqualFn(void *lhsEntry, void *rhsEntry, void *userData)
{
	J9UTF8 *lhsHashtableEntry = *(J9UTF8 **)lhsEntry;
	J9UTF8 *rhsHashtableEntry = *(J9UTF8 **)rhsEntry;

	return J9UTF8_DATA_EQUALS(J9UTF8_DATA(lhsHashtableEntry), J9UTF8_LENGTH(lhsHashtableEntry), J9UTF8_DATA(rhsHashtableEntry), J9UTF8_LENGTH(rhsHashtableEntry));
}

/**
 * @brief Function that is called when we are deleting an entry from the hashtable.
 *
 * @param entry the entry to be deleted
 * @param userData data specified as userData when the hashtable was created, in our case pointer to the port library
 */
static UDATA
ensureHashedDoDelete(void *entry, void *userData)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary *)userData);

	j9mem_free_memory(*(J9UTF8 **)entry);
	return TRUE;
}

/**
 * @brief This should be called to clean up the ensureHashed configuration.
 *
 * @param jvm pointer to J9JavaVM that can be used by the method
 */
void
cleanupEnsureHashedConfig(J9JavaVM *jvm)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);

	if (NULL != jvm->ensureHashedClasses) {
		hashTableForEachDo(jvm->ensureHashedClasses, ensureHashedDoDelete, PORTLIB);
		hashTableFree(jvm->ensureHashedClasses);
	}
	jvm->ensureHashedClasses = NULL;
}

/**
 * @brief This function parses a specific option.
 *
 * @param option the option to be parsed
 * @returns JNI_OK on success
 */
static UDATA
parseEnsureHashedOption(J9JavaVM *jvm, const char *option, BOOLEAN isAdd)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);
	const char *className = option;
	size_t classNameLength = strlen(className);

	if (0 == classNameLength) {
		/* Empty class name. */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_INVALID_EMPTY_OPTION, isAdd ? VMOPT_XXENABLEENSUREHASHED : VMOPT_XXDISABLEENSUREHASHED);
		return J9VMDLLMAIN_FAILED;
	}

	if (NULL == jvm->ensureHashedClasses) {
		jvm->ensureHashedClasses = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB),
				J9_GET_CALLSITE(),
				ENSUREHASHED_INITIAL_TABLE_SIZE,
				sizeof(char *),
				0,
				0,
				OMRMEM_CATEGORY_VM,
				ensureHashedHashFn,
				ensureHashedHashEqualFn,
				NULL,
				PORTLIB);

		if (NULL == jvm->ensureHashedClasses) {
			/* Allocation failed. */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_ENSUREHASHED_CONFIG_OUT_OF_MEM);
			return JNI_ENOMEM;
		}
	}

	J9UTF8 *hashtableEntry = (J9UTF8 *)j9mem_allocate_memory(classNameLength + sizeof(J9UTF8_LENGTH(hashtableEntry)), OMRMEM_CATEGORY_VM);
	if (NULL == hashtableEntry) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_ENSUREHASHED_CONFIG_OUT_OF_MEM);
		return JNI_ENOMEM;
	}
	memcpy(J9UTF8_DATA(hashtableEntry), className, classNameLength);
	J9UTF8_SET_LENGTH(hashtableEntry, (U_16)classNameLength);

	if (isAdd) {
		/* -XX:+EnsureHashed: */
		if (NULL == hashTableFind(jvm->ensureHashedClasses, &hashtableEntry)) {
			if (NULL == hashTableAdd(jvm->ensureHashedClasses, &hashtableEntry)) {
				j9mem_free_memory(hashtableEntry);
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_ENSUREHASHED_CONFIG_OUT_OF_MEM);
				return JNI_ENOMEM;
			}
		} else {
			/* Already in the table, discard the new entry. */
			j9mem_free_memory(hashtableEntry);
		}
	} else {
		/* -XX:-EnsureHashed: */
		hashTableRemove(jvm->ensureHashedClasses, &hashtableEntry);
		j9mem_free_memory(hashtableEntry);
	}

	return JNI_OK;
}

/**
 * @brief This function parses a string containing ensureHashed options.
 *
 * @param options string containing the options specified on the command line
 * @returns JNI_OK on success
 */
UDATA
parseEnsureHashedConfig(J9JavaVM *jvm, char *options, BOOLEAN isAdd)
{
	UDATA result = JNI_OK;
	char *cursor = options;
	PORT_ACCESS_FROM_JAVAVM(jvm);

	/* Parse out each of the options. */
	while (NULL != strstr(cursor, ",")) {
		char *nextOption = scan_to_delim(PORTLIB, &cursor, ',');
		if (NULL == nextOption) {
			result = JNI_ERR;
			break;
		} else {
			result = parseEnsureHashedOption(jvm, nextOption, isAdd);
			j9mem_free_memory(nextOption);
		}
	}
	if (JNI_OK == result) {
		result = parseEnsureHashedOption(jvm, cursor, isAdd);
	}

	return result;
}

} /* extern "C" */
