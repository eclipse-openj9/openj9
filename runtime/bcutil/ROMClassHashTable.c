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

#include "j9.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "bcutil_internal.h"

typedef struct romClassTableQueryEntry {
	J9ROMClass *romClass;
	U_8 *charData;
	UDATA length;
} romClassTableQueryEntry;

typedef union romClassTableEntry {
	UDATA tag;
	J9ROMClass* romClass;
} romClassTableEntry;

static void
romClassHashGetName(void *key, const U_8 **name, UDATA *nameLength)
{
	romClassTableEntry *entry = (romClassTableEntry*)key;

	/* We can distinguish between the different types of entries by the first slot.
	 * Query entries always have a NULL romClass field, romClasses have a non-NULL field.
	 */
	if (entry->tag == 0) {
		romClassTableQueryEntry *queryEntry = (romClassTableQueryEntry*)entry;

		*name = queryEntry->charData;
		*nameLength = queryEntry->length;
	} else {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(entry->romClass);

		*name = J9UTF8_DATA(className);
		*nameLength = J9UTF8_LENGTH(className);
	}
}

static UDATA
romClassHashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	const U_8 *leftName;
	UDATA leftLength;
	const U_8 *rightName;
	UDATA rightLength;

	romClassHashGetName(leftKey, &leftName, &leftLength);
	romClassHashGetName(rightKey, &rightName, &rightLength);

	return J9UTF8_DATA_EQUALS(leftName, leftLength, rightName, rightLength);
}

static UDATA
romClassHashFn(void *key, void *userData)
{
	const U_8 *name;
	UDATA length;
	UDATA hash;
	UDATA i;

	romClassHashGetName(key, &name, &length);

	hash = 0;
	for (i = 0; i < length; ++i) {
		hash = (hash << 5) - hash + name[i];
	}

	return hash;
}

J9HashTable*
romClassHashTableNew(J9JavaVM *vm, U_32 initialSize)
{
	J9HashTable *table;

	table = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), J9_GET_CALLSITE(), initialSize,
		sizeof(romClassTableEntry), sizeof(char *), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION,
		 J9MEM_CATEGORY_CLASSES, romClassHashFn, romClassHashEqualFn, NULL, vm);
	return table;
}

void
romClassHashTableFree(J9HashTable *hashTable)
{
	hashTableFree(hashTable);
}

UDATA
romClassHashTableAdd(J9HashTable *hashTable, J9ROMClass *value)
{
	UDATA result = 1; /* failure */
	romClassTableEntry entry;

	entry.romClass = value;
	if (NULL != hashTableAdd(hashTable, &entry)) {
		result = 0; /* success */
	}

	return result;
}

J9ROMClass*
romClassHashTableFind(J9HashTable *hashTable, U_8 *className, UDATA classNameLength)
{
	romClassTableQueryEntry entry;
	romClassTableEntry *result;

	entry.romClass = NULL;
	entry.charData = className;
	entry.length = classNameLength;
	result = (romClassTableEntry*)hashTableFind(hashTable, &entry);
	if (NULL != result) {
		return result->romClass;
	} else {
		return NULL;
	}
}

void
romClassHashTableReplace(J9HashTable *hashTable, J9ROMClass *originalClass, J9ROMClass *replacementClass)
{
	romClassTableEntry *result;
	romClassTableEntry original;

	original.romClass = originalClass;

	result = hashTableFind(hashTable, &original);
	if ( (result != NULL) && (result->romClass == originalClass)) {
		result->romClass = replacementClass;
	}
}

UDATA
romClassHashTableDelete(J9HashTable *hashTable, J9ROMClass *romClass)
{
	romClassTableEntry entry;

	entry.romClass = romClass;
	return hashTableRemove(hashTable, &entry);
}
