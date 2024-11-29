/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <string.h>

#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "rommeth.h"
#include "j9cp.h"
#include "j9modron.h"
#include "ut_j9bcverify.h"
#include "omrlinkedlist.h"

static J9ClassLoadingConstraint* findClassLoadingConstraint (J9VMThread* vmThread, J9ClassLoader* loader, U_8* name, UDATA length);
static J9ClassLoadingConstraint* registerClassLoadingConstraint (J9VMThread* vmThread, J9ClassLoader* loader, U_8* name, UDATA length, BOOLEAN copyName);
static void validateArgs (J9VMThread* vmThread, J9ClassLoader* loader1, J9ClassLoader* loader2, U_8* name1, U_8* name2, UDATA length);
static void constrainList (J9ClassLoadingConstraint* constraint, J9Class* clazz);
static UDATA constraintHashFn(void *key, void *userData);
static UDATA  constraintHashEqualFn(void *leftKey, void *rightKey, void *userData);


/* This is a helper function used by Assert_RTV_validateClassLoadingConstraints */
static void 
validateArgs (J9VMThread* vmThread, J9ClassLoader* loader1, J9ClassLoader* loader2, U_8* name1, U_8* name2, UDATA length) 
{
	J9MemorySegment *seg;

	Assert_RTV_notEqual(loader1, loader2);
	Assert_RTV_true(0 == memcmp(name1, name2, length));

	/* verify that the name is actually in the correct segment. If it's not, the name could get
	 * garbage collected while the constraint object is still alive
	 */
	seg = vmThread->javaVM->classMemorySegments->nextSegment;
	while (seg) {
		if (seg->heapBase <= name1 && seg->heapTop >= name1) {
			Assert_RTV_true( (seg->classLoader == loader1) || (seg->classLoader->flags & J9CLASSLOADER_INVARIANTS_SHARABLE) );
		}
		if (seg->heapBase <= name2 && seg->heapTop >= name2) {
			Assert_RTV_true( (seg->classLoader == loader2) || (seg->classLoader->flags & J9CLASSLOADER_INVARIANTS_SHARABLE) );
		}
		seg = seg->nextSegment;
	}
}


/*
 * sig1 must come from a ROMSegment in loader1.
 * sig2 must come from a ROMSegment in loader2.
 * sig1 and sig2 must contain identical bytes. The signatures may be method or field signatures.
 * return 0 if no class loading constraints have been violated, or non-zero if they have been.
 */
UDATA 
j9bcv_checkClassLoadingConstraintsForSignature (J9VMThread *vmThread, J9ClassLoader *loader1, J9ClassLoader *loader2, J9UTF8 *sig1, J9UTF8 *sig2, BOOLEAN copySig1)
{
	U_32 index = 0, endIndex;
	U_32 length = J9UTF8_LENGTH(sig1);
	UDATA rc = 0;
	J9JavaVM *javaVM = vmThread->javaVM;
	JavaVM* jniVM = (JavaVM*)javaVM;

	Trc_RTV_checkClassLoadingConstraintsForSignature_Entry(vmThread, loader1, loader2, sig1, sig2, J9UTF8_LENGTH(sig1), J9UTF8_DATA(sig1));
	Assert_RTV_true(J9UTF8_LENGTH(sig1) == J9UTF8_LENGTH(sig2));
	Assert_RTV_validateClassLoadingConstraints(vmThread, loader1, loader2, J9UTF8_DATA(sig1), J9UTF8_DATA(sig2), J9UTF8_LENGTH(sig1));

	omrthread_monitor_enter(javaVM->classTableMutex);
	for (;;) {
		/* find a 'L', indicating the beginning of a class name */
		while (index  < length && J9UTF8_DATA(sig1)[index] != 'L') {
			index++;
		}
		if (index >= length) {
			break;
		}

		/* skip the 'L'; */
		index++;

		/* find the ';' marking the end of the class name */
		endIndex = index;
		while (J9UTF8_DATA(sig1)[endIndex] != ';') {
			endIndex++;
		}
		rc = j9bcv_checkClassLoadingConstraintForName (vmThread, loader1, loader2, &J9UTF8_DATA(sig1)[index], &J9UTF8_DATA(sig2)[index], endIndex - index, copySig1, FALSE);
		if (rc) {
			break;
		}

		index = endIndex;
	}
	omrthread_monitor_exit(javaVM->classTableMutex);

	Trc_RTV_checkClassLoadingConstraintsForSignature_Exit(vmThread, rc);

	return rc;
}


