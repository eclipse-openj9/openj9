/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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

/*
 * file    : srphashtable.c
 *
 *  Generic SRP hash table implementation
 */

#include <string.h>
#include "srphashtable_api.h"
#include "simplepool_api.h"
#include "ut_srphashtable.h"
#include "omrutil.h"
#include "j9memcategories.h"

#undef SRPHASHTABLE_DEBUG

/*This is used in debug mode. It defines info of how many nodes to be printed*/
#define NUMBEROFNODESTOPRINT 25

/**
 * SRP node macros
 */
 
#define NEXT(srpPointer) ((J9SRP *)((char *)srpPointer + *srpPointer + srptable->srpHashtableInternal->nodeSize - sizeof(J9SRP)))

#define SRPHASHTABLE_CREATED_BY_SRPHASHTABLENEW 1
#define SRPHASHTABLE_CREATED_BY_SRPHASHTABLENEWINREGION 2
#define SRPHASHTABLE_CREATED_BY_SRPHASHTABLERECREATE 3

/**
 * Copied from gc_base/gcutils.h
 */
#define ROUND_TO_SIZEOF_UDATA(number) (((number) + (sizeof(UDATA) - 1)) & (~(sizeof(UDATA) - 1)))

static J9SRP * srpHashTableFindNode (J9SRPHashTable *srptable, void *entry);
void  * srpHashTableNextDo(J9SRPHashTable *srptable, U_32 *bucketIndex, void * currentNode);
BOOLEAN srpHashTable_checkConsistency(
		J9SRPHashTable* srptable,
		J9PortLibrary *portLib,
		BOOLEAN (*doFunction) (void* anElement, void* userData),
		void* userData,
		UDATA skipCount);
void srpHashTableDebugPrintInfo(J9SRPHashTable *srptable, UDATA numberOfNodesToPrint);
U_32 srpHashTable_requiredMemorySize(U_32 tableSize, U_32 entrySize, BOOLEAN ceilUp);



/*****************************************************************************
 *  Hash Table statistics defines
 */
#define J9SRPHASHTABLE_DEBUG_NODE_COUNT_MAX 1024

/**
 * 	LAYOUT IN MEMORY
 * 	 ______________________________________________________________________________________________________
 * 	|				struct J9SRPHashTable																   |
 * 	|																									   |
 * 	|	->tableName     			const char*															   |
 * 	|	->srpHashtableInternal		struct J9SRPHashTableInternal*										   |
 * 	|	->hashFn					UDATA  (*)(void *key, void *userData)							   |
 * 	|	->hashEqualFn				UDATA  (*)(void *leftKey, void *rightKey, void *userData)		   |
 * 	|	->printFn					void  (*)(J9PortLibrary *portLibrary, void *key, void *userData) |
 * 	|	->portLibrary				struct J9PortLibrary*												   |
 * 	|	->functionUserData			void*																   |
 *  |______________________________________________________________________________________________________|
 *
 *   _______________________________________________________________________________________________________
 * 	|struct J9SRPHashTableInternal |		Nodes		| 			SimplePool		|		Entry Nodes	   	|
 * 	|							   |	|	|	|	|	|	struct	J9SimplePool	|	|	|	|	|	|	|
 * 	| ->tableSize	  U_32	       |	|	|	|	|	|							|	|	|	|	|	|	|
 *  | ->numberOfNodes U_32  	   |	|	|	|	|	|	->numElements	U_32	|	|	|	|	|	|	|
 *  | ->entrySize     U_32		   |	|	|	|	|	|	->elementSize	U_32	|	|	|	|	|	|	|
 *  | ->nodeSize      U_32		   |	|	|	|	|	|	->freeList		J9SRP	|	|	|	|	|	|	|
 *  | ->flags         U_32  	   |	|	|	|	|	|	->firstFreeSlot	J9SRP	|	|	|	|	|	|	|
 *  | ->nodes         J9SRP		   |	|	|	|	|	|	->blockEnd		J9SRP	|	|	|	|	|	|	|
 *  | ->nodePool      J9SRP		   |	|	|	|	|	|	->flags 		U_32	|	| 	|	|	|	|	|
 * 	|______________________________|____________________|___________________________|_______________________|
 *
 */

/**
 * \brief       Create a new SRP hash table
 * \ingroup     srp_hash_table
 *
 *
 * @param portLibrary	The port library
 * @param tableName     A string giving the name of the SRP table, recommended to use J9_GET_CALLSITE() 
 * 						within caller code, this is not a restriction. If a unique name is used, it 
 * 						must be easily searchable, i.e. there should not be more than a few false hits 
 * 						when searching for the name. If necessary to ensure uniqueness or find-ability, 
 * 						create a list of the unique names or name conventions in this file.
 * @param tableSize     Constant number of SRP hash table nodes
 * 					    (Next upper value of user-passed tableSize in primes table will be used)
 * @param entrySize     Size of the user-data for each node
 * @param flags			Flags to control SRP hash table behavior
 * @param hashFn        Mandatory hashing function pointer
 * @param hashEqualFn   Mandatory hash compare function pointer
 * @param printFn       Optional node-print function pointer
 * @param userData      Optional userData pointer to be passed to hashFn and hashEqualFn
 * @return              An initialized SRP hash table
 *
 *	Creates a SRP hash table with specified constant number of nodes. The actual number
 *  of nodes is ceiled to the nearest prime number obtained from primesTable[].
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
	void *functionUserData)
{
	J9SRPHashTable *srpHashTable;
	J9SRPHashTableInternal *srpHashTableInternal;
	J9SimplePool * temppool;
	U_32 primeTableSize;

	U_32 srpHashTableInternalSize = sizeof(J9SRPHashTableInternal);
	U_32 srpHashTableTotalSize = 0;
	U_32 simplePoolNodeSize = 0;
	U_32 simplePoolTotalSize = 0;
	U_32 totalSize = 0;

	void * allocatedMemory = NULL;
	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_srpHashTableNew_Entry(portLibrary, tableName, tableSize, entrySize, flags, hashFn, hashEqualFn, printFn, functionUserData);

	/*Allocate memory for struct J9SRPHashTable*/
	srpHashTable = OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory(OMRPORT_FROM_J9PORT(PORTLIB), sizeof(J9SRPHashTable), tableName, OMRMEM_CATEGORY_VM);
