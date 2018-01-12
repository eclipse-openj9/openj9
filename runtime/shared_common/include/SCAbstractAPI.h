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

#if !defined(J9SC_ABSTRACT_API_H_INCLUDED)
#define J9SC_ABSTRACT_API_H_INCLUDED

#include "j9.h"
#include "j9generated.h"
#include "SCTransactionCTypes.h"


#ifdef __cplusplus
extern "C"
{
#endif

/*Return codes for stringTransaction_stop()*/
#define SCSTRING_TRANSACTION_STOP_ERROR -1

/*Return codes for classStoreTransaction_stop()*/
#define SCCLASS_STORE_STOP_ERROR -1
#define SCCLASS_STORE_STOP_NOTHING_STORED 0
#define SCCLASS_STORE_STOP_DATA_STORED 1

typedef struct SCAbstractAPI
{
	/*Functions for SCStringTransaction*/
	IDATA (*stringTransaction_start)(void * tobj, J9VMThread* currentThread);
	IDATA (*stringTransaction_stop)(void * tobj);
	BOOLEAN (*stringTransaction_IsOK)(void * tobj);

	/*Functions for SCStoreTransaction*/
	IDATA (*classStoreTransaction_start)(void * tobj, J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry* classPathEntries, UDATA cpEntryCount, UDATA entryIndex, UDATA loadType, const J9UTF8* partition, U_16 classnameLength, U_8 * classnameData, BOOLEAN isModifiedClassfile, BOOLEAN takeReadWriteLock);
	IDATA (*classStoreTransaction_stop)(void * tobj);
	J9ROMClass * (*classStoreTransaction_nextSharedClassForCompare)(void * tobj);
	IDATA (*classStoreTransaction_createSharedClass)(void * tobj, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces);
	IDATA (*classStoreTransaction_updateSharedClassSize)(void * tobj, U_32 sizeUsed);
	BOOLEAN (*classStoreTransaction_isOK)(void * tobj);
	BOOLEAN (*classStoreTransaction_hasSharedStringTableLock)(void * tobj);

	/*Function for JCL to update cache metadata for an existing shared class*/
	J9ROMClass * (*jclUpdateROMClassMetaData)(J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry* classPathEntries, UDATA cpEntryCount, UDATA entryIndex, const J9UTF8* partition, const J9ROMClass * existingClass);

	/* Functions for finishing initialization of shared classes. Called between
	 * AGENTS_STARTED & ABOUT_TO_BOOTSTRAP by jvminit.c
	 */
	IDATA (*sharedClassesFinishInitialization)(J9JavaVM *vm);

	/* Functions to query the state of shared classes.
	 */
	BOOLEAN (*isCacheFull)(J9JavaVM *vm);
	BOOLEAN (*isAddressInCache)(J9JavaVM *vm, void *address, UDATA length);

	/* Get shared classes defaults for -verbose:sizes
	 */
	void (*populatePreinitConfigDefaults)(J9JavaVM *vm, J9SharedClassPreinitConfig *updatedWithDefaults);

} SCAbstractAPI;

#ifdef __cplusplus
}/*extern "C"*/
#endif

#endif /* J9SC_ABSTRACT_API_H_INCLUDED */
