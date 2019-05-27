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
/*
 * StringInternTable.cpp
 */

#include "StringInternTable.hpp"

#include "j9.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9modron.h"
#include "ut_j9bcu.h"
#include "bcutil_internal.h"
#include "SCStringInternHelpers.h"

/*
 * If you set VERIFY_ON_EVERY_OPERATION to 1, then the intern table
 * will be verified before and after every API operation.
 */
#define VERIFY_ON_EVERY_OPERATION 0

#define J9INTERN_ENTRY_SET_PREVNODE(base, value) SRP_SET((base)->prevNode, value)
#define J9INTERN_ENTRY_GET_PREVNODE(base) J9SHAREDINTERNSRPHASHTABLEENTRY_PREVNODE(base)
#define J9INTERN_ENTRY_SET_NEXTNODE(base, value) SRP_SET((base)->nextNode, value)
#define J9INTERN_ENTRY_GET_NEXTNODE(base) J9SHAREDINTERNSRPHASHTABLEENTRY_NEXTNODE(base)

#define MAX_INTERN_NODE_WEIGHT	0xFFFF

#if VERIFY_ON_EVERY_OPERATION
	#define VERIFY_ENTER() verify(__FILE__, __LINE__)
	#define VERIFY_EXIT() verify(__FILE__, __LINE__)
#else
	#define VERIFY_ENTER()
	#define VERIFY_EXIT()
#endif


/* Query struct for searching the string intern hash table. */
typedef struct J9InternHashTableQuery {
	J9UTF8 *utf8; /* Must be parallel to J9InternHashTableEntry.utf8 - always NULL */
	J9ClassLoader *classLoader; /* Must be parallel to J9InternHashTableEntry.classLoader */
	UDATA length;
	U_8 *data;
} J9InternHashTableQuery;


extern "C" {

static UDATA
internHashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	UDATA leftUtf8Length, rightUtf8Length;
	U_8 *leftUtf8Data, *rightUtf8Data;

	J9InternHashTableEntry *left = (J9InternHashTableEntry*)leftKey;
	J9InternHashTableEntry *right = (J9InternHashTableEntry*)rightKey;

	if (left->classLoader != right->classLoader) {
		return 0;
	}

	/* NOTE: Making this a function to get the fields kills performance - don't do it! */
	if (NULL != left->utf8) {
		leftUtf8Length = UDATA(J9UTF8_LENGTH(left->utf8));
		leftUtf8Data = J9UTF8_DATA(left->utf8);
	} else {
		leftUtf8Length = ((J9InternHashTableQuery*)left)->length;
		leftUtf8Data = ((J9InternHashTableQuery*)left)->data;
	}

	if (NULL != right->utf8) {
		rightUtf8Length = UDATA(J9UTF8_LENGTH(right->utf8));
		rightUtf8Data = J9UTF8_DATA(right->utf8);
	} else {
		rightUtf8Length = ((J9InternHashTableQuery*)right)->length;
		rightUtf8Data = ((J9InternHashTableQuery*)right)->data;
	}

	return J9UTF8_DATA_EQUALS(leftUtf8Data, leftUtf8Length, rightUtf8Data, rightUtf8Length);
}

