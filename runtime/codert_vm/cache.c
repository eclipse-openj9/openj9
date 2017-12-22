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

#include "cache.h"
#include "jitavl.h"

J9_DECLARE_CONSTANT_UTF8(newInstanceImplName, "newInstanceImpl");
J9_DECLARE_CONSTANT_UTF8(newInstanceImplSig, "(Ljava/lang/Class;)Ljava/lang/Object;");
const J9NameAndSignature newInstanceImplNameAndSig = { (J9UTF8*)&newInstanceImplName, (J9UTF8*)&newInstanceImplSig };

J9AVLTree * jit_allocate_artifacts(J9PortLibrary * portLibrary)
{
	J9AVLTree *artifactAVLTree;
	PORT_ACCESS_FROM_PORT(portLibrary);

	artifactAVLTree = (J9AVLTree *) j9mem_allocate_memory(sizeof(J9AVLTree), OMRMEM_CATEGORY_JIT);
	if (!artifactAVLTree)
		return NULL;
	artifactAVLTree->insertionComparator = (IDATA (*)(J9AVLTree *, J9AVLTreeNode *, J9AVLTreeNode *))avl_jit_artifact_insertionCompare;
	artifactAVLTree->searchComparator = (IDATA (*)(J9AVLTree *, UDATA, J9AVLTreeNode *))avl_jit_artifact_searchCompare;
	artifactAVLTree->genericActionHook = NULL;
	artifactAVLTree->flags = 0;
	artifactAVLTree->rootNode = 0;
	artifactAVLTree->portLibrary = OMRPORT_FROM_J9PORT(PORTLIB);

	return artifactAVLTree;
}

J9JITHashTable *avl_jit_artifact_insert_existing_table(J9AVLTree * tree, J9JITHashTable * hashTable)
{
	avl_insert(tree, (J9AVLTreeNode *) hashTable);
	return hashTable;
}

