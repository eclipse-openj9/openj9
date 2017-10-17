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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdlib.h>
#include "j9.h"
#include "j9consts.h"
#include "j9protos.h"
#include "vm_internal.h"
#include "ut_j9vm.h"

#define TYPE_CLASS ((UDATA)0)
#define TYPE_PACKAGEID ((UDATA)-1)
#define TYPE_UNICODE ((UDATA)2)

#define ROUNDING_GRANULARITY	4
#define ROUNDED_BYTE_AMOUNT(number)	 (((number) + (ROUNDING_GRANULARITY - 1)) & ~(UDATA)(ROUNDING_GRANULARITY - 1))


typedef union classTableEntry {
	UDATA tag;
	J9Class* ramClass;
	J9PackageIDTableEntry packageID;
	J9UTF8* className;
} classTableEntry;

typedef struct classTableQueryEntry {
	classTableEntry entry;
	U_8 *charData;
	UDATA length;
} classTableQueryEntry;

static UDATA classHashFn (void *key, void *userData);
static UDATA classHashEqualFn (void *leftKey, void *rightKey, void *userData);
static UDATA classHashGetName(classTableEntry* entry, const U_8** name, UDATA* nameLength);

static UDATA classLocationHashFn (void *key, void *userData);
static UDATA classLocationHashEqualFn (void *leftKey, void *rightKey, void *userData);

static void addLocationGeneratedClass(J9VMThread *vmThread, J9ClassLoader *classLoader, classTableEntry *existingPackageEntry, IDATA entryIndex, I_32 locationType);

static UDATA
classHashGetName(classTableEntry* entry, const U_8** name, UDATA* nameLength)
{
	UDATA type = TYPE_CLASS;
	UDATA tag = entry->tag;

	if (TAG_RAM_CLASS == (tag & MASK_RAM_CLASS)) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(entry->ramClass->romClass);

		*name = J9UTF8_DATA(className);
		*nameLength = J9UTF8_LENGTH(className);
	} else if ((TAG_UTF_QUERY == (tag & MASK_QUERY))
		|| (TAG_PACKAGE_UTF_QUERY == (tag & MASK_QUERY))
	) {
		classTableQueryEntry* queryEntry = (classTableQueryEntry*)entry;

		*name = queryEntry->charData;
		*nameLength = queryEntry->length;
	} else if (TAG_UNICODE_QUERY == (tag & MASK_QUERY)) {
		classTableQueryEntry* queryEntry = (classTableQueryEntry*)entry;

		*name = (const U_8 *)queryEntry->charData;
		type = TYPE_UNICODE;
	} else if (J9_ARE_ANY_BITS_SET(tag, MASK_ROM_CLASS)) {
		*name = getPackageName(&entry->packageID, nameLength);
		type = TYPE_PACKAGEID;
	} else {
		Assert_VM_unreachable();
	}

	return type;
}