#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableNew <%s>: tableSize=%d, srpTable=%p\n", tableName, tableSize, srpHashTable);
#endif 
	if (!srpHashTable) {
		Trc_srpHashTableNew_failedToAllocMemoryForSRPHashTable(tableSize, entrySize);
		Trc_srpHashTableNew_Exit(NULL);
		return NULL;
	}

	/*Ceil the tableSize to the nearest prime number obtained from primesTable[]*/
	primeTableSize = (U_32)findSmallestPrimeGreaterThanOrEqualTo(tableSize);
	if (primeTableSize == PRIMENUMBERHELPER_OUTOFRANGE) {
		Trc_srpHashTableNew_tableSizeOutOfRange(tableSize, getSupportedBiggestNumberByPrimeNumberHelper());
		Trc_srpHashTableNew_Exit(NULL);
		j9mem_free_memory(srpHashTable);
		return NULL;
	}
	/*Calculate the total size of memory required for SRP Hashtable*/
	srpHashTableTotalSize = ROUND_TO_SIZEOF_UDATA(sizeof(J9SRP) * primeTableSize);
	simplePoolNodeSize = entrySize + sizeof(J9SRP);
	simplePoolTotalSize = simplepool_totalSize(simplePoolNodeSize, (U_32)primeTableSize);
	totalSize = srpHashTableInternalSize + srpHashTableTotalSize + simplePoolTotalSize;

	/*Allocate memory for struct J9SRPHashTableInternal, SRP hash table nodes, and simple pool*/
	allocatedMemory = OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory(OMRPORT_FROM_J9PORT(PORTLIB), totalSize, tableName, OMRMEM_CATEGORY_VM);
	/*Check whether memory allocation was successful*/
	if (!allocatedMemory) {
		Trc_srpHashTableNew_failedToAllocMemoryForInternalSRPHashTable(primeTableSize, entrySize, totalSize);
		Trc_srpHashTableNew_Exit(NULL);
		j9mem_free_memory(srpHashTable);
		return NULL;
	}
	/*Set bits to 0*/
	memset((char *)allocatedMemory + sizeof(J9SRPHashTableInternal) , 0, srpHashTableTotalSize);


	/*SRPHASHTABLE*/
	srpHashTable->portLibrary = portLibrary;
	srpHashTable->tableName = tableName;
	srpHashTable->hashFn = hashFn;
	srpHashTable->hashEqualFn = hashEqualFn;
	srpHashTable->printFn = printFn;
	srpHashTable->functionUserData = functionUserData;
	srpHashTable->flags = SRPHASHTABLE_CREATED_BY_SRPHASHTABLENEW;

	/*SRPHASTABLEINTERNAL*/
	srpHashTableInternal = (J9SRPHashTableInternal *) (allocatedMemory);
	srpHashTable->srpHashtableInternal = srpHashTableInternal;
	srpHashTableInternal->tableSize = primeTableSize;
	srpHashTableInternal->numberOfNodes = 0;
	srpHashTableInternal->entrySize = entrySize;
	srpHashTableInternal->nodeSize = simplePoolNodeSize;
	srpHashTableInternal->flags = flags;
	SRP_SET(srpHashTableInternal->nodes, ((char * )srpHashTableInternal) + srpHashTableInternalSize);
	temppool = simplepool_new( (void *)(((char *)srpHashTableInternal) + srpHashTableInternalSize + srpHashTableTotalSize),
			simplePoolTotalSize,
			simplePoolNodeSize,
			0);

	SRP_SET(srpHashTableInternal->nodePool, temppool);

	Trc_srpHashTableNew_Exit(srpHashTable);

	return srpHashTable;
}

/**
 * \brief       Create a new srp hash table in a given memory region
 * \ingroup     srp_hash_table
 *
 *
 * @param portLibrary   The port library.
 * @param tableName     A string giving the name of the SRP table, recommended to use J9_GET_CALLSITE() 
 * 						within caller code, this is not a restriction. If a unique name is used, it 
 * 						must be easily searchable, i.e. there should not be more than a few false hits 
 * 						when searching for the name. If necessary to ensure uniqueness or find-ability, 
 * 						create a list of the unique names or name conventions in this file.
 * @param address		Allocated memory address for SRP hash table to build.
 * @param memorySize	The size of the allocated memory address for SRP hash table.
 * @param entrySize     Size of the user-data for each node
 * @param flags			Flags to control SRP hash table behavior
 * @param hashFn        Mandatory hashing function pointer
 * @param hashEqualFn  	Mandatory hash compare function pointer
 * @param printFn   	Optional node-print function pointer
 * @param userData      Optional userData pointer to be passed to hashFn and hashEqualFn
 * @return				An initialized SRP hash table
 *
 * Creates a SRP hash table with as many nodes as possible in the given memory region.
 * The actual number of nodes is egual to the next smaller value in primes table
 * of max elements that can fit in a given memory.
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
	void *functionUserData)
{
	J9SRPHashTable *srpHashTable = NULL;
	J9SRPHashTable *srpHashTableTemp = NULL;


	PORT_ACCESS_FROM_PORT(portLibrary);

	/*Allocate memory for struct J9SRPHashTable*/
	srpHashTableTemp = OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory(OMRPORT_FROM_J9PORT(PORTLIB), sizeof(J9SRPHashTable), tableName, OMRMEM_CATEGORY_VM);
