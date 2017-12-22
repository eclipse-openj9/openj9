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

#ifndef bcutil_internal_h
#define bcutil_internal_h

/**
* @file bcutil_internal.h
* @brief Internal prototypes used within the BCUTIL module.
*
* This file contains implementation-private function prototypes and
* type definitions for the BCUTIL module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "bcutil_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- bcutil.c ---------------- */

#if defined(J9VM_OPT_INVARIANT_INTERNING)

#define BCU_TREE_VERIFY_ASSERT(tree, condition) \
do { \
	if (!condition) { \
		if (tree != NULL) { \
			tree->flags = (tree->flags & (~J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS)); \
		} \
		Trc_BCU_Assert_TrueTreeVerify(condition); \
	} \
} while(0)

#define BCU_TREE_VERIFY_ASSERT_AND_RETURN_ONFAIL_VOID(tree, condition) \
do { \
	if (!condition) { \
		if (tree != NULL) { \
			tree->flags = (tree->flags & (~J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS)); \
		} \
		Trc_BCU_Assert_TrueTreeVerify(condition); \
		return; \
	} \
} while(0)

#define BCU_TREE_VERIFY_ASSERT_AND_RETURN_ONFAIL_RC(tree, condition, rc) \
do { \
	if (!condition) { \
		if (tree != NULL) { \
			tree->flags = (tree->flags & (~J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS)); \
		} \
		Trc_BCU_Assert_TrueTreeVerify(condition); \
		return rc; \
	} \
} while(0)

#else
#define BCU_TREE_VERIFY_ASSERT(tree, condition)
#define BCU_TREE_VERIFY_ASSERT_AND_RETURN_ONFAIL_VOID(tree, condition)
#define BCU_TREE_VERIFY_ASSERT_AND_RETURN_ONFAIL_RC(tree, condition, rc)
#endif

J9HashTable*
romClassHashTableNew(J9JavaVM *vm, U_32 initialSize);

void
romClassHashTableFree(J9HashTable *hashTable);

UDATA
romClassHashTableAdd(J9HashTable *hashTable, J9ROMClass *value);

J9ROMClass*
romClassHashTableFind(J9HashTable *hashTable, U_8 *className, UDATA classNameLength);

void
romClassHashTableReplace(J9HashTable *hashTable, J9ROMClass *originalClass, J9ROMClass *replacementClass);

UDATA
romClassHashTableDelete(J9HashTable *hashTable, J9ROMClass *romClass);

void
romVerboseRecordPhaseStart(void *verboseContext, UDATA phase);

void
romVerboseRecordPhaseEnd(void *verboseContext, UDATA phase);

#ifdef __cplusplus
}
#endif

#endif /* bcutil_internal_h */

