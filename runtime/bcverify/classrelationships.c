/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of thse Eclipse Public License 2.0 which accompanies this
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

#include "j9protos.h"
#include "j9consts.h"
#include "ut_j9bcverify.h"
#include "omrlinkedlist.h"

static VMINLINE J9ClassRelationshipNode *allocateParentNode(J9VMThread *vmThread, U_8 *className, UDATA classNameLength);
static VMINLINE J9ClassRelationship *findClassRelationship(J9VMThread *vmThread, J9ClassLoader *classLoader, U_8 *className, UDATA classNameLength);
static void freeClassRelationshipParentNodes(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ClassRelationship *relationship);
static UDATA relationshipHashFn(void *key, void *userData);
static UDATA relationshipHashEqualFn(void *leftKey, void *rightKey, void *userData);

/**
 * Record a class relationship in the class relationships table.
 *
 * Returns TRUE if successful and FALSE if an out of memory error occurs.
 */
IDATA
j9bcv_recordClassRelationship(J9VMThread *vmThread, J9ClassLoader *classLoader, U_8 *childName, UDATA childNameLength, U_8 *parentName, UDATA parentNameLength, IDATA *reasonCode)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9JavaVM *vm = vmThread->javaVM;
	J9ClassRelationship *childEntry = NULL;
	J9ClassRelationshipNode *parentNode = NULL;
	J9ClassRelationship child = {0};
	IDATA recordResult = FALSE;
	*reasonCode = BCV_ERR_INSUFFICIENT_MEMORY;

	Trc_RTV_recordClassRelationship_Entry(vmThread, childNameLength, childName, parentNameLength, parentName);

	Assert_RTV_true((NULL != childName) && (NULL != parentName));

	/* Locate existing childEntry or add new entry to the hashtable */
	childEntry = findClassRelationship(vmThread, classLoader, childName, childNameLength);

	if (NULL == childEntry) {
		child.className = (U_8 *) j9mem_allocate_memory(childNameLength + 1, J9MEM_CATEGORY_CLASSES);

		/* className for child successfully allocated, continue initialization of child entry */
		if (NULL != child.className) {
			memcpy(child.className, childName, childNameLength);
			child.className[childNameLength] = '\0';
			child.classNameLength = childNameLength;
			child.flags = 0;

			childEntry = hashTableAdd(classLoader->classRelationshipsHashTable, &child);

			if (NULL == childEntry) {
				Trc_RTV_recordClassRelationship_EntryAllocationFailedChild(vmThread);
				j9mem_free_memory(child.className);
				goto recordDone;
			}
		} else {
			Trc_RTV_recordClassRelationship_EntryAllocationFailedChild(vmThread);
			goto recordDone;
		}
	}

	/* If the parent is java/lang/Throwable, set a flag instead of allocating a node */
	if (J9UTF8_DATA_EQUALS(J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING, J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING_LENGTH, parentName, parentNameLength)) {
		if (!J9_ARE_ANY_BITS_SET(childEntry->flags, J9RELATIONSHIP_PARENT_IS_THROWABLE)) {
			childEntry->flags |= J9RELATIONSHIP_PARENT_IS_THROWABLE;
		}
	} else {
		/* Add a parentNode to the child's linked list of parents */
		if (J9_LINKED_LIST_IS_EMPTY(childEntry->root)) {
			parentNode = allocateParentNode(vmThread, parentName, parentNameLength);
			if (parentNode == NULL) {
				/* Allocation failure */
				Trc_RTV_classRelationships_AllocationFailedParent(vmThread);
				goto recordDone;
			}
			Trc_RTV_recordClassRelationship_AllocatedEntry(vmThread, childEntry->classNameLength, childEntry->className, childEntry, parentNode->classNameLength, parentNode->className, parentNode); 
			J9_LINKED_LIST_ADD_LAST(childEntry->root, parentNode);
		} else {
			BOOLEAN alreadyPresent = FALSE;
			BOOLEAN addBefore = FALSE;
			J9ClassRelationshipNode *walk = J9_LINKED_LIST_START_DO(childEntry->root);
			/**
			 * Keep the list of parent nodes ordered by class name length so it's a faster traversal
			 * and duplicates can be avoided
			 */
			while (NULL != walk) {
				if (walk->classNameLength > parentNameLength) {
					addBefore = TRUE;
					break;
				} else if (J9UTF8_DATA_EQUALS(walk->className, walk->classNameLength, parentName, parentNameLength)) {
					/* Already present, skip */
					alreadyPresent = TRUE;
					break;
				} else {
					/* walk->className is shorter or equal length but different data; keep looking */
				}
				walk = J9_LINKED_LIST_NEXT_DO(childEntry->root, walk);
			}
			if (!alreadyPresent) {
				parentNode = allocateParentNode(vmThread, parentName, parentNameLength);
				if (parentNode == NULL) {
					/* Allocation failure */
					Trc_RTV_classRelationships_AllocationFailedParent(vmThread);
					goto recordDone;
				}
				Trc_RTV_recordClassRelationship_AllocatedEntry(vmThread, childEntry->classNameLength, childEntry->className, childEntry, parentNode->classNameLength, parentNode->className, parentNode); 
				if (addBefore) {
					J9_LINKED_LIST_ADD_BEFORE(childEntry->root, walk, parentNode);
				} else {
					/* If got through the whole list of shorter or equal length names, add it here */
					J9_LINKED_LIST_ADD_LAST(childEntry->root, parentNode);
				}
			}
		}
	}

	recordResult = TRUE;
	*reasonCode = 0;

