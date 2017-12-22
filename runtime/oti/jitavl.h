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

#ifndef jitavl_h
#define jitavl_h

#ifdef __cplusplus
extern "C" {
#endif

#include "avl_api.h"

/* ---------------- jitavl.c ---------------- */

/**
* @brief
* @param *javaVM
* @param *tree
* @return void
*/
void
avl_jit_artifact_free_all(J9JavaVM *javaVM, J9AVLTree *tree);


/**
* @brief
* @param *portLib
* @param *nodeToDelete
* @return void
*/
void
avl_jit_artifact_free_node(J9PortLibrary *portLib, J9JITHashTable *nodeToDelete);


/**
* @brief
* @param *tree
* @param *insertNode
* @param *walkNode
* @return IDATA
*/
IDATA
avl_jit_artifact_insertionCompare(J9AVLTree *tree, J9JITHashTable *insertNode, J9JITHashTable *walkNode);


/**
* @brief
* @param *tree
* @param searchValue
* @param *walkNode
* @return IDATA
*/
IDATA
avl_jit_artifact_searchCompare(J9AVLTree *tree, UDATA searchValue, J9JITHashTable *walkNode);


#ifdef __cplusplus
}
#endif

#endif     /* jitavl_h */