#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableNewInRegion <%s>: srpTable=%p\n", tableName, srpHashTable);
#endif
	if (!srpHashTableTemp) {
		return NULL;
	}

	srpHashTable = srpHashTableReset(
			portLibrary,
			tableName,
			srpHashTableTemp,
			address,
			memorySize,
			entrySize,
			flags,
			hashFn,
			hashEqualFn,
			printFn,
			functionUserData);
	if (srpHashTable != NULL) {
		srpHashTable->flags = SRPHASHTABLE_CREATED_BY_SRPHASHTABLENEWINREGION;
	} else {
		j9mem_free_memory(srpHashTableTemp);
	}
	return srpHashTable;
}

/**
 * This function resets SRP HashTable structure.
 * This function will possibly be called in the cases where valid SRP HashTable is corrupted.
 *
 * @param portLibrary   The port library.
 * @param tableName		A string giving the name of the SRP table.
 * @param srpHashTable	Allocated memory address for SRP hash table structure.
 * @param address		Allocated memory address for SRP hash table to build.
 * @param memorySize	The size of the allocated memory address for SRP hash table.
 * @param entrySize     Size of the user-data for each node
 * @param flags			Flags to control SRP hash table behavior
 * @param hashFn        Mandatory hashing function pointer
 * @param hashEqualFn  	Mandatory hash compare function pointer
 * @param printFn   	Optional node-print function pointer
 * @param userData      Optional userData pointer to be passed to hashFn and hashEqualFn
 * @return				A reseted SRP hash table
 *
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
		void *functionUserData)
{
	U_32 tableSize = 0;
	U_32 simplePoolNodeSize = 0;
	J9SRPHashTableInternal *srpHashTableInternal;
	J9SimplePool * temppool;

	PORT_ACCESS_FROM_PORT(portLibrary);

	Trc_srpHashTableReset_Entry(portLibrary, tableName, srpHashTable, address, memorySize, entrySize, flags, hashFn, hashEqualFn, printFn, functionUserData);
	/*Calculate the sizes of each member in the allocated memory*/

	tableSize = srpHashTable_calculateTableSize(memorySize, entrySize, FALSE);

	/**
	 * If the given memory is not big enough for the smallest SRPHashTable to be generated or
	 * if the given memory is too big that the number of elements that fits in it is not in the supported range of primeNumberHelper
	 * then return NULL
	 */
	if (tableSize == 0) {
		Trc_srpHashTableReset_memoryIsTooSmall(memorySize, srpHashTable_requiredMemorySize(2, sizeof(J9SharedInternSRPHashTableEntry), TRUE));
		Trc_srpHashTableReset_Exit(srpHashTable);
		return NULL;
	} else if (tableSize == PRIMENUMBERHELPER_OUTOFRANGE) {
		Trc_srpHashTableReset_memoryIsTooBig(memorySize, entrySize, getSupportedBiggestNumberByPrimeNumberHelper());
		Trc_srpHashTableReset_Exit(srpHashTable);
		return NULL;
	}

	simplePoolNodeSize = entrySize + sizeof(J9SRP);
	/*Set bits to 0*/
	memset((char *)address + sizeof(J9SRPHashTableInternal), 0, ROUND_TO_SIZEOF_UDATA(sizeof(J9SRP) * tableSize));

	/*SRPHASTABLEINTERNAL*/
	srpHashTableInternal = (J9SRPHashTableInternal *) (address);
	srpHashTableInternal->tableSize = tableSize;
	srpHashTableInternal->numberOfNodes = 0;
	srpHashTableInternal->entrySize = entrySize;
	srpHashTableInternal->nodeSize = simplePoolNodeSize;
	srpHashTableInternal->flags = flags;
	SRP_SET(srpHashTableInternal->nodes, ((char * )srpHashTableInternal) + sizeof(J9SRPHashTableInternal));
	temppool = 	simplepool_new( (void *)(((char *)srpHashTableInternal) + sizeof(J9SRPHashTableInternal) + ROUND_TO_SIZEOF_UDATA(sizeof(J9SRP) * tableSize)),
			simplepool_totalSize(simplePoolNodeSize, tableSize),
			simplePoolNodeSize,
			0);

	SRP_SET(srpHashTableInternal->nodePool, temppool);
	/*SRPHASHTABLE*/
	srpHashTable->portLibrary = portLibrary;
	srpHashTable->tableName = tableName;
	srpHashTable->srpHashtableInternal = srpHashTableInternal;
	srpHashTable->hashFn = hashFn;
	srpHashTable->hashEqualFn = hashEqualFn;
	srpHashTable->printFn = printFn;
	srpHashTable->functionUserData = functionUserData;

	Trc_srpHashTableReset_Exit(srpHashTable);

	return srpHashTable;
}

/**
 * \brief       Reconstitute an existing hash table in a given memory region
 * \ingroup     srp_hash_table
 *
 *
 * @param portLibrary           The port library
 * @param tableName     		A string giving the name of the SRP table, recommended to use J9_GET_CALLSITE() 
 * 								within caller code, this is not a restriction. If a unique name is used, it 
 * 								must be easily searchable, i.e. there should not be more than a few false hits 
 * 								when searching for the name. If necessary to ensure uniqueness or find-ability, 
 * 								create a list of the unique names or name conventions in this file.
 * @param address				The address of SRP Hashtable
 * @param hashFn                Mandatory hashing function pointer
 * @param hashEqualFn         	Mandatory hash compare function pointer
 * @param printFn               Optional node-print function pointer
 * @param userData              Optional userData pointer to be passed to hashFn and hashEqualFn
 * @return                      An initialized SRP hash table
 *
 *	Reconstitutes an existing SRP hash table.
 */
