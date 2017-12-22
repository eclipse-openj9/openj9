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
#if !defined(SCIMPLEMENTEDAPI_H_)
#define SCIMPLEMENTEDAPI_H_

/* @ddr_namespace: default */
#include "SCAbstractAPI.h"

#ifdef __cplusplus
extern "C"
{
#endif

IDATA j9shr_stringTransaction_start(void * tobj, J9VMThread* currentThread);
IDATA j9shr_stringTransaction_stop(void * tobj);
BOOLEAN j9shr_stringTransaction_IsOK(void * tobj);

IDATA j9shr_classStoreTransaction_start(void * tobj, J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry* classPathEntries, UDATA cpEntryCount, UDATA entryIndex, UDATA loadType, const J9UTF8* partition, U_16 classnameLength, U_8 * classnameData, BOOLEAN isModifiedClassfile, BOOLEAN takeReadWriteLock);
IDATA j9shr_classStoreTransaction_stop(void * tobj);
J9ROMClass *j9shr_classStoreTransaction_nextSharedClassForCompare(void * tobj);
IDATA j9shr_classStoreTransaction_createSharedClass(void * tobj, const J9RomClassRequirements * sizes, J9SharedRomClassPieces * pieces);
IDATA j9shr_classStoreTransaction_updateSharedClassSize(void * tobj, U_32 sizeUsed);
BOOLEAN j9shr_classStoreTransaction_isOK(void * tobj);
BOOLEAN j9shr_classStoreTransaction_hasSharedStringTableLock(void * tobj);

J9ROMClass * j9shr_jclUpdateROMClassMetaData(J9VMThread* currentThread, J9ClassLoader* classloader, J9ClassPathEntry* classPathEntries, UDATA cpEntryCount, UDATA entryIndex, const J9UTF8* partition, const J9ROMClass * existingClass);

#ifdef __cplusplus
}/*extern "C"*/
#endif

#endif /* SCIMPLEMENTEDAPI_H_ */