/* NOTE: the current thread must own the class table mutex */

UDATA
j9bcv_checkClassLoadingConstraintForName (J9VMThread *vmThread, J9ClassLoader *loader1, J9ClassLoader *loader2, U_8 *name1, U_8 *name2, UDATA length, BOOLEAN copyName1, BOOLEAN copyName2)
{
	J9Class *class1;
	J9Class *class2;
	J9ClassLoadingConstraint *const1 = NULL;
	J9ClassLoadingConstraint *const2 = NULL;
	J9InternalVMFunctions const *vmFuncs = vmThread->javaVM->internalVMFunctions;

	Trc_RTV_checkClassLoadingConstraintForName(vmThread, loader1, loader2, length, name1);
	Assert_RTV_validateClassLoadingConstraints(vmThread, loader1, loader2, name1, name2, length);

	/* peek at the class tables to see if the class has been loaded yet */
	class1 = vmFuncs->hashClassTableAt (loader1, name1, length);
	class2 = vmFuncs->hashClassTableAt (loader2, name2, length);

	if (class1 && class2) {
		if (class1 != class2) {
			return 1;
		}
	} else if (class1 == NULL && class2 != NULL) {
		const1 = registerClassLoadingConstraint (vmThread, loader1, name1, length, copyName1);
		if (const1 == NULL) return 1;
		if (const1->clazz != NULL) {
			if (const1->clazz != class2) {
				return 1;
			}
		} else {
			Assert_RTV_true(J9_ARE_NO_BITS_SET(class2->classFlags, J9ClassIsAnonymous));
			const1->clazz = class2;
		}
	} else if (class2 == NULL && class1 != NULL) {
		const2 = registerClassLoadingConstraint (vmThread, loader2, name2, length, copyName2);
		if (const2->clazz != NULL) {
			if (const2->clazz != class1) {
				return 1;
			}
		} else {
			const2->clazz = class1;
			Assert_RTV_true(J9_ARE_NO_BITS_SET(class1->classFlags, J9ClassIsAnonymous));
		}
	} else { /* class1 == NULL && class2 == NULL */
		J9ClassLoadingConstraint *tempNext;
		J9ClassLoadingConstraint *tempPrevious;

		const1 = registerClassLoadingConstraint (vmThread, loader1, name1, length, copyName1);
		if (const1 == NULL) {
			return 1;
		}
		const2 = registerClassLoadingConstraint (vmThread, loader2, name2, length, copyName2);
		if (const2 == NULL) {
			return 1;
		}

		if (const1->clazz != const2->clazz) {
			/* need to merge two constraint chains with different constraints.
			 * This is only solvable if one of them is NULL
			 */
			if (const1->clazz == NULL) {
				constrainList(const1, const2->clazz);
			} else if (const2->clazz == NULL) {
				constrainList(const2, const1->clazz);
			} else {
				/* both constraints are satisfied, but in an incompatible manner */
				return 1;
			}
		}

		/* now link them up, keeping in mind that one or either of them might have already been in a list */
		tempNext = const1->linkNext;
		tempPrevious = const2->linkPrevious;
		const1->linkNext = const2;
		const2->linkPrevious = const1;
		tempNext->linkPrevious = tempPrevious;
		tempPrevious->linkNext = tempNext;
	}

	return 0;
}


/* NOTE: the current thread must own the class table mutex */