J9SRPHashTable *
srpHashTableRecreate(
	J9PortLibrary *portLibrary,
	const char *tableName,
	void *address,
	J9SRPHashTableHashFn hashFn,
	J9SRPHashTableEqualFn hashEqualFn,
	J9SRPHashTablePrintFn printFn,
	void *functionUserData)
{
	J9SRPHashTable *srpHashTable;
	J9SRPHashTableInternal *srpHashTableInternal = (J9SRPHashTableInternal *)address;

	PORT_ACCESS_FROM_PORT(portLibrary);

	/*Allocate memory for struct J9SRPHashTable*/
	srpHashTable = OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory(OMRPORT_FROM_J9PORT(PORTLIB), sizeof(J9SRPHashTable), tableName, OMRMEM_CATEGORY_VM);
#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableNew <%s>: tableSize=%d, srpTable=%p\n", tableName, srpHashTableInternal->tableSize, srpHashTable);
#endif
	if (!srpHashTable) {
		return NULL;
	}

	/*SRPHASHTABLE*/
	srpHashTable->portLibrary = portLibrary;
	srpHashTable->tableName = tableName;
	srpHashTable->hashFn = hashFn;
	srpHashTable->hashEqualFn = hashEqualFn;
	srpHashTable->printFn = printFn;
	srpHashTable->functionUserData = functionUserData;
	srpHashTable->flags = SRPHASHTABLE_CREATED_BY_SRPHASHTABLERECREATE;
	srpHashTable->srpHashtableInternal = srpHashTableInternal;

	return srpHashTable;
}

/**
 * \brief       Free a previously allocated srp-hash-table.
 * \ingroup     srp_hash_table
 *
 *
 * @param srptable
 *
 *      Frees the srp-hash-table and all node-chains.
 */
void
srpHashTableFree(J9SRPHashTable *srptable)
{
	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "hashTableFree <%s>: table=%p\n", srptable->tableName, srptable);
#endif

	if (srptable) {
		if ((srptable->flags & SRPHASHTABLE_CREATED_BY_SRPHASHTABLENEW) != 0) {
			j9mem_free_memory(srptable->srpHashtableInternal);
			j9mem_free_memory(srptable);
		} else if (((srptable->flags & SRPHASHTABLE_CREATED_BY_SRPHASHTABLENEWINREGION) != 0) ||
				((srptable->flags & SRPHASHTABLE_CREATED_BY_SRPHASHTABLERECREATE) != 0)) {
			j9mem_free_memory(srptable);
		} else {
			/* We never come here */
		}
	}
}



/**
 * \brief       Find an entry in the SRP hash table.
 * \ingroup     srp_hash_table
 *
 *
 * @param table
 * @param key				A key that will have enough info to generate hash value.
 * 							Also it should provide enough info to compare with the existing entries.
 * 							A SRP HashTable Entry can be passed as a key.
 * @return                  NULL if entry is not present in the table;
 * 							otherwise a pointer to the user-data
 *
 */
void *
srpHashTableFind(J9SRPHashTable *srptable, void *key)
{
	J9SRP * srpnode = 0;
	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableFind <%s>: srptable=%p key=%p\n", srptable->tableName, srptable, key);
#endif

	srpnode = srpHashTableFindNode(srptable, key);

	return SRP_GET(*srpnode, void *);
}


/**
 * \brief       Add an entry to the SRP hash table.
 * \ingroup     srp_hash_table
 *
 *
 * @param table
 * @param key				A key should provide enough info to generate hash value and
 * 							to do comparison with the entries in SRP hashtable to find out whether
 * 							such entry already exist or where to add the value.
 * @return                  NULL on failure (to allocate a new node);
 * 							pointer to an existing entry if it already exist,
 * 							otherwise pointer to an empty simplepool node that is added plus 1.
 *
 *      The call may fail on allocating new nodes to hold the entry. If the
 *      entry is already present in the srp-hash-table, returns a pointer to it.
 *      Otherwise, it adds an empty node to simplepool and return a pointer to it and PLUS 1.
 *      We add 1 to the added node's pointer so that the caller of this function can know whether
 *      the entry already exist or it is added. If the value of a pointer is ODD number,
 *      it means empty node is added and caller is responsible to fill it in. If it is EVEN number,
 *      then it is pointer to an existing entry.
 */
void *
srpHashTableAdd(J9SRPHashTable *srptable, void *key)
{
	J9SRP * srpnode;
	void *newElement;

	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableAdd <%s>: srptable=%p key=%p\n", srptable->tableName, srptable, key);
#endif

	/*
	 * Find the right node in the hash array to make the insertion at. Node
	 * might be on a collision chain
	 */
	srpnode = srpHashTableFindNode(srptable, key);
	if (*srpnode != 0) {
		/* If entry is already hashed, return it */
#ifdef SRPHASHTABLE_DEBUG
		j9tty_printf(PORTLIB, "--->ENTRY IS ALREADY HASHED\n");
#endif
		return SRP_GET(*srpnode, void *);
	}

	/* Create a new node */
	newElement = simplepool_newElement(SRP_GET(srptable->srpHashtableInternal->nodePool, J9SimplePool *));
	if (newElement == NULL) {
		return NULL;
	}

	/* Insert the newNode. Node might be on a collision chain */
	SRP_SET(*srpnode, newElement);
	srptable->srpHashtableInternal->numberOfNodes++;

#ifdef SRPHASHTABLE_DEBUG
	srpHashTableDumpDistribution(srptable);
	srpHashTablePrintDebugInfo(srptable, NUMBEROFNODESTOPRINT);
#endif
	MARK_NEW_ELEMENT(newElement, void *);
	return newElement;
}

