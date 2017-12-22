/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

/*
 * StringInternTable.hpp
 */

#ifndef STRINGINTERNTABLE_HPP_
#define STRINGINTERNTABLE_HPP_

/* @ddr_namespace: default */
#include "j9.h"

struct J9InternHashTableEntry;

/**
 *
 * Shared cache(s) can be SC_COMPLETELY_OUT_OF_THE_SRP_RANGE if and only if all start and end addresses
 * 		of shared cache(s) are out of the range of a specific address in the memory.
 * Shared cache(s) can be SC_COMPLETELY_IN_THE_SRP_RANGE if and only if all start and end addresses
 * 		of shared cache(s) are in the range of a specific address in the memory.
 * In all other cases, shared cache(s) are SC_PARTIALLY_IN_THE_SRP_RANGE.
 * 		In this case, range check between start and end addressed of shared cache(s) results in
 * 		combination of SC_COMPLETELY_OUT_OF_THE_SRP_RANGE and SC_COMPLETELY_IN_THE_SRP_RANGE.
 * 		That is the reason why these values are bitwise, all the range check results between start/end addresses of shared cache(s)
 * 		and a specific memory address are ORed.
 *
 */
enum SharedCacheRangeInfo
{
	SC_NO_RANGE_INFO = 0,
	SC_COMPLETELY_OUT_OF_THE_SRP_RANGE  = 1,
	SC_COMPLETELY_IN_THE_SRP_RANGE = 2,
	SC_PARTIALLY_IN_THE_SRP_RANGE = 3
};

typedef struct J9InternSearchResult {
	J9UTF8 *utf8;
	void *node;
	bool isSharedNode;
} J9InternSearchResult;

typedef struct J9InternSearchInfo {
    struct J9ClassLoader* classloader;
    U_8* stringData;
    UDATA stringLength;
    U_8* romClassBaseAddr;
    U_8* romClassEndAddr;
    SharedCacheRangeInfo sharedCacheSRPRangeInfo;
} J9InternSearchInfo;

class StringInternTable
{
public:
	StringInternTable(J9JavaVM *vm, J9PortLibrary *portLibrary, UDATA maximumNodeCount);
	~StringInternTable();

	J9JavaVM *javaVM() const { return _vm; }

	bool isOK() const { return (0 == _maximumNodeCount) || (NULL != _internHashTable); }

	bool findUtf8(J9InternSearchInfo *searchInfo, J9SharedInvariantInternTable *sharedInternTable, bool requiresSharedUtf8, J9InternSearchResult *result);

	void markNodeAsUsed(J9InternSearchResult *result, J9SharedInvariantInternTable *sharedInternTable);

	void internUtf8(J9UTF8 *utf8, J9ClassLoader *classLoader, bool fromSharedROMClass = false, J9SharedInvariantInternTable *sharedInternTable = NULL);

	void removeLocalNodesWithDeadClassLoaders();

	J9InternHashTableEntry * getLRUHead() const { return _headNode; }

	bool verify(const char *file, IDATA line) const;

#if defined(J9VM_ENV_DATA64)
	/**
	 *	This function checks whether two given addresses are in the SRP range to each other or not.
	 *	This check is done only for 64 bit environments.
	 *	@param addr1	First given address.
	 *	@param addr2	Second given address.
	 *	@return true if two addresses are in the SRP range to each other,
	 *	@return false; otherwise.
	 *
	 */
	static bool
	areAddressesInSRPRange(void *addr1, void *addr2)
	{
		UDATA rangeCheck = (addr1 > addr2) ? (UDATA)addr1 - (UDATA)addr2 : (UDATA)addr2 - (UDATA)addr1;
		if (0x7FFFFFFF < rangeCheck) {
			return false;
		}
	return true;
	};
#endif

private:

	/* NOTE: Be sure to update J9DbgStringInternTable when changing the state variables below. */
	J9JavaVM *_vm;
	J9PortLibrary *_portLibrary;
	J9HashTable *_internHashTable;
	J9InternHashTableEntry *_headNode;
	J9InternHashTableEntry *_tailNode;
	UDATA _nodeCount;
	UDATA _maximumNodeCount;

	J9InternHashTableEntry * insertLocalNode(J9InternHashTableEntry *node, bool promoteIfExistingFound);
	void deleteLocalNode(J9InternHashTableEntry *node);

	void promoteNodeToHead(J9InternHashTableEntry *node);
	void removeNodeFromList(J9InternHashTableEntry *node);

	bool verifyNode(J9InternHashTableEntry *node, const char *file, IDATA line) const;

#if defined(J9VM_OPT_SHARED_CLASSES)

	J9SharedInternSRPHashTableEntry * insertSharedNode(J9SharedInvariantInternTable *table, J9UTF8 *utf8, U_16 internWeight, U_16 flags, bool promoteIfExistingFound);

	void deleteSharedNode(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *node);
	void removeSharedNodeFromList(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *sharedNode);

	UDATA getRequiredBytesForUTF8Length(U_16 length);
	void updateLocalNodeWeight(J9InternHashTableEntry *node);
	void updateSharedNodeWeight(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *sharedNode);

	bool testNodePromotionWeight(J9SharedInvariantInternTable *table, J9InternHashTableEntry *node, J9SharedInternSRPHashTableEntry *sharedNodeToDisplace);

	void swapLocalNodeWithTailSharedNode(J9InternHashTableEntry *node, J9SharedInvariantInternTable *table);
	void promoteSharedNodeToHead(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *node);

#endif /* J9VM_OPT_SHARED_CLASSES */
};

#endif /* STRINGINTERNTABLE_HPP_ */