static J9ClassLoadingConstraint*
registerClassLoadingConstraint (J9VMThread* vmThread, J9ClassLoader* loader, U_8* name, UDATA length, BOOLEAN copyName)
{
	PORT_ACCESS_FROM_VMC (vmThread);
	J9JavaVM* vm = vmThread->javaVM;
	J9ClassLoadingConstraint* constraint;
	J9ClassLoadingConstraint exemplar;

	Trc_RTV_registerClassLoadingConstraint_Entry(vmThread, length, name, loader);

	if (vm->classLoadingConstraints == NULL) {
		Trc_RTV_registerClassLoadingConstraint_AllocatingTable(vmThread);
		vm->classLoadingConstraints  = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 256, sizeof(J9ClassLoadingConstraint), sizeof(char *), 0, J9MEM_CATEGORY_CLASSES, constraintHashFn, constraintHashEqualFn, NULL, vm);
		if (vm->classLoadingConstraints == NULL) {
			Trc_RTV_registerClassLoadingConstraint_TableAllocationFailed(vmThread);
			Trc_RTV_registerClassLoadingConstraint_Exit(vmThread, NULL);
			return NULL;
		}
	}

	exemplar.classLoader = loader;
	exemplar.name = name;
	exemplar.nameLength = length;
	exemplar.clazz = NULL;
	exemplar.linkNext = NULL;
	exemplar.linkPrevious = NULL;
	exemplar.freeName = FALSE;

	constraint = hashTableAdd(vm->classLoadingConstraints, &exemplar);
	if (constraint == NULL) {
oom:
		Trc_RTV_registerClassLoadingConstraint_EntryAllocationFailed(vmThread);
	} else if (constraint->linkNext == NULL) {
		/* this must be a newly added constraint. Link it up to itself */
		constraint->linkNext = constraint->linkPrevious = constraint;
		if (copyName) {
			U_8 *nameCopy = j9mem_allocate_memory(length, J9MEM_CATEGORY_CLASSES);
			if (NULL == nameCopy) {
				hashTableRemove(vm->classLoadingConstraints, constraint);
				constraint = NULL;
				goto oom;
			}
			memcpy(nameCopy, name, length);
			constraint->name = nameCopy;
			constraint->freeName = TRUE;
		}
		Trc_RTV_registerClassLoadingConstraint_AllocatedEntry(vmThread, constraint, length, name, loader);
	}

	Trc_RTV_registerClassLoadingConstraint_Exit(vmThread, constraint);
	return constraint;
}


/* NOTE: the current thread must own the class table mutex */

static J9ClassLoadingConstraint*
findClassLoadingConstraint (J9VMThread* vmThread, J9ClassLoader* loader, U_8* name, UDATA length) 
{
	J9ClassLoadingConstraint *constraint = NULL;
	J9JavaVM* vm = vmThread->javaVM;

	Trc_RTV_findClassLoadingConstraint_Entry(vmThread, length, name, loader);

	if (vm->classLoadingConstraints != NULL) {
		J9ClassLoadingConstraint exemplar;

		exemplar.classLoader = loader;
		exemplar.name = name;
		exemplar.nameLength = length;
		exemplar.clazz = NULL;
		exemplar.linkNext = NULL;
		exemplar.linkPrevious = NULL;

		constraint = hashTableFind(vm->classLoadingConstraints, &exemplar);
	}

	Trc_RTV_findClassLoadingConstraint_Exit(vmThread, constraint);

	return constraint;
}


/* NOTE: this function must only be called while the current thread owns the class table mutex */