/**
 * \brief       Remove an entry matching given key from the srp hash table
 * \ingroup     srp_hash_table
 *
 *
 * @param table         srp hash table
 * @param key        	A key should provide enough info to generate hash value and
						to do comparison with the entries in SRP hashtable.
 * @return              0 on success, 1 on failure
 *
 *	Removes an entry matching given key from the hash-table and frees it.
 */
U_32
srpHashTableRemove(J9SRPHashTable *srptable, void *key)
{
	J9SRP * srpnode;
	void * removedNode;

	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableRemove <%s>: table=%p, key=%p\n", srptable->tableName, srptable, key);
#endif

	srpnode = srpHashTableFindNode(srptable, key);
	removedNode = SRP_GET(*srpnode, void *);
	if (removedNode == NULL) {
#ifdef SRPHASHTABLE_DEBUG
		j9tty_printf(PORTLIB, "\nRESULT : \tENTRY DOES NOT EXIST\n\n");
#endif
		/* failed to find a node matching entry */
		return 1;
	}

	SRP_SET(*srpnode, SRP_GET(*(NEXT(srpnode)), void *));
	srptable->srpHashtableInternal->numberOfNodes--;
	simplepool_removeElement(SRP_GET(srptable->srpHashtableInternal->nodePool, J9SimplePool*), removedNode);

#ifdef SRPHASHTABLE_DEBUG
	srpHashTableDumpDistribution(srptable);
	srpHashTablePrintDebugInfo(srptable, NUMBEROFNODESTOPRINT);
#endif

	return 0;
}

/**
 * \brief       Call a user-defined function for all nodes in the SRP hash table
 * \ingroup     srp_hash_table
 *
 * @param srptable		SRP hash table
 * @param doFn          function to be called
 * @param opaque        user data to be passed to doFn
 *
 * Walks all nodes in the srp-hash-table, calling a user-defined function on the
 * user-data of each node.
 */
void
srpHashTableForEachDo(J9SRPHashTable *srptable, J9SRPHashTableDoFn doFn, void *opaque)
{
	void* node;
	U_32 bucketIndex = 0;
	J9SRPHashTableInternal * srptableInternal = srptable->srpHashtableInternal;
	J9SRP * nodes = J9SRPHASHTABLEINTERNAL_NODES(srptableInternal);
	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

	Assert_srphashtable_true(NULL != nodes);

#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srpHashTableForEachDo <%s>: table=%p\n", srptable->tableName, srptable);
#endif

	/* find the first non-empty bucket */
	while ((bucketIndex < srptableInternal->tableSize) && (0 == nodes[bucketIndex])) {
		bucketIndex += 1;
	}
	if (bucketIndex == srptableInternal->tableSize) {
		/* finished */
		return;
	} else {
		node = SRP_GET(nodes[bucketIndex],void *);
	}

	while (NULL != node) {
		doFn(node, opaque);

		/* advance to the next element in the bucket */
		node = SRP_GET(*((J9SRP *)((char *)node + srptableInternal->nodeSize - sizeof(J9SRP))) ,void *);

		/* if no nodes remain in the current bucket find the next non-empty bucket */
		while ((bucketIndex < srptableInternal->tableSize) && (NULL == node)) {
			bucketIndex += 1;
			node = SRP_GET(nodes[bucketIndex],void *);
		}

		if (bucketIndex == srptableInternal->tableSize) {
			/* finished */
			return;
		}
	}
	return;
}


#ifdef SRPHASHTABLE_DEBUG
/**
 * \brief       Dump the distribution of nodes in a srp-hash-table.
 * \ingroup     srp_hash_table
 *
 *
 * @param srptable
 *
 *      Walks the entire srp-hash-table, including overflow nodes, and dumps information
 *      about the distribution of nodes.
 */
void
srpHashTableDumpDistribution(J9SRPHashTable *srptable)
{
	U_32 i, nodeCount;
	U_32 usedNodes = 0;
	J9SRP * srpnodes;
	J9SRP * srpnode;
	U_32 nodeCounts[J9SRPHASHTABLE_DEBUG_NODE_COUNT_MAX];
	J9SRPHashTableInternal * srptableInternal;
	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

	srptableInternal = srptable->srpHashtableInternal;

	memset(nodeCounts, 0, sizeof(U_32) * J9SRPHASHTABLE_DEBUG_NODE_COUNT_MAX);
	srpnodes = J9SRPHASHTABLEINTERNAL_NODES(srptable->srpHashtableInternal);
	for (i = 0; i < srptableInternal->tableSize; i++) {
		srpnode = &(srpnodes[i]);

		if (*srpnode) {
			usedNodes++;
		}
		nodeCount = 0;
		while (*srpnode) {
			nodeCount++;
			srpnode = NEXT(srpnode);
		}

		if (nodeCount < J9SRPHASHTABLE_DEBUG_NODE_COUNT_MAX - 1) {
			nodeCounts[nodeCount]++;
		} else {
			nodeCounts[J9SRPHASHTABLE_DEBUG_NODE_COUNT_MAX - 1]++;
		}
	}

	j9tty_printf(PORTLIB, "SRP Hash Table <%s> [0x%p]\n", srptable->tableName, srptable);
	j9tty_printf(PORTLIB, "   |- used table-entries:      %d out of %d\n", usedNodes, srptableInternal->tableSize);
	j9tty_printf(PORTLIB, "   |- total entry count:       %d\n", srptableInternal->numberOfNodes);
	for (i = 0; i < J9SRPHASHTABLE_DEBUG_NODE_COUNT_MAX; i++) {
		if (nodeCounts[i] != 0) {
			j9tty_printf(PORTLIB, "   |- [%d] table-entries with: %d nodes in chain\n", nodeCounts[i], i);
		}
	}
}
#endif

#ifdef SRPHASHTABLE_DEBUG
/**
 * Prints the debug info of the value of elements in SRP hash table along with their addresses.
 *
 */