static UDATA  
classHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	UDATA tableNodeLength;
	UDATA queryNodeLength;
	const U_8* tableNodeName = NULL;
	const U_8* queryNodeName = NULL;
	UDATA tableNodeType;
	UDATA queryNodeType;
	J9JavaVM* javaVM = (J9JavaVM*) userData;

	tableNodeType = classHashGetName(tableNode, &tableNodeName, &tableNodeLength);
	queryNodeType = classHashGetName(queryNode, &queryNodeName, &queryNodeLength);

	if (queryNodeType == TYPE_UNICODE) {
		if (tableNodeType == TYPE_CLASS) {
			j9object_t stringObject = (j9object_t) queryNodeName;
			j9object_t charArray = J9VMJAVALANGSTRING_VALUE_VM(javaVM, stringObject);
			U_32 i = 0;
			U_32 end = J9VMJAVALANGSTRING_LENGTH_VM(javaVM, stringObject);

			BOOLEAN isCompressed = IS_STRING_COMPRESSED_VM(javaVM, stringObject);

			while (i < end) {
				U_16 unicode;
				U_16 utf;
				U_8 c;

				if (isCompressed) {
					unicode = J9JAVAARRAYOFBYTE_LOAD_VM(javaVM, charArray, i);
				} else {
					unicode = J9JAVAARRAYOFCHAR_LOAD_VM(javaVM, charArray, i);
				}

				if (tableNodeLength == 0) {
					return FALSE;
				}

				c = *(tableNodeName++);
				if ((c & 0x80) == 0x00) {
					/* one byte encoding */

					utf = (U_16)c;
					tableNodeLength -= 1;
				} else if ((c & 0xE0) == 0xC0) {
					/* two byte encoding */

					utf = ((U_16)c & 0x1F) << 6;
					c = *(tableNodeName++);
					utf += (U_16)c & 0x3F;
					tableNodeLength -= 2;
				} else {
					/* three byte encoding */

					utf = ((U_16)c & 0x0F) << 12;
					c = *(tableNodeName++);
					utf += ((U_16)c & 0x3F) << 6;
					c = *(tableNodeName++);
					utf += (U_16)c & 0x3F;
					tableNodeLength -= 3;
				}

				if (unicode != utf) {
					if ((unicode != '.') || (utf != '/')) {
						return FALSE;
					}
				}

				++i;
			}
			return tableNodeLength == 0;
		}
		return FALSE;
	}

	/*
	 * queryNodeType must equal tableNodeType
	 */

	if (tableNodeType != queryNodeType) {
		return FALSE;
	}

	Assert_VM_true(TAG_PACKAGE_UTF_QUERY != ((classTableEntry *) tableNode)->tag);

	if (TAG_PACKAGE_UTF_QUERY == ((classTableEntry *) queryNode)->tag) {
		if (queryNodeLength < tableNodeLength) {
			char * className = strrchr((char*)tableNodeName, '/');
			return J9UTF8_DATA_EQUALS(tableNodeName, (UDATA) className - (UDATA) tableNodeName, queryNodeName, queryNodeLength);
		} 
		return FALSE;
	} 
	return J9UTF8_DATA_EQUALS(tableNodeName, tableNodeLength, queryNodeName, queryNodeLength);
}


static UDATA 
classHashFn(void *key, void *userData) 
{
	J9JavaVM* javaVM = (J9JavaVM*) userData;

	UDATA type;
	UDATA length = 0;
	const U_8* name = NULL;
	U_32 hash;

	type = classHashGetName(key, &name, &length);
	if (type == TYPE_UNICODE) {
		j9object_t stringObject = (j9object_t) name;

		hash = J9VMJAVALANGSTRING_HASHCODE_VM(javaVM, stringObject);
		if (0 == hash) {
			j9object_t charArray = J9VMJAVALANGSTRING_VALUE_VM(javaVM, stringObject);
			U_32 i = 0;
			U_32 end = i + J9VMJAVALANGSTRING_LENGTH_VM(javaVM, stringObject);

			if (IS_STRING_COMPRESSED_VM(javaVM, stringObject)) {
				while (i < end) {
					hash = (hash << 5) - hash + J9JAVAARRAYOFBYTE_LOAD_VM(javaVM, charArray, i);
					++i;
				}
			} else {
				while (i < end) {
					hash = (hash << 5) - hash + J9JAVAARRAYOFCHAR_LOAD_VM(javaVM, charArray, i);
					++i;
				}
			}

			J9VMJAVALANGSTRING_SET_HASHCODE_VM(javaVM, stringObject, hash);
		}
		type = TYPE_CLASS;
	} else {
		hash = 0;
		while (length != 0) {
			U_8 c;
			U_16 unicodeChar;

			c = *(name++);
			if ((c & 0x80) == 0x00) {
				/* one byte encoding */

				unicodeChar = (U_16)c;
				length -= 1;
			} else if ((c & 0xE0) == 0xC0) {
				/* two byte encoding */

				unicodeChar = ((U_16)c & 0x1F) << 6;
				c = *(name++);
				unicodeChar += (U_16)c & 0x3F;
				length -= 2;
			} else {
				/* three byte encoding */

				unicodeChar = ((U_16)c & 0x0F) << 12;
				c = *(name++);
				unicodeChar += ((U_16)c & 0x3F) << 6;
				c = *(name++);
				unicodeChar += (U_16)c & 0x3F;
				length -= 3;
			}

			/* Make the String and internal representations of the class name consistent */

			if ('/' == unicodeChar) {
				unicodeChar = '.';
			}
			hash = (hash << 5) - hash + unicodeChar;
		}
	}

	if (TYPE_PACKAGEID == type) {
		hash = hash ^ (U_32)type;
	}

	return hash;
}


