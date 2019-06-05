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

#include "ClasspathItem.hpp"
#include "sharedconsts.h"
#include "ut_j9shr.h"
#include "j9shrnls.h"
#include <stdlib.h>
#include <string.h>

#define CLASSPATHITEM_ERR_TRACE(var) j9nls_printf(PORTLIB, J9NLS_ERROR, var);
#define HEADER_SIZE sizeof(ClasspathItem)
#define CPEI_ARRAY_PTR_FROM_CPI(cpiPtr) (IDATA*)((BlockPtr)cpiPtr + HEADER_SIZE)
#define UPDATE_CPEI_ARRAY_PTR_FROM_CPI(cpiPtr) (IDATA*)(cpiPtr += HEADER_SIZE)

ClasspathEntryItem*
ClasspathEntryItem::newInstance(const char* path, U_16 pathLen, UDATA protocol, ClasspathEntryItem* memForConstructor)
{
	ClasspathEntryItem* newCPEI = (ClasspathEntryItem*)memForConstructor;

	new(newCPEI) ClasspathEntryItem();
	if (newCPEI->initialize(path, pathLen, protocol)==0) {
		return newCPEI;
	} else {
		return NULL;
	}
}

IDATA
ClasspathEntryItem::initialize(const char* path_, U_16 pathLen_, UDATA protocol_)
{
	flags = 0;
	protocol = protocol_;
	/* important to initialize timestamp to -1 */
	timestamp = -1;
	hashValue = 0;

	path = (char*)path_;
	pathLen = pathLen_;
	locationPathLen = pathLen_;
	if (protocol == PROTO_JAR) {
		if (NULL != path) {
			char* jarPath = strstr(path,"!/");
			if (NULL == jarPath) {
				jarPath = strstr(path,"!\\");
			}
			if (NULL != jarPath) {
				locationPathLen = jarPath - path;
			}
		}
	}

	return 0;
}

/* Assume we have enough space */
ClasspathEntryItem::BlockPtr 
ClasspathEntryItem::writeToAddress(BlockPtr block)
{
	ClasspathEntryItem* cpeiInCache = (ClasspathEntryItem*)block;
	IDATA paddedPathSize = SHC_PAD(pathLen, SHC_WORDALIGN);
	BlockPtr pathInCache = block + sizeof(ClasspathEntryItem);
	
	memcpy(block, this, sizeof(ClasspathEntryItem));
	strncpy(pathInCache, path, pathLen);
	/* Mark cache entry so that we know how to read it */
	cpeiInCache->flags |= IS_IN_CACHE_FLAG;
	return (pathInCache + paddedPathSize);
}

/* If this is view on cache, return correct ptr. Otherwise, return known ptr. */
const char* 
ClasspathEntryItem::getPath(U_16* pathLen_) const 
{
	if (pathLen_) {
		*pathLen_ = (U_16)pathLen;
	}
	if ((flags & IS_IN_CACHE_FLAG)==0) {
		return path;
	} else {
		return (((BlockPtr)this) + sizeof(ClasspathEntryItem));
	}
}

U_32 
ClasspathEntryItem::getSizeNeeded(void) const
{
	return (sizeof(ClasspathEntryItem) + SHC_PAD((U_32)pathLen, SHC_WORDALIGN));
} 

UDATA
ClasspathEntryItem::hash(J9InternalVMFunctions* functionTable)
{
	U_16 pathLen = 0;
	const char* path = getPath(&pathLen);

	if (!hashValue) {
		return (hashValue = (functionTable->computeHashForUTF8((U_8*)path, pathLen) + protocol));
	}
	return hashValue;
}

const char*
ClasspathEntryItem::getLocation(U_16* locationPathLen_) const
{
	if (locationPathLen_) {
		*locationPathLen_ = (U_16)locationPathLen;
	}
	if ((flags & IS_IN_CACHE_FLAG)==0) {
		return path;
	} else {
		return (((BlockPtr)this) + sizeof(ClasspathEntryItem));
	}
}

void
ClasspathEntryItem::cleanup(void)
{
	/* Currently no cleanup */
}

ClasspathItem*
ClasspathItem::newInstance(J9JavaVM* vm, IDATA entries_, IDATA helperID_, U_16 cpType_, ClasspathItem* memForConstructor)
{
	ClasspathItem* newCPI = (ClasspathItem*)memForConstructor;

	new(newCPI) ClasspathItem();
	newCPI->initialize(vm, entries_, helperID_, cpType_, ((BlockPtr)memForConstructor + sizeof(ClasspathItem)));

	return newCPI;
}

UDATA
ClasspathItem::getRequiredConstrBytes(UDATA entries)
{
	UDATA reqBytes = 0;

	reqBytes += sizeof(ClasspathItem);
	reqBytes += (entries * (sizeof(ClasspathEntryItem) + sizeof(ClasspathEntryItem*)));

	return reqBytes;
}