void
srpHashTablePrintDebugInfo(J9SRPHashTable *srptable, UDATA numberOfNodesToPrint)
{
	J9SRPHashTableInternal * srptableInternal;
	J9SRP * nodes;
	J9SimplePool * simplepool;
	void * sp_element;
	UDATA i;
	PORT_ACCESS_FROM_PORT(srptable->portLibrary);
	srptableInternal = srptable->srpHashtableInternal;
	nodes = SRP_GET(srptableInternal->nodes, J9SRP *);
	simplepool = SRP_GET(srptableInternal->nodePool, J9SimplePool *);

	sp_element = (void *)((char *)simplepool + simplepool_headerSize());

	j9tty_printf(PORTLIB, "\t\t\t\t     * PRINTING DEBUG INFO *\n");


	j9tty_printf(PORTLIB, "===>SRP HASH TABLE INTERNAL\n");
	j9tty_printf(PORTLIB, "tableSize : \t%d \n", srptableInternal->tableSize);

	j9tty_printf(PORTLIB, "===>SRP HASH TABLE\n");
	for(i = 0; i < numberOfNodesToPrint && i<srptableInternal->tableSize; i++) {
		j9tty_printf(PORTLIB, "SRP NODE[%d] Address : \t%d,\tValue :\t%d \n", i, &(nodes[i]), nodes[i]);
	}

	j9tty_printf(PORTLIB, "\n===>SIMPLEPOOL STRUCTURE\n");
	j9tty_printf(PORTLIB, "SimplePool->numElements : \t%d\n", simplepool->numElements);
	j9tty_printf(PORTLIB, "SimplePool->elementSize : \t%d\n", simplepool->elementSize);
	j9tty_printf(PORTLIB, "SimplePool->freeList : \t\t%d,\tPoints to :\t%d \n", simplepool->freeList, SRP_GET(simplepool->freeList, void *));
	j9tty_printf(PORTLIB, "SimplePool->firstFreeSlot :\t%d,\tPoints to :\t%d \n", simplepool->firstFreeSlot, SRP_GET(simplepool->firstFreeSlot, void *));
	j9tty_printf(PORTLIB, "SimplePool->blockEnd :\t\t%d,\tPoints to :\t%d \n\n", simplepool->blockEnd, SRP_GET(simplepool->blockEnd, void *));

	j9tty_printf(PORTLIB, "===>SIMPLEPOOL\n");

	for(i = 0; i < numberOfNodesToPrint && i<srptableInternal->tableSize; i++) {
		j9tty_printf(PORTLIB, "ELEMENT[%d] Address : \t\t%d,\tValue :\t%d \n", i, sp_element, *((UDATA *)(sp_element)));
		j9tty_printf(PORTLIB, "ELEMENT[%d]_SRP Address :\t%d,\tValue :\t%d \n", i, (J9SRP *)((char *)sp_element + srptableInternal->nodeSize - sizeof(J9SRP)), *((J9SRP *)((char *)sp_element + srptableInternal->nodeSize - sizeof(J9SRP))));
		j9tty_printf(PORTLIB, "ELEMENT[%d]_bytes4-8 :\tValue :\t%d \n", i, *((J9SRP *)((char *)sp_element + sizeof(J9SRP))));
		j9tty_printf(PORTLIB, "\n");
		sp_element = (void *)((char *)sp_element + srptableInternal->nodeSize);
	}
	j9tty_printf(PORTLIB, "srptableInternal->numberOfNodes = %d\n", srptableInternal->numberOfNodes);
	j9tty_printf(PORTLIB, "freelist = %d\n", J9SIMPLEPOOL_FREELIST(SRP_GET(srptableInternal->nodePool, J9SimplePool *)));
}
#endif


/**
 * \brief       Return the number of entries in a srp-hash-table
 * \ingroup     srp_hash_table
 *
 *
 * @param srptable
 * @return                  Number of srp hash table entries
 *
 */

U_32
srpHashTableGetCount(J9SRPHashTable *srptable)
{
	return srptable->srpHashtableInternal->numberOfNodes;

}

/**
 * \brief       Verify a srp hash table
 * \ingroup     srp_hash_table
 *
 *
 * @param memorySize			The size of the memory address
 * @param entrySize             Size of the user-data for each node
 * @return                      TRUE if the SRP hash table verifies
 *
 * Verifies a SRP hash table. Calls simplepool_verify() as well.
 * Ensure all possible fields in J9SRPHashTableInternal are correct.
 * Recompute the tableSize and verify its correct.
 * Verify the entrySize against the specified value.
 * Recompute the nodeSize and verify its correct.
 * Ensure only valid flags are set in the flags field.
 * Recompute the nodes and nodePool values and verify they are correct.
 */