J9HashTable *
hashClassTableNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	/* If -XX:+FastClassHashTable is enabled, do not allow hash tables to grow automatically */
	if (J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FAST_CLASS_HASH_TABLE)) {
		flags |= J9HASH_TABLE_DO_NOT_GROW;
	}

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(classTableEntry), sizeof(char *), flags, J9MEM_CATEGORY_CLASSES, classHashFn, classHashEqualFn, NULL, javaVM);
}


J9Class *
hashClassTableAt(J9ClassLoader* classLoader, U_8 *className, UDATA classNameLength)
{
	J9HashTable *table = classLoader->classHashTable;
	classTableQueryEntry key;
	classTableEntry* result;

	key.entry.tag = TAG_UTF_QUERY;
	key.charData = className;
	key.length = classNameLength;
	result = hashTableFind(table, &key);
	if (result) {
		return result->ramClass;
	} else {
		return NULL;
	}
}

BOOLEAN
isAnyClassLoadedFromPackage(J9ClassLoader* classLoader, U_8 *pkgName, UDATA pkgNameLength)
{
	J9HashTable *table = classLoader->classHashTable;
	classTableQueryEntry key;
	classTableEntry* result;

	key.entry.tag = TAG_PACKAGE_UTF_QUERY;
	key.charData = pkgName;
	key.length = pkgNameLength;
	result = hashTableFind(table, &key);
	if (NULL != result) {
		return TRUE;
	}
	return FALSE;
}


void
hashClassTableFree(J9ClassLoader* classLoader)
{
	J9HashTable *table = classLoader->classHashTable;


	/* Free all the chained hash tables */
	while (NULL != table) {
		J9HashTable *previous = table->previous;
		hashTableFree(table);
		table = previous;
	}

	classLoader->classHashTable = NULL;
}


static classTableEntry *
growClassHashTable(J9JavaVM *vm, J9ClassLoader *classLoader, classTableEntry *newEntry)
{
	classTableEntry *node = NULL;
	/* If -XX:+FastClassHashTable is enabled, attempt to allocate a new, larger hash table, otherwise return failure */
	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FAST_CLASS_HASH_TABLE)) {
		J9HashTable *oldTable = classLoader->classHashTable;
		J9HashTable *newTable = hashTableNew(oldTable->portLibrary, J9_GET_CALLSITE(), oldTable->tableSize + 1, sizeof(classTableEntry), sizeof(char *), J9HASH_TABLE_DO_NOT_GROW | J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, classHashFn, classHashEqualFn, NULL, vm);
		if (NULL != newTable) {
			J9HashTableState walkState;
			classTableEntry *oldNode = NULL;
			/* Copy all of the data from the old hash table into the new one */
			oldNode = hashTableStartDo(oldTable,  &walkState);
			while (NULL != oldNode) {
				if (NULL == hashTableAdd(newTable, oldNode)) {
					hashTableFree(newTable);
					return NULL;
				}
				oldNode = hashTableNextDo(&walkState);
			}
			node = hashTableAdd(newTable, newEntry);
			if (NULL == node) {
				hashTableFree(newTable);
				return NULL;
			}
			newTable->previous = oldTable;
			vm->freePreviousClassLoaders = TRUE;
			issueWriteBarrier();
			classLoader->classHashTable = newTable;
		}
	}
	return node;
}