recordDone:
	Trc_RTV_recordClassRelationship_Exit(vmThread, recordResult);
	return recordResult;
}

/**
 * Validate each recorded relationship for a class.
 *
 * Returns failedClass, which is NULL if successful, or the class that fails validation if unsuccessful.
 */
J9Class *
j9bcv_validateClassRelationships(J9VMThread *vmThread, J9ClassLoader *classLoader, U_8 *childName, UDATA childNameLength, J9Class *childClass)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9Class *parentClass = NULL;
	J9Class *failedClass = NULL;
	J9ClassRelationship *childEntry = NULL;
	J9ClassRelationshipNode *parentNode = NULL;

	Trc_RTV_validateClassRelationships_Entry(vmThread, childNameLength, childName);
	Assert_RTV_true(NULL != childName);
	childEntry = findClassRelationship(vmThread, classLoader, childName, childNameLength);

	/* No relationships were recorded for the class (in this class loader), or its relationships have already been verified */
	if (NULL == childEntry) {
		goto validateDone;
	}

	/* The class is invalid if it has been marked as an interface, but it actually isn't */
	if (J9_ARE_ANY_BITS_SET(childEntry->flags, J9RELATIONSHIP_MUST_BE_INTERFACE)) {
		Trc_RTV_validateClassRelationships_FlaggedAsInterface(vmThread, childNameLength, childName);
		if (!J9ROMCLASS_IS_INTERFACE(childClass->romClass)) {
			Trc_RTV_validateClassRelationships_ShouldBeInterface(vmThread, childNameLength, childName);
			failedClass = childClass;
			goto validateDone;
		}
	}

	/* If J9RELATIONSHIP_PARENT_IS_THROWABLE is set, check that the relationship holds */
	if (J9_ARE_ANY_BITS_SET(childEntry->flags, J9RELATIONSHIP_PARENT_IS_THROWABLE)) {
		/* Throwable will already be loaded since it is a required class J9VMCONSTANTPOOL_JAVALANGTHROWABLE */
		parentClass = J9VMJAVALANGTHROWABLE_OR_NULL(vmThread->javaVM);
		Assert_RTV_true(NULL != parentClass);
		if (isSameOrSuperClassOf(parentClass, childClass)) {
			Trc_RTV_validateClassRelationships_ParentIsSuperClass(vmThread, J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING_LENGTH, J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING, NULL);
		} else {
			/* The class is invalid since it doesn't hold the expected relationship with java/lang/Throwable */
			Trc_RTV_validateClassRelationships_InvalidRelationship(vmThread, J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING_LENGTH, J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING);
			failedClass = parentClass;
			goto validateDone;
		}
	}

	parentNode = J9_LINKED_LIST_START_DO(childEntry->root);

	while (NULL != parentNode) {
		/* Find the parent class in the loaded classes table */
		parentClass = J9_VM_FUNCTION(vmThread, hashClassTableAt)(classLoader, parentNode->className, parentNode->classNameLength);

		/* If the parent class has not been loaded, then it has to be an interface since the child is already loaded */
		if (NULL == parentClass) {
			/* Add a new relationship to the table if one doesn't already exist and flag the parentClass as J9RELATIONSHIP_MUST_BE_INTERFACE */
			J9ClassRelationship *parentEntry = findClassRelationship(vmThread, classLoader, parentNode->className, parentNode->classNameLength);

			Trc_RTV_validateClassRelationships_ParentNotLoaded(vmThread, parentNode->classNameLength, parentNode->className, parentNode);

			if (NULL == parentEntry) {
				J9ClassRelationship parent = {0};
				PORT_ACCESS_FROM_VMC(vmThread);
				parent.className = (U_8 *) j9mem_allocate_memory(parentNode->classNameLength + 1, J9MEM_CATEGORY_CLASSES);

				/* className for parent successfully allocated, continue initialization of parent entry */
				if (NULL != parent.className) {
					Trc_RTV_validateClassRelationships_AllocatingParent(vmThread);
					memcpy(parent.className, parentNode->className, parentNode->classNameLength);
					parent.className[parentNode->classNameLength] = '\0';
					parent.classNameLength = parentNode->classNameLength;
					parent.flags = J9RELATIONSHIP_MUST_BE_INTERFACE;

					parentEntry = hashTableAdd(classLoader->classRelationshipsHashTable, &parent);

					if (NULL == parentEntry) {
						Trc_RTV_classRelationships_AllocationFailedParent(vmThread);
						j9mem_free_memory(parent.className);
						failedClass = childClass;
						goto validateDone;
					}
					Trc_RTV_validateClassRelationships_AllocatedParentEntry(vmThread);
				} else {
					Trc_RTV_classRelationships_AllocationFailedParent(vmThread);
					failedClass = childClass;
					goto validateDone;
				}
			} else {
				parentEntry->flags |= J9RELATIONSHIP_MUST_BE_INTERFACE;
			}
		} else {
			/* The already loaded parentClass should either be an interface, or is the same or superclass of the childClass */
			if (J9ROMCLASS_IS_INTERFACE(parentClass->romClass)) {
				/* If the target is an interface, be permissive as per the verifier type checking rules */
				Trc_RTV_validateClassRelationships_ParentIsInterface(vmThread, parentNode->classNameLength, parentNode->className, parentNode);
			} else if (isSameOrSuperClassOf(parentClass, childClass)) {
				Trc_RTV_validateClassRelationships_ParentIsSuperClass(vmThread, parentNode->classNameLength, parentNode->className, parentNode);
			} else {
				/* The child and parent have an invalid relationship */
				Trc_RTV_validateClassRelationships_InvalidRelationship(vmThread, parentNode->classNameLength, parentNode->className);
				failedClass = parentClass;
				goto validateDone;
			}
		}
		parentNode = J9_LINKED_LIST_NEXT_DO(childEntry->root, parentNode);
	}

	/* Successful validation; free memory for childEntry */
	freeClassRelationshipParentNodes(vmThread, classLoader, childEntry);
	j9mem_free_memory(childEntry->className);
	hashTableRemove(classLoader->classRelationshipsHashTable, childEntry);

