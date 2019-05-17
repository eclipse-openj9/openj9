/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

#if !defined(J9SC_STRING_INTERN_TYPES_H_INCLUDED)
#define J9SC_STRING_INTERN_TYPES_H_INCLUDED

#include "j9.h"
#include "srphashtable_api.h"


/* Query struct for searching the shared string intern srp hash table. */
typedef struct J9SharedInternHashTableQuery {
	UDATA length;
	U_8 *data;
} J9SharedInternHashTableQuery;

/**
 * Perform a specified action on a node
 *
 * This helper is provided for external modules (outside of j9dyn) that need to perform
 * operations on J9InternAVLLRU structs.
 *
 * @param[in] table  The J9SharedInvariantInternTable in use. Must be a J9SharedInvariantInternTable.
 * @param[in] node  A node
 * @param[in] action  The action to perform
 * @param[in] userData  optional user data
 *
 * @return value expected by the given action
 */
static UDATA
sharedInternTable_performNodeAction(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *entry, UDATA action, void *userData)
{
	switch (action) {
	case STRINGINTERNTABLES_ACTION_VERIFY_BOTH_TABLES :
	{
		/* TODO:FATIH:verify trees :Most probably we wont need performNodeAction and it will be removed */
		break;
	}
	case STRINGINTERNTABLES_ACTION_VERIFY_LOCAL_TABLE_ONLY :
	{
		/* TODO:FATIH:verify local tree : Most probably we wont need performNodeAction and it will be removed */
		break;
	}

	default:
		return 0;
	}
	return 0;
}

/*
 * Key is the user passed search key.
 * existingEntry is the SRP Hashtable entry with the hash calculated by using the given key
 * @param existingEntry Entry in the SRP hashtable
 * @param key is compared with the existingEntry.
 * @return 0, meaning false, if key does not match with the existingEntry,
 * 		   1, otherwise
 *
 */
static UDATA
sharedInternHashEqualFn(void *existingEntry, void *key, void *userData)
{
	UDATA existingUtf8Length, keyUtf8Length;
	U_8 *existingUtf8Data, *keyUtf8Data;

	J9SharedInternHashTableQuery *query = (J9SharedInternHashTableQuery*)key;
	keyUtf8Length = query->length;
	keyUtf8Data = query->data;

	J9UTF8 *existing_utf8 = SRP_GET(((J9SharedInternSRPHashTableEntry *)existingEntry)->utf8SRP, J9UTF8 *);
	if (NULL != existing_utf8) {
		existingUtf8Length = J9UTF8_LENGTH(existing_utf8);
		existingUtf8Data = J9UTF8_DATA(existing_utf8);
	} else {
		/* We should never be here */
		/* Trc_SHR_Assert_ShouldNeverHappen(); */
		return 0;
	}

	return J9UTF8_DATA_EQUALS(existingUtf8Data, existingUtf8Length, keyUtf8Data, keyUtf8Length);
}

/**
 * Calculates the hash value for the given key and userData
 * @param key Either it is J9UTF8 * or J9SharedInternHashTableQuery * which has the utf8 data and length in it.
 * @param userData
 * return UDATA Hash Value
 */
static UDATA
sharedInternHashFn(void *key, void *userData)
{
	UDATA length;
	U_8 *data;

	J9SharedInternHashTableQuery *query = (J9SharedInternHashTableQuery*)key;
	length = query->length;
	data = query->data;

	UDATA hash = 0;
	for (UDATA i = 0; i < length; i++) {
		hash = (hash << 5) - hash + data[i];
	}

	return hash;
}




#endif /*J9SC_STRING_INTERN_TYPES_H_INCLUDED*/