J9Class * 
j9bcv_satisfyClassLoadingConstraint (J9VMThread* vmThread, J9ClassLoader* loader, J9Class* clazz)
{
	J9ROMClass* romClass = clazz->romClass;
	J9UTF8* name = J9ROMCLASS_CLASSNAME (romClass);
	J9ClassLoadingConstraint* constraint = findClassLoadingConstraint (vmThread, loader, J9UTF8_DATA(name), J9UTF8_LENGTH(name));

	if (constraint) {
		if ((NULL != constraint->clazz) && (constraint->clazz != clazz)) {
			return constraint->clazz;
		} else {
			J9ClassLoadingConstraint* root = constraint;
			U_8 *nameToFree = constraint->freeName ? constraint->name : NULL;
			constrainList (constraint, clazz);
			J9_LINKED_LIST_REMOVE(root, constraint);
			hashTableRemove (vmThread->javaVM->classLoadingConstraints, constraint);
			if (NULL != nameToFree) {
				PORT_ACCESS_FROM_VMC(vmThread);
				j9mem_free_memory(nameToFree);
			}
		}
	}
	return NULL;
}


/*
 * Called when class loaders are being unloaded. This function removes all of the dying loaders'
 * constraints from linked lists of constraints, and removes references to classes defined by
 * the dying loaders.
 * The current thread should have exclusive VM access
 */

void 
unlinkClassLoadingConstraints (J9JavaVM* jvm) 
{
	J9HashTableState walkState;
	J9ClassLoadingConstraint* constraint;

	Trc_RTV_unlinkClassLoadingConstraints_Entry();

	if (jvm->classLoadingConstraints != NULL) {
		PORT_ACCESS_FROM_JAVAVM(jvm);
		constraint = hashTableStartDo(jvm->classLoadingConstraints, &walkState);
		while (constraint != NULL) {
			U_8 *nameToFree = constraint->freeName ? constraint->name : NULL;
			if ((NULL == constraint->clazz) && (constraint->linkNext == constraint)) { /* no point in having a single empty element */
				hashTableDoRemove(&walkState);
				if (NULL != nameToFree) {
					j9mem_free_memory(nameToFree);
				}
			} else if (J9_GC_CLASS_LOADER_DEAD == (constraint->classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD) ) {
				/* this class loader is being unloaded. Remove the constraint from the linked list */
				J9ClassLoadingConstraint* root = constraint;
				J9_LINKED_LIST_REMOVE(root, constraint);
				hashTableDoRemove(&walkState);
				if (NULL != nameToFree) {
					j9mem_free_memory(nameToFree);
				}
			} else {
				/* mark the constraint as unsatisfied if it refers to a dying loader */
				J9Class* clazz = constraint->clazz;

				if ((NULL != clazz) && (J9AccClassDying == (J9CLASS_FLAGS(clazz) & J9AccClassDying))) {
					constraint->clazz = NULL;
				}
			}

			constraint = hashTableNextDo(&walkState);
		}
	}

	Trc_RTV_unlinkClassLoadingConstraints_Exit();
}


/*
 * Set the 'clazz' field of every J9ClassLoadingConstraint in the circular list to be clazz.
 */
static void
constrainList (J9ClassLoadingConstraint* constraint, J9Class* clazz)
{
	J9ClassLoadingConstraint* cursor = constraint;

   	Assert_RTV_true(J9_ARE_NO_BITS_SET(clazz->classFlags, J9ClassIsAnonymous));
	while (NULL != cursor) {
		cursor->clazz = clazz;
		cursor = J9_LINKED_LIST_NEXT_DO(constraint, cursor);
	}
}

static UDATA 
constraintHashFn(void *key, void *userData)
{
	J9ClassLoadingConstraint *k = key;
	J9JavaVM *vm = userData;
	UDATA utf8Hash = J9_VM_FUNCTION_VIA_JAVAVM(vm, computeHashForUTF8)(k->name, k->nameLength);

	return (UDATA)k->classLoader ^ utf8Hash;
}


static UDATA  
constraintHashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	J9ClassLoadingConstraint *left_k = leftKey;
	J9ClassLoadingConstraint *right_k = rightKey;
	J9JavaVM *vm = userData;

	return 
		(left_k->classLoader == right_k->classLoader) 
		&& (J9UTF8_DATA_EQUALS(left_k->name, left_k->nameLength, right_k->name, right_k->nameLength));
}