validateDone:
	Trc_RTV_validateClassRelationships_Exit(vmThread, failedClass);
	return failedClass;
}

/**
 * Add a parentNode to a child entry's linked list of parents.
 *
 * Return the allocated J9ClassRelationshipNode.
 */
static VMINLINE J9ClassRelationshipNode *
allocateParentNode(J9VMThread *vmThread, U_8 *className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9ClassRelationshipNode *parentNode = (J9ClassRelationshipNode *) j9mem_allocate_memory(sizeof(J9ClassRelationshipNode), J9MEM_CATEGORY_CLASSES);

	if (NULL != parentNode) {
		parentNode->className = (U_8 *) j9mem_allocate_memory(classNameLength + 1, J9MEM_CATEGORY_CLASSES);

		if (NULL != parentNode->className) {
			memcpy(parentNode->className, className, classNameLength);
			parentNode->className[classNameLength] = '\0';
			parentNode->classNameLength = classNameLength;
		} else {
			j9mem_free_memory(parentNode);
			parentNode = NULL;
		}
	}

	return parentNode;
}

/**
 * Find the class relationship table entry for a particular class.
 *
 * Returns the found J9ClassRelationship, or NULL if it is not found.
 */
static VMINLINE J9ClassRelationship *
findClassRelationship(J9VMThread *vmThread, J9ClassLoader *classLoader, U_8 *className, UDATA classNameLength)
{
	J9ClassRelationship *classEntry = NULL;
	J9JavaVM *vm = vmThread->javaVM;

	Trc_RTV_findClassRelationship_Entry(vmThread, classNameLength, className);

	if (NULL != classLoader->classRelationshipsHashTable) {
		J9ClassRelationship exemplar = {0};
		exemplar.className = className;
		exemplar.classNameLength = classNameLength;
		classEntry = hashTableFind(classLoader->classRelationshipsHashTable, &exemplar);
	}

	Trc_RTV_findClassRelationship_Exit(vmThread, classEntry);
	return classEntry;
}

