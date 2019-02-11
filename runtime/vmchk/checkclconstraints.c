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

#include "j9.h"
#include "j9port.h"
#include "vmcheck.h"
#include "omrlinkedlist.h"
#include "j9protos.h"
#include "j9modron.h"
#include "omrlinkedlist.h"

/*
 *  for each J9ClassLoadingConstraint member of the vm->classLoadingConstraints hash table verify:
 *  	- the constraint's classloader is alive
 *  	- the constraint's class object is alive
 *		- all the elements in the constraint's linked list point to the same class
 *		- all doubly-linked lists are well-formed
 *		- all members of each linked list are in the hash table
 */


void
checkClassLoadingConstraints(J9JavaVM *vm)
{
	UDATA nodeCount = 0;

	vmchkPrintf(vm, "  %s Checking classloading constraints>\n", VMCHECK_PREFIX);
	if (vm->classLoadingConstraints != NULL) {
		J9HashTableState walkState;
		J9ClassLoadingConstraint* constraint = hashTableStartDo(vm->classLoadingConstraints, &walkState);
		
		while (NULL != constraint) {
			J9ClassLoadingConstraint *searchResult;
			J9Class* clazz = constraint->clazz;

			if (NULL == constraint->classLoader) {
				vmchkPrintf(vm, "%s - Error constraint=0x%p has no classloader>\n", VMCHECK_FAILED, constraint);
			}
			if (J9_GC_CLASS_LOADER_DEAD == (constraint->classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) ) {
				vmchkPrintf(vm, "%s - Error classLoader=0x%p is dead>\n", VMCHECK_FAILED, constraint->classLoader);
			}
			if ((NULL != clazz) && (J9AccClassDying == (J9CLASS_FLAGS(clazz) & J9AccClassDying))) {
				vmchkPrintf(vm, "%s - Error class=0x%p is dead>\n", VMCHECK_FAILED, clazz);
			}
			if ((NULL == constraint->linkNext) || (NULL == constraint->linkPrevious) ||
					(constraint->linkNext->linkPrevious  != constraint) || (constraint->linkPrevious->linkNext  != constraint)) {
				vmchkPrintf(vm, "%s - Error linked list at constraint=0x%p is corrupt>\n", VMCHECK_FAILED, constraint);
			}
			if (constraint->linkNext->clazz  != constraint->clazz) {
				vmchkPrintf(vm, "%s - Error constraint=0x%p has a different class than its neighbour>\n", VMCHECK_FAILED, constraint);
			}

			searchResult = hashTableFind(vm->classLoadingConstraints, constraint->linkNext);
			if (searchResult != constraint->linkNext) {
				vmchkPrintf(vm, "%s - Error constraint=0x%p not found in hash table>\n", VMCHECK_FAILED, constraint->linkNext);
			}

			++nodeCount;
			constraint = hashTableNextDo(&walkState);
		}
	}

	vmchkPrintf(vm, "  %s Checking classloading constraints, %d nodes done>\n", VMCHECK_PREFIX, nodeCount);
}