static UDATA
internHashFn(void *key, void *userData)
{
	UDATA length;
	U_8 *data;

	J9InternHashTableEntry *node = (J9InternHashTableEntry*)key;

	if (NULL != node->utf8) {
		length = UDATA(J9UTF8_LENGTH(node->utf8));
		data = J9UTF8_DATA(node->utf8);
	} else {
		length = ((J9InternHashTableQuery*)node)->length;
		data = ((J9InternHashTableQuery*)node)->data;
	}

	UDATA hash = UDATA(node->classLoader);
	for (UDATA i = 0; i < length; i++) {
		hash = (hash << 5) - hash + data[i];
	}

	return hash;
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void
internHashClassLoadersUnloadHook(J9HookInterface **vmHooks, UDATA eventNum, void *eventData, void *userData)
{
	J9VMClassLoadersUnloadEvent *event = (J9VMClassLoadersUnloadEvent*)eventData;
	StringInternTable *stringInternTable = (StringInternTable*)userData;
	Trc_Assert_BCU_mustHaveExclusiveVMAccess(event->currentThread);
	stringInternTable->removeLocalNodesWithDeadClassLoaders();
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

} /* extern "C" */

StringInternTable::StringInternTable(J9JavaVM *vm, J9PortLibrary *portLibrary, UDATA maximumNodeCount) :
	_vm(vm),
	_portLibrary(portLibrary),
	_internHashTable(NULL),
	_headNode(NULL),
	_tailNode(NULL),
	_nodeCount(0),
	_maximumNodeCount(maximumNodeCount)
{
	if (0 != maximumNodeCount) {
		_internHashTable = hashTableNew(OMRPORT_FROM_J9PORT(_portLibrary), J9_GET_CALLSITE(),
			U_32(maximumNodeCount + 1), sizeof(J9InternHashTableEntry), sizeof(char *), 0,
			 J9MEM_CATEGORY_CLASSES, internHashFn, internHashEqualFn, NULL, vm);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if ((NULL != _vm) && (NULL != _internHashTable)) {
			J9HookInterface **vmHooks = _vm->internalVMFunctions->getVMHookInterface(vm);
			if (0 != (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_LOADERS_UNLOAD, internHashClassLoadersUnloadHook, OMR_GET_CALLSITE(), this)) {
				/* Failed to register the hook. Kill the hash table so that isOK() returns false. */
				hashTableFree(_internHashTable);
				_internHashTable = NULL;
			}
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}

	if (isOK()) {
		if (maximumNodeCount == 0) {
			Trc_BCU_stringInternTableNotCreated();
		} else {
			Trc_BCU_stringInternTableCreated(maximumNodeCount);
		}
	} else {
		Trc_BCU_stringInternTableCreationFailed(maximumNodeCount);
	}
}

StringInternTable::~StringInternTable()
{
	if (NULL != _internHashTable) {
		hashTableFree(_internHashTable);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (NULL != _vm) {
			J9HookInterface **vmHooks = _vm->internalVMFunctions->getVMHookInterface(_vm);
			(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_LOADERS_UNLOAD, internHashClassLoadersUnloadHook, this);
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	}
}

bool
StringInternTable::findUtf8(J9InternSearchInfo *searchInfo, J9SharedInvariantInternTable *sharedInternTable, bool isSharedROMClass, J9InternSearchResult *result)
{

	if (NULL == _internHashTable) {
		return false;
	}

	VERIFY_ENTER();

#if defined(J9VM_OPT_SHARED_CLASSES)
	/**
	 * Every UTF8s in shared string intern table is shared UTF8 in the shared cache.
	 * If ROM class is shared, then UTF8 can be used without SRP range check. (isSharedCacheInRange = SC_COMPLETELY_IN_THE_SRP_RANGE)
	 *
	 * If ROM class is local, then UTF8 can be used only if it is in the SRP range.
	 * In this case, SRP range check is required depending on the value of isSharedCacheInRange.
	 * 	 	1 (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE): Shared cache is not in the SRP range. UTF8 can not be used by local ROM class.
	 * 		2 (SC_COMPLETELY_IN_THE_SRP_RANGE): Shared cache is in the SRP range. UTF8 can be used by local ROM class safely without need for SRP range check.
	 * 		3 (SC_PARTIALLY_IN_THE_SRP_RANGE): Part of the shared cache is in the SRP range. UTF8 can be used if it is in the SRP range of local ROM class. SRP range check is required.
	 *
	 */
	if ((SC_COMPLETELY_IN_THE_SRP_RANGE == searchInfo->sharedCacheSRPRangeInfo) || (SC_PARTIALLY_IN_THE_SRP_RANGE == searchInfo->sharedCacheSRPRangeInfo)) {
		if (NULL != sharedInternTable) {
			J9SharedInternHashTableQuery sharedQuery;

			sharedQuery.length = searchInfo->stringLength;
			sharedQuery.data = searchInfo->stringData;

			J9SharedInternSRPHashTableEntry *sharedNode = (J9SharedInternSRPHashTableEntry*)srpHashTableFind(sharedInternTable->sharedInvariantSRPHashtable, &sharedQuery);
			if (NULL != sharedNode) {

				J9UTF8 *utf8 = J9SHAREDINTERNSRPHASHTABLEENTRY_UTF8SRP(sharedNode);
				bool isUTF8InSRPRange = true;
#if defined(J9VM_ENV_DATA64)
				if (SC_PARTIALLY_IN_THE_SRP_RANGE == searchInfo->sharedCacheSRPRangeInfo) {
					if (!areAddressesInSRPRange(utf8, searchInfo->romClassBaseAddr) ||
							!areAddressesInSRPRange(utf8, searchInfo->romClassEndAddr)) {
						isUTF8InSRPRange = false;
					}
				}
#endif
				if (isUTF8InSRPRange) {
					Trc_BCU_Assert_True(NULL != utf8);
					result->utf8 = utf8;
					result->node = sharedNode;
					result->isSharedNode = true;
					VERIFY_EXIT();
					return true;
				}
			}
		}
	}
#endif

	/* Otherwise, search the local hash table. */
	J9InternHashTableQuery query;

	query.utf8 = NULL;
	query.classLoader = searchInfo->classloader;
	query.length = searchInfo->stringLength;
	query.data = searchInfo->stringData;

	J9InternHashTableEntry *node = (J9InternHashTableEntry*)hashTableFind(_internHashTable, &query);

	/* If the node was not matched, try searching for the utf8 with the systemClassLoader. */
	if ((NULL == node) && (NULL != _vm) && (query.classLoader != _vm->systemClassLoader)) {
		query.classLoader = _vm->systemClassLoader;
		node = (J9InternHashTableEntry*)hashTableFind(_internHashTable, &query);
	}

	if (NULL != node) {
		bool sharedUtf8 = (STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED == (node->flags & STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED));
		/**
		 * node->utf8 can be used in 3 conditions out of 4.
		 * 1. sharedUTF8 && isSharedROMClass	-	node->utf8 can be used and no SRP range check is required for 64 bit environment.
		 * 											(isSharedCacheInRange = SC_COMPLETELY_IN_THE_SRP_RANGE)
		 * 2. sharedUTF8 && !isSharedROMClass	-	node->utf8 maybe used if it is in SRP range.
		 * 											(isSharedCacheInRange = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE, SC_COMPLETELY_IN_THE_SRP_RANGE or SC_PARTIALLY_IN_THE_SRP_RANGE)
		 * 3. !sharedUTF8 && isSharedROMClass	-	node->utf8 can not be used.
		 * 											ROM classes in shared cache can only use sharedUTF8s in the shared cache.
		 * 4. !sharedUTF8 && !isSharedROMClass	-	node->utf8 maybe used if it if in SRP range.
		 *
		 *
		 */
		if (sharedUtf8 || !isSharedROMClass) {
			/* Condition 1, 2 or 4 */
			bool isUTF8InSRPRange = true;
#if defined(J9VM_ENV_DATA64)
			if (!isSharedROMClass) {
			/* Condition 2 or 4 */
				if ((sharedUtf8 && (SC_PARTIALLY_IN_THE_SRP_RANGE == searchInfo->sharedCacheSRPRangeInfo)) || !sharedUtf8) {
					if (!areAddressesInSRPRange(node->utf8, searchInfo->romClassBaseAddr) ||
							!areAddressesInSRPRange(node->utf8, searchInfo->romClassEndAddr)) {
						isUTF8InSRPRange = false;
					}
				} else if ((sharedUtf8 && (searchInfo->sharedCacheSRPRangeInfo == SC_COMPLETELY_OUT_OF_THE_SRP_RANGE))) {
					isUTF8InSRPRange = false;
				}
			}
#endif
			if (isUTF8InSRPRange) {
				result->utf8 = node->utf8;
				result->node = node;
				result->isSharedNode = false;
				VERIFY_EXIT();
				return true;
			}
		}
	}

	VERIFY_EXIT();
	return false;
}
void
StringInternTable::markNodeAsUsed(J9InternSearchResult *result, J9SharedInvariantInternTable *sharedTable)
{
	VERIFY_ENTER();

#if defined(J9VM_OPT_SHARED_CLASSES)
	if (NULL != sharedTable) {

		if (result->isSharedNode) {
			if (0 == (sharedTable->flags & J9AVLTREE_DISABLE_SHARED_TREE_UPDATES)) {
				J9SharedInternSRPHashTableEntry *sharedNode = (J9SharedInternSRPHashTableEntry *)result->node;
				updateSharedNodeWeight(sharedTable, sharedNode);
				promoteSharedNodeToHead(sharedTable, sharedNode);
			}
		} else { /* local node */
			J9InternHashTableEntry *node = (J9InternHashTableEntry*)result->node;

			bool promoteToShared = false;

			updateLocalNodeWeight(node);

			/* we don't allocate new shared nodes, so there must be an existing shared node in order to promote */
			if (NULL != sharedTable->tailNode) {
				J9SharedInternSRPHashTableEntry *sharedTail = sharedTable->tailNode;
				promoteToShared = testNodePromotionWeight(sharedTable, node, sharedTail);
			}

			if (promoteToShared) {
				swapLocalNodeWithTailSharedNode(node, sharedTable);
			} else {
				promoteNodeToHead(node);
			}
		}
		VERIFY_EXIT();
		return;
	}
#endif

	Trc_BCU_Assert_False(result->isSharedNode);

	J9InternHashTableEntry *node = (J9InternHashTableEntry*)result->node;
	promoteNodeToHead(node);

	VERIFY_EXIT();
}
void
StringInternTable::internUtf8(J9UTF8 *utf8, J9ClassLoader *classLoader, bool fromSharedROMClass, J9SharedInvariantInternTable *sharedInternTable)
{
	Trc_BCU_Assert_True(NULL != utf8);

	if (NULL == _internHashTable) {
		return;
	}

	VERIFY_ENTER();

#if defined(J9VM_OPT_SHARED_CLASSES)
	if ((NULL != sharedInternTable) && (0 == (sharedInternTable->flags & J9AVLTREE_DISABLE_SHARED_TREE_UPDATES)) && fromSharedROMClass) {
		J9SharedInternSRPHashTableEntry *insertedEntry = insertSharedNode(sharedInternTable, utf8, 0, STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED, /* promoteIfExistingFound = */ true);

		if (NULL != insertedEntry) {
			VERIFY_EXIT();
			return;
		} else {
			Trc_BCU_getNewStringTableNode_SharedTreeFull(sharedInternTable->sharedInvariantSRPHashtable->srpHashtableInternal->tableSize);
		}
	}
#endif

	/* We did not insert it into the shared SRPHashTable, so add it to the local hash table. */
	J9InternHashTableEntry nodeToAdd;

	nodeToAdd.utf8 = utf8;
	nodeToAdd.classLoader = classLoader;
	nodeToAdd.internWeight = 0;
	nodeToAdd.flags = (fromSharedROMClass ? STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED : 0);

	J9InternHashTableEntry *entry = insertLocalNode(&nodeToAdd, /* promoteIfExistingFound = */ true);
	if (NULL != entry) {
		if (_nodeCount == _maximumNodeCount) {
			Trc_BCU_Assert_True(NULL != _tailNode);
			deleteLocalNode(_tailNode);
		} else {
			_nodeCount++;
		}
	}

	VERIFY_EXIT();
}

J9InternHashTableEntry *
StringInternTable::insertLocalNode(J9InternHashTableEntry *node, bool promoteIfExistingFound)
{
	U_32 nodeCount = hashTableGetCount(_internHashTable);

	node = (J9InternHashTableEntry*)hashTableAdd(_internHashTable, node);

	if (NULL != node) {
		/*
		 * If the hash table node count increased, that means that the returned node
		 * is the newly inserted node (and not an existing node with the same value).
		 *
		 * We cannot compare the returned node pointer with the node pointer passed to
		 * this function to check this as the pointer passed to this function points to
		 * a temporary node on the stack.
		 */
		bool wasInserted = (hashTableGetCount(_internHashTable) == nodeCount + 1);

		if (wasInserted) {
			node->prevNode = NULL;
			node->nextNode = _headNode;
			if (NULL == _tailNode) {
				_tailNode = node;
			} else {
				_headNode->prevNode = node;
			}
			_headNode = node;
		} else {
			/* Found existing node with same value - do not return it. */
			if (promoteIfExistingFound) {
				promoteNodeToHead(node);
			}
			node = NULL;
		}
	}

	return node;
}

void
StringInternTable::deleteLocalNode(J9InternHashTableEntry *node)
{
	removeNodeFromList(node);
	hashTableRemove(_internHashTable, node);
}

void
StringInternTable::removeNodeFromList(J9InternHashTableEntry *node)
{
	Trc_BCU_Assert_True(NULL != node);

	J9InternHashTableEntry *prevNode = node->prevNode;
	J9InternHashTableEntry *nextNode = node->nextNode;

	if (NULL != prevNode) {
		prevNode->nextNode = nextNode;
	}
	if (NULL != nextNode) {
		nextNode->prevNode = prevNode;
	}
	if (_tailNode == node) {
		_tailNode = prevNode;
	}
	if (_headNode == node) {
		_headNode = nextNode;
	}
}

void
StringInternTable::promoteNodeToHead(J9InternHashTableEntry *node)
{
	Trc_BCU_Assert_True(NULL != node);

	/* Assumption: node must already be in the LRU list. */

	if (_headNode != node) {
		J9InternHashTableEntry *prevNode = node->prevNode;
		J9InternHashTableEntry *nextNode = node->nextNode;

		if (NULL != prevNode) {
			prevNode->nextNode = nextNode;
		}
		if (NULL != nextNode) {
			nextNode->prevNode = prevNode;
		}

		node->prevNode = NULL;
		node->nextNode = _headNode;

		_headNode->prevNode = node;
		_headNode = node;

		if (node == _tailNode) {
			_tailNode = prevNode;
		}
	}
}

void StringInternTable::removeLocalNodesWithDeadClassLoaders()
{
	VERIFY_ENTER();

	J9InternHashTableEntry *node = _headNode;
	while (NULL != node) {
		J9InternHashTableEntry *nextNode = node->nextNode;
		if (J9_ARE_ALL_BITS_SET(node->classLoader->gcFlags, J9_GC_CLASS_LOADER_DEAD)) {
			deleteLocalNode(node);
			_nodeCount--;
		}
		node = nextNode;
	}

	VERIFY_EXIT();
}

#define VERIFY_ASSERT(cond) do { \
		if (!(cond)) { \
			PORT_ACCESS_FROM_PORT(_portLibrary); \
			j9tty_printf(PORTLIB, "StringInternTable verification condition ["#cond"] failed at %s:%d!\n", file, line); \
			Trc_BCU_Assert_InternVerificationFailure(); \
			return false; \
		} \
	} while (0)

bool StringInternTable::verify(const char *file, IDATA line) const
{
	VERIFY_ASSERT(_nodeCount <= _maximumNodeCount);
	VERIFY_ASSERT(hashTableGetCount(_internHashTable) == _nodeCount);

	if ((NULL != _headNode) || (NULL != _tailNode)) {
		verifyNode(_headNode, file, line);
		verifyNode(_tailNode, file, line);
		VERIFY_ASSERT(_nodeCount > 0);
	} else {
		VERIFY_ASSERT(NULL == _headNode);
		VERIFY_ASSERT(NULL == _tailNode);
		VERIFY_ASSERT(_nodeCount == 0);
	}

	UDATA count = 0;
	J9InternHashTableEntry *node = _headNode;
	while (NULL != node) {
		verifyNode(node, file, line);
		node = node->nextNode;
		count++;
	}
	VERIFY_ASSERT(count == _nodeCount);

	return true;
}

bool StringInternTable::verifyNode(J9InternHashTableEntry *node, const char *file, IDATA line) const
{
	VERIFY_ASSERT(NULL != node);
	if (node == _headNode) {
		VERIFY_ASSERT(NULL == node->prevNode);
	} else {
		VERIFY_ASSERT(NULL != node->prevNode);
		VERIFY_ASSERT(node == node->prevNode->nextNode);
	}
	if (node == _tailNode) {
		VERIFY_ASSERT(NULL == node->nextNode);
	} else {
		VERIFY_ASSERT(NULL != node->nextNode);
		VERIFY_ASSERT(node == node->nextNode->prevNode);
	}
	VERIFY_ASSERT(NULL != node->utf8);
	VERIFY_ASSERT(hashTableFind(_internHashTable, node) == node);
	return true;
}

#undef VERIFY_ASSERT

#if defined(J9VM_OPT_SHARED_CLASSES)

J9SharedInternSRPHashTableEntry *
StringInternTable::insertSharedNode(J9SharedInvariantInternTable *table, J9UTF8 * utf8, U_16 internWeight, U_16 flags, bool promoteIfExistingFound)
{
	J9SharedInternHashTableQuery sharedQuery;

	sharedQuery.length = J9UTF8_LENGTH(utf8);
	sharedQuery.data = J9UTF8_DATA(utf8);

	J9SharedInternSRPHashTableEntry *insertedNode = (J9SharedInternSRPHashTableEntry *)srpHashTableAdd(table->sharedInvariantSRPHashtable, &sharedQuery);
	if (insertedNode) {
		/* It is either found or inserted into srp hashtable */
		if (IS_NEW_ELEMENT(insertedNode)) {
			/* A new node is inserted */
			UNMARK_NEW_ELEMENT(insertedNode, J9SharedInternSRPHashTableEntry *);
			insertedNode->prevNode = 0;
			/* Fix the pointers and fill out the inserted shared node */
			J9INTERN_ENTRY_SET_NEXTNODE(insertedNode, table->headNode);
			if (NULL == table->tailNode) {
				table->tailNode = insertedNode;
			} else {
				J9INTERN_ENTRY_SET_PREVNODE(table->headNode, insertedNode);
			}
			table->headNode = insertedNode;
			SRP_SET(insertedNode->utf8SRP, utf8);
			insertedNode->internWeight = internWeight;
			insertedNode->flags = flags;
			/* update the table */
			++*(table->totalSharedNodesPtr);
			*(table->totalSharedWeightPtr) += internWeight;
		} else {
			/* Existing node is found */
			if (promoteIfExistingFound) {
				promoteSharedNodeToHead(table, insertedNode);
			}
		}
	}
	return insertedNode;
}

void
StringInternTable::deleteSharedNode(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *node)
{
	removeSharedNodeFromList(table, node);
	U_16 weight = node->internWeight;
	J9UTF8 * utf8ToDelete = SRP_GET(node->utf8SRP, J9UTF8 *);
	J9SharedInternHashTableQuery sharedQuery;
	sharedQuery.length = J9UTF8_LENGTH(utf8ToDelete);
	sharedQuery.data = J9UTF8_DATA(utf8ToDelete);

	U_32 deleted = srpHashTableRemove(table->sharedInvariantSRPHashtable, &sharedQuery);
	if (0 == deleted) {
		/* shared node removed - update stats */
		--*(table->totalSharedNodesPtr);
		*(table->totalSharedWeightPtr) -= weight;
	}
	return;
}

void
StringInternTable::removeSharedNodeFromList(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *sharedNode)
{
	Trc_BCU_Assert_True(NULL != sharedNode);

	J9SharedInternSRPHashTableEntry *prevNode = J9INTERN_ENTRY_GET_PREVNODE(sharedNode);
	J9SharedInternSRPHashTableEntry *nextNode = J9INTERN_ENTRY_GET_NEXTNODE(sharedNode);

	if (NULL != prevNode) {
		J9INTERN_ENTRY_SET_NEXTNODE(prevNode, nextNode);
	}
	if (NULL != nextNode) {
		J9INTERN_ENTRY_SET_PREVNODE(nextNode, prevNode);
	}
	if (table->tailNode == sharedNode) {
		table->tailNode = prevNode;
	}
	if (table->headNode == sharedNode) {
		table->headNode = nextNode;
	}
}

/**
 * Calculate the bytes required to write a UTF8 with the specified length into memory.
 * @param	length Length of the UTF8
 * @return U_16 space required in the memory to write the UTF8 with specified length
 */
UDATA
StringInternTable::getRequiredBytesForUTF8Length(U_16 length)
{
	UDATA bytesRequiredByUTF8 = sizeof(U_16);
	bytesRequiredByUTF8 += length;
	if (0 != (length & 1)) {
		bytesRequiredByUTF8 += 1;
	}
	return bytesRequiredByUTF8;
}

/**
 * Increment a local node's weight when it's used.
 * @param[in] node The local node.
 */
void
StringInternTable::updateLocalNodeWeight(J9InternHashTableEntry *node)
{
	if (node->internWeight == MAX_INTERN_NODE_WEIGHT) {
		return;
	}

	UDATA bytesRequiredByUTF8 = getRequiredBytesForUTF8Length(J9UTF8_LENGTH(node->utf8));
	if ((node->internWeight + bytesRequiredByUTF8) >= MAX_INTERN_NODE_WEIGHT) {
		node->internWeight = MAX_INTERN_NODE_WEIGHT;
	} else {
		node->internWeight += (U_16)bytesRequiredByUTF8;
	}
}

/**
 * Increment a shared node's weight when it's used.
 * The shared table's weight is also incremented.
 * @param[in] table The invariantInternTable.
 * @param[in] sharedNode The shared node.
 */
void
StringInternTable::updateSharedNodeWeight(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *sharedNode)
{
	J9UTF8 *utf8 = J9SHAREDINTERNSRPHASHTABLEENTRY_UTF8SRP(sharedNode);
	UDATA bytesRequiredByUTF8 = getRequiredBytesForUTF8Length(J9UTF8_LENGTH(utf8));
	if (sharedNode->internWeight != MAX_INTERN_NODE_WEIGHT) {
		if ((sharedNode->internWeight + bytesRequiredByUTF8) >= MAX_INTERN_NODE_WEIGHT) {
			sharedNode->internWeight = MAX_INTERN_NODE_WEIGHT;
		} else {
			sharedNode->internWeight += (U_16)bytesRequiredByUTF8;
		}
	}

	*(table->totalSharedWeightPtr) += (U_32)bytesRequiredByUTF8;
}

/**
 *  Tests whether a local node should be promoted to the shared tree.
 *  @param[in] table The invariantInternTable.
 *  @param[in] node The local node to be tested.
 *  @param[in] sharedNodeToDisplace The shared node to displace, not null.
 *  @retval false The node should not be promoted into the shared table.
 *  @retval true  The node should be promoted into the shared table.
 */
bool
StringInternTable::testNodePromotionWeight(J9SharedInvariantInternTable *table, J9InternHashTableEntry *node, J9SharedInternSRPHashTableEntry *sharedNodeToDisplace)
{
	/* Only allow a node into the shared tree IF:
	 * - If J9AVLTREE_DISABLE_SHARED_TREE_UPDATES is not set
	 * - It is STRINGINTERNTABLENODE_FLAG_UTF8_IS_SHARED. This means that the string it points to is shared and has been successfully relocated.
	 * - If the intern weight of the node to be promoted is greater than the shared node being displaced,
	 * - or if the node has a greater intern weight of > 100,
	 */
	return (0 == (table->flags & J9AVLTREE_DISABLE_SHARED_TREE_UPDATES)) &&
	       (0 != (node->flags & STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED)) &&
	       ((node->internWeight > 100) || (node->internWeight > sharedNodeToDisplace->internWeight));
}

void
StringInternTable::swapLocalNodeWithTailSharedNode(J9InternHashTableEntry *node, J9SharedInvariantInternTable *table)
{
	/* No need to check for J9AVLTREE_DISABLE_SHARED_TREE_UPDATES as this is tested in testNodePromotionWeight() */
	J9InternHashTableEntry localNodeToInsert;
	J9SharedInternSRPHashTableEntry *sharedTail = table->tailNode;
	/* Copy shared tail node data*/
	localNodeToInsert.utf8 = SRP_GET(sharedTail->utf8SRP, J9UTF8*);
	localNodeToInsert.flags = (sharedTail->flags);
	localNodeToInsert.internWeight = sharedTail->internWeight;
	localNodeToInsert.classLoader = table->systemClassLoader;

	deleteSharedNode(table, table->tailNode);

	J9SharedInternSRPHashTableEntry * insertedSharedNode = insertSharedNode(table, node->utf8, node->internWeight, node->flags,  FALSE);
	deleteLocalNode(node);

	/* Copy data from shared node to a local node and insert the local node into the hash table. */
	J9InternHashTableEntry *entry = insertLocalNode(&localNodeToInsert, /* promoteIfExistingFound = */ false);
	if (NULL == entry) {
		/* Shared node matched an existing node in the local table and was not inserted. Decrement local count. */
		_nodeCount--;
	}
}

void
StringInternTable::promoteSharedNodeToHead(J9SharedInvariantInternTable *table, J9SharedInternSRPHashTableEntry *node)
{
	J9SharedInternSRPHashTableEntry *headNode;
	J9SharedInternSRPHashTableEntry **tableHeadNodePtr = &(table->headNode);

	headNode = *tableHeadNodePtr;

	if (headNode != node) {
		J9SharedInternSRPHashTableEntry *prevNode;
		J9SharedInternSRPHashTableEntry *nextNode;

		prevNode = J9INTERN_ENTRY_GET_PREVNODE(node);
		nextNode = J9INTERN_ENTRY_GET_NEXTNODE(node);

		if (prevNode) {
			/* unlink this node from the previous node */
			J9INTERN_ENTRY_SET_NEXTNODE(prevNode, nextNode);
		}
		if (nextNode) {
			/* unlink this node from the next node */
			J9INTERN_ENTRY_SET_PREVNODE(nextNode, prevNode);
		}

		J9INTERN_ENTRY_SET_PREVNODE(node, 0);
		J9INTERN_ENTRY_SET_NEXTNODE(node, headNode);

		if (headNode) {
			J9INTERN_ENTRY_SET_PREVNODE(headNode, node);
		}
		*tableHeadNodePtr = node;

		if (NULL == table->tailNode) {
			table->tailNode = node;
		} else if (node == table->tailNode) {
			if (prevNode != NULL) {
				table->tailNode = prevNode;
			}
		}
	}

	return;
}

#endif /* J9VM_OPT_SHARED_CLASSES */