BOOLEAN
srpHashTableVerify(
	J9SRPHashTable * srptable,
	U_32 memorySize,
	U_32 entrySize)
{
	J9SRPHashTableInternal *  srptableInternal = srptable->srpHashtableInternal;
	J9SRP * nodes = J9SRPHASHTABLEINTERNAL_NODES(srptableInternal);
	J9SimplePool * simplePool = J9SRPHASHTABLEINTERNAL_NODEPOOL(srptableInternal);
	J9SRP * currentSRP;
	UDATA numberOfSRPs = 0;
	UDATA i;
	U_32 recalculatedTableSize;
	BOOLEAN verifies = FALSE;

	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

	Trc_srpHashTableVerify_Entry(srptable, memorySize, entrySize);

	/* Verify entry size against the the specified value */
	if (entrySize != srptableInternal->entrySize) {
		Trc_srpHashTableVerify_entrySizeIncorrect(srptable, srptableInternal->entrySize, entrySize);
	/* Verify the nodeSize against the specified value */
	} else if ((entrySize + sizeof(J9SRP)) != srptableInternal->nodeSize) {
		Trc_srpHashTableVerify_nodeSizeIncorrect(srptable, srptableInternal->nodeSize, entrySize + sizeof(J9SRP));
	} else if (simplepool_numElements(simplePool) != srptableInternal->numberOfNodes) {
		Trc_srpHashTableVerify_numberOfNodesIncorrect(srptable, simplepool_numElements(simplePool), srptableInternal->numberOfNodes);
	} else  if (srptableInternal->tableSize == PRIMENUMBERHELPER_OUTOFRANGE){
		Trc_srpHashTableVerify_tableSizeIncorrect(srptable, srptableInternal->tableSize, PRIMENUMBERHELPER_OUTOFRANGE);
	} else if (NULL != nodes) {
		/* Recompute table size and verify it against the one stored in SRP hashtable strcuture */
		recalculatedTableSize = srpHashTable_calculateTableSize(memorySize, entrySize, FALSE);
		if (recalculatedTableSize != srptableInternal->tableSize) {
			Trc_srpHashTableVerify_tableSizeIncorrect(srptable, srptableInternal->tableSize, recalculatedTableSize);
		/* Verify simplepool address is correct */
		} else if (((UDATA)simplePool) != ((UDATA)nodes + ROUND_TO_SIZEOF_UDATA(srptableInternal->tableSize * sizeof(J9SRP)))) {
			Trc_srpHashTableVerify_simplePoolAddressIncorrect(srptable, simplePool, ((char *)nodes) + (srptableInternal->tableSize * sizeof(J9SRP)));
		/* Verify SimplePool */
		} else if (!simplepool_verify(
			simplePool,
			simplepool_headerSize() + srptableInternal->tableSize * srptableInternal->nodeSize,
			entrySize + sizeof(J9SRP))) {
			/* DO NOTHING. */
		/* Ensure only valid flags are set in the flags field */
		} else if (srptableInternal->flags != 0) {
			Trc_srpHashTableVerify_flagsIncorrect(srptable, srptableInternal->flags, 0);
		/* Recompute the nodes and nodePool values and verify they are correct. */
		} else if (((UDATA)nodes) != ((UDATA)srptableInternal + sizeof(J9SRPHashTableInternal))) {
			Trc_srpHashTableVerify_nodesAddressIncorrect(srptable, nodes, ((char *)srptableInternal) + sizeof(J9SRPHashTableInternal));
		} else {
			verifies = TRUE;
		}
	}

	/* Check consistency of SRP nodes */
	for(i = 0; verifies && (i < srptableInternal->tableSize); i++) {
		currentSRP = &nodes[i];
		while(verifies && (*currentSRP != 0)) {
			if (!simplepool_isElement(simplePool, SRP_GET(*currentSRP, void *))) {
				Trc_srpHashTableVerify_SRPPointsToNonSimplepoolElement(currentSRP, *currentSRP, SRP_GET(*currentSRP, void *), srptable);
				verifies = FALSE;
			}
			numberOfSRPs++;
			currentSRP = NEXT(currentSRP);
		}
	}

	/* Verify number of SRPs is same with number of entries */
	if (verifies && (simplepool_numElements(simplePool) != numberOfSRPs)) {
		Trc_srpHashTableVerify_numOfElementsIncorrect(srptable, simplepool_numElements(simplePool), numberOfSRPs);
		verifies = FALSE;
	}

	Trc_srpHashTableVerify_Exit(verifies);
	return verifies;
}

/**
 *	Calls a user provided function for each element in the simple pool skipping over
 *	skipCount allocated elements each time. If the function returns FALSE, the iteration
 *	stops.
 *
 * @param[in] srptable		The SRP hash table to check consistency
 * @param[in] portLib		A pointer to the port library, (this is required for j9mem_allocate/free functions)
 * @param[in] doFunction	Pointer to function which will "do" things to the elements of simple pool. Return TRUE
 * 							to continue iterating, FALSE to stop iterating.
 * @param[in] userData		Pointer to data to be passed to "do" function, along with each simple pool element
 * @param[in] skipCount		Number of elements to skip after each iteration, 0 means every element will be iterated over
 *
 * @return BOOLEAN	Returns FALSE if the doFunction returns FALSE for any element, TRUE otherwise
 */
BOOLEAN
srpHashTable_checkConsistency(J9SRPHashTable* srptable, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void* anElement, void* userData), void* userData, UDATA skipCount)
{
	J9SimplePool * simplePool = J9SRPHASHTABLEINTERNAL_NODEPOOL(srptable->srpHashtableInternal);
	return simplepool_checkConsistency(simplePool, portLib, doFunction, userData, skipCount);
}

/**
 * \brief       Find a srp node that points to the same entry in the simple pool.
 * \ingroup     srphash_table
 *
 *@param srptable	SRP Hash Table
 *@param key		A key should provide enough info to generate hash value and
 					to do comparison with the entries in SRP hashtable to find the entry to remove.
 *@return			NULL if SRP hash table nodes can not be found,
 *					otherwise;
 *					if the entry exist in simple pool,
 *					then a J9SRP pointer that points to entry in the simple pool,
 *					if it does not exist in simple pool,
 *					then a J9SRP pointer to the next SRP node where SRP of this entry can be set.
 *
 */