void
ClasspathItem::initialize(J9JavaVM* vm, IDATA entries_, IDATA helperID_, U_16 cpType_, BlockPtr memForConstructor) {
	type = cpType_;
	flags = 0;
	entries = entries_;
	portlib = vm->portLibrary;
	helperID = helperID_;
	itemsAdded = 0;
	firstDirIndex = -1;
	hashValue = 0;
	jarsLockedToIndex = -1;

	Trc_SHR_CPI_initialize_Entry(helperID_, entries_, cpType_);

	items = (ClasspathEntryItem**)memForConstructor;
	for (IDATA i=0; i<entries_; i++) {
		items[i] = (ClasspathEntryItem*)(((UDATA)items) + ((entries_ * sizeof(ClasspathEntryItem*))
										 + (sizeof(ClasspathEntryItem) * i)));
	}

	Trc_SHR_CPI_initialize_Exit();
}

/* Returns -1 if addItem fails otherwise total number of items added */
IDATA 
ClasspathItem::addItem(J9InternalVMFunctions* functionTable, const char* path, U_16 pathLen, UDATA protocol)
{
	ClasspathEntryItem* current = NULL;

	Trc_SHR_CPI_addItem_Entry(pathLen, path, protocol);

	if (entries==itemsAdded) {
		/* Cannot access verbose level, so this is not suppressed by "silent". However, it's a "should never happen" message. */
		PORT_ACCESS_FROM_PORT(portlib);
		CLASSPATHITEM_ERR_TRACE(J9NLS_SHRC_CPI_TOO_MANY_ITEMS);
		Trc_SHR_CPI_addItem_ExitTooMany();
		Trc_SHR_Assert_ShouldNeverHappen();
		return -1;
	}
	current = ClasspathEntryItem::newInstance(path, pathLen, protocol, items[itemsAdded]);
	
	if (!current) {
		Trc_SHR_CPI_addItem_ExitError();
		return -1;
	}
	if (protocol==PROTO_DIR && firstDirIndex==-1) {
		firstDirIndex = itemsAdded;
	}
	hashValue += current->hash(functionTable);	
	++itemsAdded;

	Trc_SHR_CPI_addItem_Exit(itemsAdded);

	return itemsAdded;
}

/* Either knows directly about its items, or calculates using offsets in the cache */
ClasspathEntryItem* 
ClasspathItem::itemAt(I_16 i) const
{
	Trc_SHR_CPI_itemAt_Entry(i);

	if (i>=itemsAdded) {
		Trc_SHR_CPI_itemAt_ExitError(itemsAdded);
		Trc_SHR_Assert_ShouldNeverHappen();
		return NULL;
	}
	if (!(flags & IS_IN_CACHE_FLAG)) {
		if (!items) {
			Trc_SHR_CPI_itemAt_NotInitialized();
			return NULL;		/* Not initialized yet */
		} else {
			Trc_SHR_CPI_itemAt_ExitLocal();
			return items[i];
		}
	} else {
		IDATA* offsets = CPEI_ARRAY_PTR_FROM_CPI(this);

		Trc_SHR_CPI_itemAt_ExitInCache();
		return (ClasspathEntryItem*)((BlockPtr)this + offsets[i]);
	}
}

I_16 
ClasspathItem::find(J9InternalVMFunctions* functionTable, ClasspathEntryItem* test) const
{
	return find(functionTable, test, -1);
}

/* Does not compare timestamps or flags */
bool /* static */ 
ClasspathItem::compare(J9InternalVMFunctions* functionTable, ClasspathEntryItem* test, ClasspathEntryItem* compareTo)
{
	U_16 testPathLen = 0;
	U_16 comparePathLen = 0;
	U_8* testPath = NULL;
	U_8* comparePath = NULL;
	UDATA hash1, hash2;

	Trc_SHR_CPI_compare_CPEI_Entry(test, compareTo);

	if (test==compareTo) {
		Trc_SHR_CPI_compare_CPEI_ExitSame();
		return true;
	}

	if (test==NULL || compareTo==NULL) {
		Trc_SHR_CPI_compare_CPEI_ExitNull();
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}

	testPath = (U_8*)test->getPath(&testPathLen);
	comparePath = (U_8*)compareTo->getPath(&comparePathLen);

	Trc_SHR_CPI_compare_CPEI_Paths(testPathLen, testPath, comparePathLen, comparePath);

	hash1 = test->hash(functionTable);
	hash2 = compareTo->hash(functionTable);
	if (hash1 != hash2) {
		Trc_SHR_CPI_compare_CPEI_ExitHash(hash1, hash2);
		return false;
	}

	if (test->protocol != compareTo->protocol) {
		Trc_SHR_CPI_compare_CPEI_ExitProtocol(test->protocol, compareTo->protocol);
		return false;
	}

	if (!J9UTF8_DATA_EQUALS(testPath, testPathLen, comparePath, comparePathLen)) {
		Trc_SHR_CPI_compare_CPEI_ExitCompare();
		return false;
	}
	Trc_SHR_CPI_compare_CPEI_ExitSuccess();
	return true;
}

