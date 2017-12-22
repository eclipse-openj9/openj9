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
#include "j9protos.h"
#include "jithash.h"





UDATA jit_artifact_insert(J9PortLibrary * portLibrary, J9AVLTree * tree, J9JITExceptionTable * dataToInsert)
{
	J9JITHashTable *foundTable = NULL;

	/* find the correct hash table */
	foundTable = (J9JITHashTable *) avl_search(tree, (UDATA) dataToInsert->startPC);

	if (!foundTable)
		return (UDATA) 2;

	/* insert the element */
	return (UDATA) hash_jit_artifact_insert(portLibrary, foundTable, dataToInsert);
}

UDATA jit_artifact_remove(J9PortLibrary * portLibrary, J9AVLTree * tree, J9JITExceptionTable * dataToDelete)
{
	J9JITHashTable *foundTable = NULL;

	/* find the correct hash table */
	foundTable = (J9JITHashTable *) avl_search(tree, (UDATA) dataToDelete);

	if (!foundTable)
		return (UDATA) 1;

	/* delete the element */
	return (UDATA) hash_jit_artifact_remove(portLibrary, foundTable, dataToDelete);
}

J9JITHashTable *jit_artifact_add_code_cache(J9PortLibrary * portLibrary, J9AVLTree * tree, J9MemorySegment * cacheToInsert, J9JITHashTable *optionalHashTable)
{
	if(optionalHashTable) {
		avl_insert(tree, (J9AVLTreeNode *) optionalHashTable);
		return optionalHashTable;
	} else {
		J9JITHashTable *newTable;

		/* register the code cache we've created */
		newTable = hash_jit_allocate(portLibrary, (UDATA) cacheToInsert->heapBase, (UDATA) cacheToInsert->heapTop);

		if (!newTable)
			return NULL;

		avl_insert(tree, (J9AVLTreeNode *) newTable);

		return newTable;
	}
}

J9JITHashTable *
jit_artifact_protected_add_code_cache(J9JavaVM * vm, J9AVLTree * tree, J9MemorySegment * cacheToInsert, J9JITHashTable *optionalHashTable)
{
	J9JITHashTable * table;
	J9VMThread * currentThread = vm->internalVMFunctions->currentVMThread(vm);

	if (currentThread != NULL) {
		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
	}
	table = jit_artifact_add_code_cache(vm->portLibrary, tree, cacheToInsert, optionalHashTable);
	if (currentThread != NULL) {
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
	}
	return table;
}