static J9SRP *
srpHashTableFindNode(J9SRPHashTable *srptable, void *key)
{
	UDATA hash;
	J9SRP * srpnode;
	J9SRP * srpnodes;
	PORT_ACCESS_FROM_PORT(srptable->portLibrary);

	Trc_srpHashTableFindNode_Entry(srptable, key);

	/* calculate the key hash */
	hash = srptable->hashFn(key, srptable->functionUserData) % srptable->srpHashtableInternal->tableSize;
#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "HASH = %d, ENTRY VALUE = %d\n", hash, *((UDATA *)key));
#endif

	/* find the right node for given key hash */
	srpnodes = J9SRPHASHTABLEINTERNAL_NODES(srptable->srpHashtableInternal);
	if (srpnodes == NULL) {
		Trc_srpHashTableFindNode_srptableNodesCanNotBeLocated(srptable);
		Trc_srpHashTableFindNode_Exit(NULL, NULL);
		return NULL;
	}

	srpnode = &(srpnodes[hash]);

#ifdef SRPHASHTABLE_DEBUG
	j9tty_printf(PORTLIB, "srphashTableFindNode <%s>:  key=%p hash=%x in node=%x->%x\n", srptable->tableName, key, hash, srpnode, srpnode);
#endif
	/*
	 * Look through the node looking for the correct key, use user supplied
	 * key compare function if available
	 */
	while (*srpnode != 0 && (srptable->hashEqualFn(SRP_GET(*srpnode, void *), key, srptable->functionUserData) == 0) ) {
		srpnode = NEXT(srpnode);
	}

	Trc_srpHashTableFindNode_Exit(srpnode, SRP_GET(*srpnode, void *));

	return srpnode;
}

/**
 * Calculates the required memory size for a given tableSize and entrySize
 * @param tableSize		Preferred table size passed by user. It is ceil up or down to the prime number.
 * @param entrySize 	Size of user entry
 * @param ceilUp		If TRUE, tableSize is ceil up to next bigger prime number,
 * 						otherwise, tableSize is set to next lower prime number
 * @return min required memory size. Return zero if the recalculated tableSize is 0
 *
 */
U_32
srpHashTable_requiredMemorySize(U_32 tableSize, U_32 entrySize, BOOLEAN ceilUp)
{
	U_32 requiredMinSize;
	if (ceilUp) {
		tableSize = (U_32)findSmallestPrimeGreaterThanOrEqualTo(tableSize);
	} else {
		tableSize = (U_32)findLargestPrimeLessThanOrEqualTo(tableSize);
	}

	if (tableSize == 0) {
		requiredMinSize = 0;
	} else if (tableSize == PRIMENUMBERHELPER_OUTOFRANGE){
		requiredMinSize = PRIMENUMBERHELPER_OUTOFRANGE;
	} else {
		requiredMinSize = sizeof(J9SRPHashTableInternal) +
			ROUND_TO_SIZEOF_UDATA(sizeof(J9SRP) * tableSize) +
			simplepool_totalSize(entrySize + sizeof(J9SRP), tableSize);
	}
	return requiredMinSize;

}

/**
 * Calculates the tableSize (max number of elements that can be stored in SRPHashTable) of SRPHashTable for a given memory.
 * If user is not restricted with the given memory size, then tableSize is ceil up to next prime number.
 * Otherwise, it is set to previous prime number.
 * If the given memory is large enough and can hold more elements than supported maximum size by primeNumberHelper,
 * then this function returns PRIMENUMBERHELPER_OUTOFRANGE.
 * It is caller's responsibility to decide what to do in such cases.
 *
 * @param  memorySize Size of memory
 * @param entrySize Size of entry
 * @param canExtendGivenMemory	True, if user can use a bigger memory to create SRPHashtable,
 * False otherwise
 * @return calculated table size
 */
U_32
srpHashTable_calculateTableSize(U_32 memorySize, U_32 entrySize, BOOLEAN canExtendGivenMemory)
{
	U_32 simplePoolNodeSize = 0;
	U_32 requiredSizePerEntry = 0;
	U_32 remainingMemorySize = 0;
	U_32 tableSize = 0;
	U_32 totalHeaderSize = 0;

	if (entrySize == 0) {
		return 0;
	}
	/*Calculate the sizes of each member in the allocated memory*/
	simplePoolNodeSize = entrySize + sizeof(J9SRP);

	/*
	 * For each entry, we need a srp hashtable node and
	 * simplepool node which consists of entry and SRP to point to the next element
	 */
	totalHeaderSize = sizeof(J9SRPHashTableInternal) + simplepool_headerSize();
	if (memorySize <= totalHeaderSize) {
		return 0;
	}
	remainingMemorySize = memorySize - totalHeaderSize;
	requiredSizePerEntry = simplePoolNodeSize + sizeof(J9SRP);

	if (canExtendGivenMemory) {
		tableSize = (U_32)findSmallestPrimeGreaterThanOrEqualTo(remainingMemorySize / requiredSizePerEntry);
	} else {
		tableSize = (U_32)findLargestPrimeLessThanOrEqualTo(remainingMemorySize / requiredSizePerEntry);
		/*
		 * If tableSize is PRIMENUMBERHELPER_OUTOFRANGE, then given memory size is not supported by primeNumberHelper primes.
		 * If this is the case, just return it and do not do next if statement which checks padding.
		 */
		if (tableSize == PRIMENUMBERHELPER_OUTOFRANGE) {
			return PRIMENUMBERHELPER_OUTOFRANGE;
		}
		/*
		 * Recalculate the min size of memory required for the table size calculated above.
		 * In the case of padding taking place while rounding up,
		 * recalculated min required memory size should be higher than memory size passed by user of this function.
		 * If this is the case, then get the next smaller prime number as table size
		 */
		if (srpHashTable_requiredMemorySize( (remainingMemorySize / requiredSizePerEntry), entrySize, FALSE) > memorySize) {
			tableSize = (U_32)findLargestPrimeLessThanOrEqualTo(tableSize - 1);
		}
	}
	return tableSize;
}

/**
 * Returns the table size of the SRP Hashtable
 * @param srptable	SRP Hashtable of which table size is asked for
 * @return size of the given SRP Hashtable
 */
U_32
srpHashTable_tableSize(J9SRPHashTable *srptable)
{
	return srptable->srpHashtableInternal->tableSize;
}