/* NOTE: Should not compare getType(). A 1-element URL should be the same as a 1-element Classpath. */
bool /* static */
ClasspathItem::compare(J9InternalVMFunctions* functionTable, ClasspathItem* test, ClasspathItem* compareTo)
{
	Trc_SHR_CPI_compare_Entry(test, compareTo);

	if (test==compareTo) {
		Trc_SHR_CPI_compare_ExitTrue1();
		return true;
	}
	if (test==NULL || compareTo==NULL) {
		Trc_SHR_CPI_compare_ExitNullError();
		Trc_SHR_Assert_ShouldNeverHappen();
		return false;
	}
	if (test->getItemsAdded()!=compareTo->getItemsAdded()) {
		Trc_SHR_CPI_compare_ExitFalse1();
		return false;
	}
	if (test->getHashCode()!=compareTo->getHashCode()) {
		Trc_SHR_CPI_compare_ExitFalse2();
		return false;
	}
	for (I_16 i=0; i<(I_16)test->itemsAdded; i++) {
		if (!compare(functionTable, test->itemAt(i), compareTo->itemAt(i))) {
			Trc_SHR_CPI_compare_ExitFalse4(i);
			return false;
		}
	}
	Trc_SHR_CPI_compare_ExitTrue2();
	return true;
}

/* PERFORMANCE: Since most classpath matching will be done comparing the same classpath,
 * the quickest patch to compare is to work right to left, only starting at the index
 * we're interested in.
 */
I_16 
ClasspathItem::find(J9InternalVMFunctions* functionTable, ClasspathEntryItem* test, I_16 stopAtIndex) const
{
	I_16 countFrom = 0;

	Trc_SHR_CPI_find_Entry(test, stopAtIndex);

	/* stopAtIndex should hopefully never be greater than itemsAdded! */
	if ((stopAtIndex==-1) || (stopAtIndex >= (I_16)itemsAdded)) {
		countFrom = (I_16)itemsAdded - 1;
	} else {
		countFrom = stopAtIndex;
	}

	for (I_16 i=countFrom; i>=0; i--) {
		if (compare(functionTable, itemAt(i), test)) {
			Trc_SHR_CPI_find_ExitFound(i);
			return i;
		}
	}

	Trc_SHR_CPI_find_ExitNotFound();
	return -1;
}

IDATA
ClasspathItem::getMaxItems(void) const
{
	return entries;
}

I_16 
ClasspathItem::getItemsAdded(void) const
{
	return (I_16)itemsAdded;
}

/* Assume we have enough memory at block to write whole item */
IDATA 
ClasspathItem::writeToAddress(BlockPtr block) 
{
	BlockPtr cursor = block;
	IDATA* arrayPtr = NULL;			/* Array of offsets */
	ClasspathItem* cpiInCache = (ClasspathItem*)block;

	Trc_SHR_CPI_writeToAddress_Entry(block);

	memcpy(cpiInCache, this, sizeof(ClasspathItem));
	arrayPtr = UPDATE_CPEI_ARRAY_PTR_FROM_CPI(cursor);
	cursor += itemsAdded * (sizeof(IDATA));
	for (I_16 i=0; i<(I_16)itemsAdded; i++) {
		/* store the offset */
		*(arrayPtr++) = (cursor - block);
		cursor = itemAt(i)->writeToAddress(cursor);
	}
	/* Indicate that this is stored in cache */
	cpiInCache->flags |= IS_IN_CACHE_FLAG;

	Trc_SHR_CPI_writeToAddress_Exit();

	return 0;
}

/* Returns amount of size needed in cache to write out complete ClasspathItem */
U_32 
ClasspathItem::getSizeNeeded(void) const
{
	U_32 total = HEADER_SIZE + ((U_32)itemsAdded*sizeof(IDATA));
	
	for (I_16 i=0; i<(I_16)itemsAdded; i++) {
		total += itemAt(i)->getSizeNeeded();
	}
	return total;
}

/* Only valid/interesting for ClasspathItems created locally (not in cache) */
IDATA 
ClasspathItem::getHelperID(void) const
{
	if (!(flags & IS_IN_CACHE_FLAG)) {
		return helperID;
	} else {
		return HELPERID_NOT_APPLICABLE;
	}
}

IDATA 
ClasspathItem::getFirstDirIndex(void) const
{
	return firstDirIndex;
}

UDATA
ClasspathItem::getHashCode(void) const
{
	return hashValue;
}

UDATA
ClasspathItem::getType(void) const
{
	return type;
}

bool
ClasspathItem::isInCache(void) const
{
	return ((flags & IS_IN_CACHE_FLAG) != 0);
}

void
ClasspathItem::setJarsLockedToIndex(I_16 i)
{
	if (!(flags & IS_IN_CACHE_FLAG)) {
		jarsLockedToIndex = i;
	}
}

I_16
ClasspathItem::getJarsLockedToIndex(void) const
{
	if (!(flags & IS_IN_CACHE_FLAG)) {
		return (I_16)jarsLockedToIndex;
	} else {
		return -1;
	}
}

void
ClasspathItem::cleanup(void)
{
	if (!(flags & IS_IN_CACHE_FLAG)) {
		if (items) {
			for (int i=0; i<itemsAdded; i++) {
				items[i]->cleanup();
			}
		}
	}
}