UDATA
hashClassTableAtPut(J9VMThread* vmThread, J9ClassLoader* classLoader, U_8 *className, UDATA classNameLength, J9Class *value)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9HashTable *table = classLoader->classHashTable;
	classTableEntry *node = NULL;
	classTableEntry entry;

	if (NULL == node) {
		entry.ramClass = value;
		node = hashTableAdd(table, &entry);

		if (NULL == node) {
			if (NULL == growClassHashTable(vm, classLoader, &entry)) {
				return 1;
			}
		}
	}

	/* Issue a GC write barrier when modifying the class hash table */
	vm->memoryManagerFunctions->j9gc_objaccess_postStoreClassToClassLoader(vmThread, classLoader, value);
	return 0;
}


UDATA
hashClassTableDelete(J9ClassLoader* classLoader, U_8 *className, UDATA classNameLength)
{
	J9HashTable *table = classLoader->classHashTable;
	classTableQueryEntry key;

	key.entry.tag = TAG_UTF_QUERY;
	key.charData = className;
	key.length = classNameLength;
	return hashTableRemove(table, &key);
}


void
hashClassTableReplace(J9VMThread* vmThread, J9ClassLoader *classLoader, J9Class *originalClass, J9Class *replacementClass)
{
	J9HashTable* table = classLoader->classHashTable;
	classTableEntry* result;
	classTableQueryEntry original;

	original.entry.ramClass = originalClass;

	result = hashTableFind(table, &original);
	if ( (result != NULL) && (result->ramClass == originalClass)) {
		result->ramClass = replacementClass;
		/* Issue a GC write barrier when modifying the class hash table */
		vmThread->javaVM->memoryManagerFunctions->j9gc_objaccess_postStoreClassToClassLoader(vmThread, classLoader, replacementClass);
	}
}

J9Class*
hashClassTableStartDo(J9ClassLoader* classLoader, J9HashTableState* walkState)
{
	classTableEntry* first = hashTableStartDo(classLoader->classHashTable, walkState);

	/* only report RAM classes */
	while ( (first != NULL) && (TAG_RAM_CLASS != (first->tag & MASK_RAM_CLASS)) ) {
		first = hashTableNextDo(walkState);
	}

	return (first == NULL) ? NULL : first->ramClass;
}

J9Class*
hashClassTableNextDo(J9HashTableState* walkState)
{
	classTableEntry* next = hashTableNextDo(walkState);

	/* only report RAM classes */
	while ( (next != NULL) && (TAG_RAM_CLASS != (next->tag & MASK_RAM_CLASS)) ) {
		next = hashTableNextDo(walkState);
	}

	return (next == NULL) ? NULL : next->ramClass;
}


/**
 * Find the package ID for the given name and length.
 * NOTE: You must own the class table mutex before calling this function.
 */
UDATA
hashPkgTableIDFor(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass* romClass, IDATA entryIndex, I_32 locationType)
{
	classTableEntry key;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9HashTable *table = classLoader->classHashTable;
	UDATA packageNameLength = 0;
	BOOLEAN isSystemClassLoader = (classLoader == javaVM->systemClassLoader);

	key.tag = (UDATA)romClass | TAG_ROM_CLASS;

	if (isSystemClassLoader && (J9ROMCLASS_IS_UNSAFE(romClass) || (LOAD_LOCATION_UNKNOWN == locationType))) {
		key.tag |= TAG_GENERATED_PACKAGE;
	}

	getPackageName(&key.packageID, &packageNameLength);
	if (packageNameLength == 0) {
		/* default package IDs don't go in the table. This prevents them from being returned by JVM_GetSystemPackages */
		return (UDATA)classLoader;
	} else {
		UDATA packageID = 0;
		classTableEntry *result = hashTableAdd(table, &key);
		if (NULL == result) {
			result = growClassHashTable(javaVM, classLoader, &key);
		}
		if (NULL != result) {
			packageID = *(UDATA *)result;
			if (isSystemClassLoader && J9_ARE_ALL_BITS_SET(result->tag, TAG_GENERATED_PACKAGE)) {
				if (J9_ARE_NO_BITS_SET(key.tag, TAG_GENERATED_PACKAGE)) {
					/* Below function removes the TAG_GENERATED_PACKAGE bit from the hash table entry after adding
					 * a location for the generated class.
					 */
					addLocationGeneratedClass(vmThread, classLoader, result, entryIndex, locationType);
				}
				/* Mask out TAG_GENERATED_PACKAGE so it is not included in the packageID stored in the RAM classes */
				packageID &= ~(UDATA)TAG_GENERATED_PACKAGE;
			}
		}
		return packageID;
	}
}

