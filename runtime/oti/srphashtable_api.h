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

#ifndef srphashtable_api_h
#define srphashtable_api_h

/**
* @file srphashtable_api.h
* @brief Public API for the HASHTABLE module.
*
* This file contains public function prototypes and
* type definitions for the HASHTABLE module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "j9srphashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SRPHASHTABLE_NEW_ELEMENT 1
#define IS_NEW_ELEMENT(address) ((((UDATA)address) & SRPHASHTABLE_NEW_ELEMENT) != 0)
#define MARK_NEW_ELEMENT(address,type) (address = (type)(((UDATA)address) | SRPHASHTABLE_NEW_ELEMENT))
#define UNMARK_NEW_ELEMENT(address,type) (address = (type)(((UDATA)address) & (~SRPHASHTABLE_NEW_ELEMENT)))

/* ---------------- srphashtable.c ---------------- */

/**
* @brief
* @param *portLibrary
* @param *tableName
* @param tableSize
* @param entrySize
* @param hashFn
* @param hashEqualFn
* @param printFn
* @param *functionUserData
* @return J9SRPHashTable *
*/
J9SRPHashTable *
srpHashTableNew(
	J9PortLibrary *portLibrary,
	const char *tableName,
	U_32 tableSize,
	U_32 entrySize,
	U_32 flags,
	J9SRPHashTableHashFn hashFn,
	J9SRPHashTableEqualFn hashEqualFn,
	J9SRPHashTablePrintFn printFn,
	void *functionUserData);


/**
 * @brief
 * @param *portLibrary
 * @param *tableName
 * @param *address
 * @param memorySize
 * @param entrySize
 * @param flags
 * @param hashFn
 * @param hashEqualFn
 * @param printFn
 * @param *functionUserData
 * @return J9SRPHashTable *
 */
J9SRPHashTable *
srpHashTableNewInRegion(
	J9PortLibrary *portLibrary,
	const char *tableName,
	void *address,
	U_32 memorySize,
	U_32 entrySize,
	U_32 flags,
	J9SRPHashTableHashFn hashFn,
	J9SRPHashTableEqualFn hashEqualFn,
	J9SRPHashTablePrintFn printFn,
	void *functionUserData);

/**
 * @brief
 * @param *portLibrary
 * @param *tableName
 * @param *srpHashTable
 * @param *address
 * @param memorySize
 * @param entrySize
 * @param flags
 * @param hashFn
 * @param hashEqualFn
 * @param printFn
 * @param *functionUserData
 * @return J9SRPHashTable *
 */
J9SRPHashTable *
srpHashTableReset(
		J9PortLibrary *portLibrary,
		const char *tableName,
		J9SRPHashTable *srpHashTable,
		void * address,
		U_32 memorySize,
		U_32 entrySize,
		U_32 flags,
		J9SRPHashTableHashFn hashFn,
		J9SRPHashTableEqualFn hashEqualFn,
		J9SRPHashTablePrintFn printFn,
		void *functionUserData);

/**
 * @brief
 * @param *portLibrary
 * @param *tableName
 * @param *address
 * @param hashFn
 * @param hashEqualFn
 * @param printFn
 * @param *functionUserData
 * @return J9SRPHashTable *
 */
J9SRPHashTable *
srpHashTableRecreate(
	J9PortLibrary *portLibrary,
	const char *tableName,
	void *address,
	J9SRPHashTableHashFn hashFn,
	J9SRPHashTableEqualFn hashEqualFn,
	J9SRPHashTablePrintFn printFn,
	void *functionUserData);


/**
* @brief
* @param *table
* @return void
*/
void
srpHashTableFree(J9SRPHashTable *srptable);



/**
* @brief
* @param *table
* @param *entry
* @return void *
*/
void *
srpHashTableFind(J9SRPHashTable *srptable, void *entry);

/**
* @brief
* @param *table
* @param *entry
* @return void *
*/
void *
srpHashTableAdd(J9SRPHashTable *srptable, void *entry);

/**
* @brief
* @param *srptable
* @param *entry
* @return U_32
*/
U_32
srpHashTableRemove(J9SRPHashTable *srptable, void *entry);

/**
* @brief
* @param *table
* @return void
*/
void
srpHashTableDumpDistribution(J9SRPHashTable *srptable);


/**
* @brief
* @param *table
* @param doFn
* @param *opaque
* @return void
*/
void
srpHashTableForEachDo(J9SRPHashTable *srptable, J9SRPHashTableDoFn doFn, void *opaque);

/**
* @brief
* @param *srptable
* @param *bucketIndex
* @return void
*/
void *
srpHashTableStartDo(J9SRPHashTable *srptable, UDATA * bucketIndex);

/**
* @brief
* @param *srptable
* @param *bucketIndex
* @param *currentNode
* @return void
*/
void  *
srpHashTableNextDo(J9SRPHashTable *srptable, U_32 *bucketIndex, void * currentNode);


/**
* @brief
* @param *table
* @return U_32
*/
U_32
srpHashTableGetCount(J9SRPHashTable *srptable);

/**
* @brief
* @param *srptable
* @param memorySize
* @param entrySize
* @return BOOLEAN
*/
BOOLEAN
srpHashTableVerify(
	J9SRPHashTable * srptable,
	U_32 memorySize,
	U_32 entrySize);
/**
 * @brief
 * @param srptable
 * @param portLib
 * @param doFunction
 * @param userData
 * @param skipCount
 * @return BOOLEAN
 */
BOOLEAN
srpHashTable_checkConsistency(J9SRPHashTable* srptable,
		J9PortLibrary *portLib,
		BOOLEAN (*doFunction) (void* anElement, void* userData),
		void* userData,
		UDATA skipCount);

/**
 * @brief
 * @param tableSize
 * @param entrySize
 * @param ceilUp
 * @return U_32
 */
U_32
srpHashTable_requiredMemorySize(U_32 tableSize, U_32 entrySize, BOOLEAN ceilUp);

/**
 * @brief
 * @param memorySize
 * @param entrySize
 * @param canExtendGivenMemory
 * @return U_32
 */

U_32
srpHashTable_calculateTableSize(U_32 memorySize, U_32 entrySize, BOOLEAN canExtendGivenMemory);

/**
 * @brief
 * @param srptable
 * @return U_32
 */
U_32
srpHashTable_tableSize(J9SRPHashTable *srptable);


/**
 * @brief
 * @param srptable
 * @param numberOfNodesToPrint
 * @return void
 */
void
srpHashTablePrintDebugInfo(J9SRPHashTable *srptable, UDATA numberOfNodesToPrint);

#ifdef __cplusplus
}
#endif

#endif /* srphashtable_api_h */

