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
#include "j9.h"

#ifndef jithash_h
#define jithash_h

#ifdef __cplusplus
extern "C" {
#endif

void hash_jit_free(J9PortLibrary * portLibrary, J9JITHashTable * table);
J9JITHashTable *hash_jit_allocate(J9PortLibrary * portLibrary, UDATA start, UDATA end);
UDATA hash_jit_artifact_insert_range(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToInsert, UDATA startPC, UDATA endPC);
J9JITExceptionTable * hash_jit_next_do(J9JITHashTableWalkState* walkState);
J9JITHashTable* hash_jit_toJ9MemorySegment(J9JITHashTable * table, J9MemorySegment * codeCache, J9MemorySegment * dataCache);
J9JITExceptionTable * hash_jit_start_do(J9JITHashTableWalkState* walkState, J9JITHashTable* table);
UDATA hash_jit_artifact_remove(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToRemove);
UDATA hash_jit_artifact_insert(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToInsert);
J9JITExceptionTable** hash_jit_allocate_method_store(J9PortLibrary *portLibrary, J9JITHashTable *table);
J9JITExceptionTable** hash_jit_artifact_array_insert(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable** array, J9JITExceptionTable *dataToInsert, UDATA startPC);
J9JITExceptionTable** hash_jit_artifact_array_remove(J9PortLibrary *portLibrary, J9JITExceptionTable** array, J9JITExceptionTable *dataToRemove);
UDATA hash_jit_artifact_remove_range(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToRemove, UDATA startPC, UDATA endPC);


#ifdef __cplusplus
}
#endif

#endif     /* jithash_h */