J9PackageIDTableEntry *
hashPkgTableAt(J9ClassLoader* classLoader, J9ROMClass* romClass)
{
	classTableEntry key;
	J9HashTable *table = classLoader->classHashTable;
	UDATA packageNameLength = 0;
	classTableEntry* result = NULL;

	key.tag = (UDATA)romClass | TAG_ROM_CLASS;

	getPackageName(&key.packageID, &packageNameLength);
	if (0 != packageNameLength) {
		result = hashTableFind(table, &key);
	}
	return (NULL != result) ? &result->packageID: NULL;
}

J9PackageIDTableEntry*
hashPkgTableStartDo(J9ClassLoader* classLoader, J9HashTableState* walkState)
{
	classTableEntry* first = hashTableStartDo(classLoader->classHashTable, walkState);

	/* examine only package IDs */
	while ( (first != NULL) && (TAG_ROM_CLASS != (first->tag & MASK_ROM_CLASS)) ) {
		first = hashTableNextDo(walkState);
	}

	return (first == NULL) ? NULL : &first->packageID;
}

J9PackageIDTableEntry*
hashPkgTableNextDo(J9HashTableState* walkState)
{
	classTableEntry* next = hashTableNextDo(walkState);

	/* examine only package IDs */
	while ( (next != NULL) && (TAG_ROM_CLASS != (next->tag & MASK_ROM_CLASS)) ) {
		next = hashTableNextDo(walkState);
	}

	return (next == NULL) ? NULL : &next->packageID;
}


J9Class *
hashClassTableAtString(J9ClassLoader* classLoader, j9object_t stringObject)
{
	J9HashTable *table = classLoader->classHashTable;
	classTableQueryEntry key;
	classTableEntry* result;

	key.entry.tag = TAG_UNICODE_QUERY;
	key.charData = (U_8 *) stringObject;
	result = hashTableFind(table, &key);
	if (NULL != result) {
#if 0
		/* TODO stefanbu This path appears deprecated, userData was previously cast to J9VMToken. */
		void * userData = table->functionUserData;
		J9UTF8 * foundName = J9ROMCLASS_CLASSNAME(result->ramClass->romClass);
		j9object_t charArray = J9VMJAVALANGSTRING_VALUE((J9VMThread*)userData, stringObject);
		U_32 i = 0;
		U_32 end = i + J9VMJAVALANGSTRING_LENGTH((J9VMThread*)userData, stringObject);

		printf("looking for ");
		while (i < end) {
			U_16 c = J9JAVAARRAYOFCHAR_LOAD((J9VMThread*)userData, charArray, i);
			printf("%c", c);
			++i;
		}
		printf(" and found %.*s\n", J9UTF8_LENGTH(foundName), J9UTF8_DATA(foundName));
#endif
		return result->ramClass;
	}
	return NULL;
}

J9HashTable *
hashClassLocationTableNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(J9ClassLocation), sizeof(char *), flags, J9MEM_CATEGORY_CLASSES, classLocationHashFn, classLocationHashEqualFn, NULL, javaVM);
}


