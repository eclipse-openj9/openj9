/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#if !defined(J9SC_STORE_TRANSACTION_HPP_INCLUDED)
#define J9SC_STORE_TRANSACTION_HPP_INCLUDED

#include "j9.h"
#include "j9generated.h"
#include "SCAbstractAPI.h"

class SCStoreTransaction
{
public:
	/**
	 * Construct a transaction for storing a shared class
	 */
	SCStoreTransaction(J9VMThread* currentThread, J9ClassLoader* classloader, UDATA entryIndex, UDATA loadType, U_16 classnameLength, U_8 * classnameData, bool isModifiedClassfile, bool isIntermediateROMClass = false)
	{
		if (currentThread != NULL && currentThread->javaVM != NULL && currentThread->javaVM->sharedClassConfig != NULL) {
			sharedapi = (SCAbstractAPI *)(currentThread->javaVM->sharedClassConfig->sharedAPIObject);
			memset ((void*)&pieces,0,sizeof(J9SharedRomClassPieces));
			if (sharedapi != NULL) {
				J9ClassPathEntry* classPathEntries = NULL;
				UDATA cpEntryCount = 0;
				if ((NULL != classloader) && (false == isIntermediateROMClass)) {
					if (classloader == currentThread->javaVM->systemClassLoader) {
						classPathEntries = classloader->classPathEntries;
						cpEntryCount = classloader->classPathEntryCount;
					} 
				}
				sharedapi->classStoreTransaction_start((void *) &tobj, currentThread, classloader, classPathEntries, cpEntryCount, entryIndex, loadType, NULL, classnameLength, classnameData, ((isModifiedClassfile == true)?TRUE:FALSE), TRUE);
			}
		} else {
			sharedapi = NULL;
		}
		return;
	}

	/**
	 * Destruct a transaction for storing a shared class
	 */
	~SCStoreTransaction()
	{
		if ((sharedapi != NULL)) {
			sharedapi->classStoreTransaction_stop((void *) &tobj);
		}
		sharedapi = NULL;
	}

	/**
	 * Returns true if the transaction object is OK.
	 *
	 * Called after constructor/method is called to query the state of the class.
	 *
	 */
	bool isOK()
	{
		if (sharedapi != NULL) {
			if (sharedapi->classStoreTransaction_isOK((void *) &tobj) == TRUE) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Returns true if the transaction owns the shared string table.
	 *
	 * Called after constructor/method is called to query the state of the class.
	 *
	 */
	bool hasSharedStringTableLock()
	{
		if (sharedapi != NULL) {
			if (sharedapi->classStoreTransaction_hasSharedStringTableLock((void *) &tobj) == TRUE) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Returns NULL when there are no ROMClasses to compare.
	 */
	J9ROMClass *nextSharedClassForCompare()
	{
		if (sharedapi != NULL) {
			return sharedapi->classStoreTransaction_nextSharedClassForCompare((void *) &tobj);
		}
		return NULL;
	}

	/**
	 * Call if no existing shared class is found. Returns the address to write the shared class
	 * or NULL if store is not possible (i.e. cache full, etc).
	 */
	void *createSharedClass(U_32 sizeRequired)
	{
		if (sharedapi != NULL) {
			J9RomClassRequirements sizes;
			memset ((void*)&sizes,0,sizeof(J9RomClassRequirements));
			sizes.romClassSizeFullSize = sizeRequired;
			sizes.romClassMinimalSize = sizeRequired;
			if (true == allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
				return getRomClass();
			}
		}
		return NULL;
	}

	/**
	 * Allocate all the memory required for a J9ROMClass
	 */
	bool allocateSharedClass(const J9RomClassRequirements * sizes)
	{
		if (sharedapi != NULL) {
			if (0 != sharedapi->classStoreTransaction_createSharedClass((void *) &tobj, sizes, &pieces)) {
				return false;
			}
			return true;
		}
		return false;
	}

	/**
	 * Return 'true' the memory returned by 'getRomClass' be romClassSizeFullSize bytes.
	 * Note: If 'allocateSharedClass' failed the result of this method is meaningless.
	 */
	bool hasAllocatedFullSizeRomClass(void) {
		return (0 != (pieces.flags & J9SC_ROMCLASS_PIECES_USED_FULL_SIZE));
	}

	/**
	 * Return 'true' if out-of-line memory was allocated for debug data by 
	 * 'allocateSharedClass'.
	 * Note: If 'allocateSharedClass' failed the result of this method is meaningless.
	 */
	bool debugDataAllocatedOutOfLine(void) {
		return (0 != (pieces.flags & J9SC_ROMCLASS_PIECES_DEBUG_DATA_OUT_OF_LINE));
	}

	/**
	 * Get 'RomClass' memory allocated by call to 'allocateSharedClass'
	 */
	void * getRomClass(void) {
		return pieces.romClass;
	}
	
	/**
	 * Get 'LineNumberTable' memory allocated by call to 'allocateSharedClass'
	 */
	void * getLineNumberTable(void) {
		return pieces.lineNumberTable;
	}

	/**
	 * Get 'LocalVariableTable' memory allocated by call to 'allocateSharedClass'
	 */
	void * getLocalVariableTable(void) {
		return pieces.localVariableTable;
	}

	/**
	 * Update the size of a shared class requested in call to 'createSharedClass'.
	 */
	IDATA updateSharedClassSize(U_32 sizeUsed)
	{
		if (sharedapi != NULL) {
			return sharedapi->classStoreTransaction_updateSharedClassSize((void *) &tobj, sizeUsed);
		}
		return -1;

	}

private:
	SCAbstractAPI * sharedapi;
	J9SharedClassTransaction tobj;
	J9SharedRomClassPieces pieces;

	/*
	 * We expect this class to always be allocated on the stack so new and delete are private
	 */
	void operator delete(void *)
	{
		return;
	}

	void *operator new(size_t size) throw ()
	{
		return NULL;
	}

};

#endif /*J9SC_STORE_TRANSACTION_HPP_INCLUDED*/
