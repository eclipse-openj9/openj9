/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(STRING_TABLE_HPP_)
#define STRING_TABLE_HPP_

#include "BaseVirtual.hpp"

#include "ModronAssertions.h"

class MM_EnvironmentBase;

class MM_StringTable : public MM_BaseVirtual {
private:
	UDATA _tableCount;				/**< count of hash sub-tables */
	J9HashTable **_table;			/**< pointer to an array  of hash sub-tables */
	omrthread_monitor_t *_mutex;		/**< pointer to an array  of monitors associated with each hash sub-table */

	enum { cacheSize = 511 };
	j9object_t _cache[cacheSize];	/**< interned string table cash */
public:

private:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

public:

	/**
	 * @return size of interned string cache size
	 */
	static UDATA getCacheSize() { return cacheSize; }
	/**
	 * @return the address of cache (represented as an array)
	 */
	j9object_t *getStringInternCache() { return  _cache; }
	/**
	 * @param hash hash value of the string being cached
	 * @return the address of an entry into the cache array
	 */
	j9object_t *getStringInternCache(UDATA hash) { return  &_cache[hash % cacheSize]; }

	/**
	 * @return hash sub-table count
	 */
	UDATA getTableCount() { return _tableCount; }

	/* 
	 * @param hash value of a string
	 * @return tableIndex given a hash value 
	 */
	UDATA getTableIndex(UDATA  hash) {
		return hash % _tableCount;
	}
	/**
	 * @param tableIndex index of hash table into the array of sub-tables
	 * @return pointer to hash sub-table with provided index
	 */
	J9HashTable *getTable(UDATA tableIndex) { return _table[tableIndex]; }

	/**
	 * Find a string in the hash table.
	 * @param tableIndex index of hash table into the array of sub-tables
	 * @param entry pointer to a String object or pointer to a stringTableUTF8Query
	 * @return pointer to a String object or NULL in case of failure.
	 */
	j9object_t hashAt(UDATA tableIndex, j9object_t string);
	/**
	 * wrapper function to allow user to look up UTF8 strings in the hash table
	 * @param tableIndex index of hash table into the array of sub-tables
	 * @param utf8Data pointer to UTF8 string data
	 * @para utf8Length length of the string
	 */
	j9object_t hashAtUTF8(UDATA tableIndex, U_8 *utf8Data, UDATA utf8Length, U_32 hash);
	/**
	 * Add a string to the hash table.
	 * @param tableIndex index of hash table into the array of sub-tables
	 * @param string pointer to a string object.
	 * @return success (pointer to the string object) or failure (NULL)
	 */
	j9object_t hashAtPut(UDATA tableIndex, j9object_t string);

	/*
	 * Check if string is already in the string table and add if not added
	 * @param vmThread pointer to J9VMThread struct
	 * @param string object being added to String table
	 * @return pointer to existing or newly created interned string
	 */
	j9object_t addStringToInternTable(J9VMThread *vmThread, j9object_t string);

	/*
	 * Lock sub-table with provided index
	 * @param tableIndex index of hash table into the array of sub-tables
	 */
	void lockTable(UDATA tableIndex) {
		omrthread_monitor_enter(_mutex[tableIndex]);
	}

	/*
	 * Unlock sub-table with provided index
	 * @param tableIndex index of hash table into the array of sub-tables
	 */
	void unlockTable(UDATA tableIndex) {
		omrthread_monitor_exit(_mutex[tableIndex]);
	}

	static MM_StringTable *newInstance(MM_EnvironmentBase *env, UDATA tableCount);
	virtual void kill(MM_EnvironmentBase *env);
	

	MM_StringTable(MM_EnvironmentBase *env, UDATA tableCount) :
		MM_BaseVirtual(),
		_tableCount(tableCount),
		_table(NULL),
		_mutex(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
};


#endif /* STRING_TABLE_HPP_ */