static UDATA
classLocationHashFn (void *key, void *userData)
{
	J9ClassLocation *entry = (J9ClassLocation *)key;

	return (UDATA)entry->clazz;
}

static UDATA
classLocationHashEqualFn (void *leftKey, void *rightKey, void *userData)
{
	J9ClassLocation *leftNode = (J9ClassLocation *)leftKey;
	J9ClassLocation *rightNode = (J9ClassLocation *)rightKey;

	return leftNode->clazz == rightNode->clazz;
}

/**
 * @brief If a generated class is loaded from a package, then package information should not be available. If a non-generated
 * class is loaded from the same package at a later point, then package information should be available. In order to make
 * package information available, this function adds a class location for the generated class in the classLocationHashTable,
 * which points at the entryIndex of the non-generated class. Then, it also removes the TAG_GENERATED_PACKAGE from the existing
 * package entry of the generated class.
 * @param *vmThread J9VMThread instance
 * @param *classLoader Classloader for the class
 * @param *existingPackageEntry Package entry for the generated class i.e. ROM classs
 * @param entryIndex classpath index of the non-generated class
 * @param locationType location type of non-generated class
 * @return void
 */
static void
addLocationGeneratedClass(J9VMThread *vmThread, J9ClassLoader *classLoader, classTableEntry *existingPackageEntry, IDATA entryIndex, I_32 locationType)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *funcs = javaVM->internalVMFunctions;
	J9ROMClass *resultROMClass = (J9ROMClass *)(existingPackageEntry->tag & ~(UDATA)(TAG_ROM_CLASS | TAG_GENERATED_PACKAGE));
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(resultROMClass);
	J9Class *clazz = NULL;

	clazz = funcs->hashClassTableAt(classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
	if (NULL != clazz) {
		J9ClassLocation *classLocation = NULL;
		J9ClassLocation newLocation = {0};

		omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
		classLocation = funcs->findClassLocationForClass(vmThread, clazz);

		if (NULL == classLocation) {
			I_32 newLocationType = 0;
			if (LOAD_LOCATION_PATCH_PATH == locationType) {
				newLocationType = LOAD_LOCATION_PATCH_PATH_NON_GENERATED;
			} else if (LOAD_LOCATION_CLASSPATH == locationType) {
				newLocationType = LOAD_LOCATION_CLASSPATH_NON_GENERATED;
			} else if (LOAD_LOCATION_MODULE == locationType) {
				newLocationType = LOAD_LOCATION_MODULE_NON_GENERATED;
			} else {
				Assert_VM_unreachable();
			}

			newLocation.clazz = clazz;
			newLocation.entryIndex = entryIndex;
			newLocation.locationType = newLocationType;

			/* LOAD_LOCATION_UNKNOWN entries (with entryIndex == -1) are not stored in the classLocationHashtable.
			 * So, we don't need to remove a hash table entry before adding the new one.
			 */
			hashTableAdd(classLoader->classLocationHashTable, (void *)&newLocation);
		} else {
			/* In case of multi-threaded class loading, we must avoid adding an entry if it already exists. But, we
			 * should make sure that the existing entry has locationType for non-generated class i.e. (locationType < 0).
			 */
			Assert_VM_true(classLocation->locationType < 0);
		}
		omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);

		existingPackageEntry->tag &= ~(UDATA)TAG_GENERATED_PACKAGE;
	}
}

J9ClassLocation *
findClassLocationForClass(J9VMThread *currentThread, J9Class *clazz)
{
	J9ClassLocation classLocation = {0};
	J9ClassLocation *targetPtr = NULL;

	if (NULL == clazz->classLoader->classLocationHashTable) {
		return NULL;
	}

	Assert_VM_mustOwnMonitor(currentThread->javaVM->classLoaderModuleAndLocationMutex);

	classLocation.clazz = clazz;
	targetPtr = hashTableFind(clazz->classLoader->classLocationHashTable, (void *)&classLocation);

	return targetPtr;
} 