/**
 * Free allocated memory for each parent class node of a class relationship table entry.
 */
static void
freeClassRelationshipParentNodes(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ClassRelationship *relationship)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9ClassRelationshipNode *parentNode = NULL;

	Trc_RTV_freeClassRelationshipParentNodes_Entry(vmThread, relationship->classNameLength, relationship->className);

	while (NULL != relationship->root) {
		parentNode = relationship->root;
		Trc_RTV_freeClassRelationshipParentNodes_Parent(vmThread, parentNode->classNameLength, parentNode->className);
		J9_LINKED_LIST_REMOVE(relationship->root, parentNode);
		j9mem_free_memory(parentNode->className);
		j9mem_free_memory(parentNode);
	}

	Trc_RTV_freeClassRelationshipParentNodes_Exit(vmThread);
	return;
}

/**
 * Allocates new hash table to store class relationship entries.
 *
 * Returns 0 if successful, and 1 otherwise.
 */
UDATA
j9bcv_hashClassRelationshipTableNew(J9ClassLoader *classLoader, J9JavaVM *vm)
{
	UDATA result = 0;

	/* Allocate classRelationshipsHashTable if -XX:+ClassRelationshipVerifier is used */
	if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER)) {
		classLoader->classRelationshipsHashTable = hashTableNew(OMRPORT_FROM_J9PORT(vm->portLibrary), J9_GET_CALLSITE(), 256, sizeof(J9ClassRelationship), sizeof(char *), 0, J9MEM_CATEGORY_CLASSES, relationshipHashFn, relationshipHashEqualFn, NULL, vm);

		if (NULL == classLoader->classRelationshipsHashTable) {
			result = 1;
		}
	}

	return result;
}

/**
 * Frees memory for each J9ClassRelationship table entry, J9ClassRelationshipNode, 
 * and the classRelationships hash table itself.
 */
void
j9bcv_hashClassRelationshipTableFree(J9VMThread *vmThread, J9ClassLoader *classLoader, J9JavaVM *vm)
{
	/* Free the class relationships hash table if -XX:+ClassRelationshipVerifier is used */
	if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER)) {
		PORT_ACCESS_FROM_VMC(vmThread);
		J9HashTableState hashTableState = {0};
		J9HashTable *classRelationshipsHashTable = classLoader->classRelationshipsHashTable;
		J9ClassRelationship *relationshipEntryStart = (J9ClassRelationship *) hashTableStartDo(classRelationshipsHashTable, &hashTableState);
		J9ClassRelationship *relationshipEntry = relationshipEntryStart;

		/* Free all parent nodes of a relationship entry and then the entry itself */
		while (NULL != relationshipEntry) {
			UDATA result = 0;
			freeClassRelationshipParentNodes(vmThread, classLoader, relationshipEntry);
			j9mem_free_memory(relationshipEntry->className);
			result = hashTableDoRemove(&hashTableState);
			Assert_RTV_true(0 == result);
			relationshipEntry = (J9ClassRelationship *) hashTableNextDo(&hashTableState);
		}
	}

	return;
}

static UDATA
relationshipHashFn(void *key, void *userData)
{
	J9ClassRelationship *relationshipKey = key;
	J9JavaVM *vm = userData;

	UDATA utf8HashClass = J9_VM_FUNCTION_VIA_JAVAVM(vm, computeHashForUTF8)(relationshipKey->className, relationshipKey->classNameLength);

	return utf8HashClass;
}

static UDATA
relationshipHashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	J9ClassRelationship *left_relationshipKey = leftKey;
	J9ClassRelationship *right_relationshipKey = rightKey;

	UDATA classNameEqual = J9UTF8_DATA_EQUALS(left_relationshipKey->className, left_relationshipKey->classNameLength, right_relationshipKey->className, right_relationshipKey->classNameLength);

	return classNameEqual;
}
