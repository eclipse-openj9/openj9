/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.IDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreeNodePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreePointer;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * @author andhall
 *
 */
class AVLTree_29_V0 extends AVLTree
{
	private static final long AVL_BALANCEMASK = 0x3;
	
	private final J9AVLTreePointer _tree;	
	private final IAVLSearchComparator _searchComparator;	
	
	AVLTree_29_V0(J9AVLTreePointer tree, IAVLSearchComparator searchComparator)
	{
		super(90,0);
		_tree = tree;
		_searchComparator = searchComparator;
	}
	
	private J9AVLTreeNodePointer findNode(UDATA search) throws CorruptDataException
	{
		J9AVLTreeNodePointer walk = _tree.rootNode();
		
		while (walk.notNull()) {
			int dir = _searchComparator.searchComparator(_tree, search, walk);
			
			if (0 == dir) {
				break;
			}
			
			if (dir < 0) {
				walk = AVL_SRP_GETNODE(walk.leftChild(), walk.leftChildEA());
			} else {
				walk = AVL_SRP_GETNODE(walk.rightChild(), walk.rightChildEA());
			}
		}
		
		return walk;
	}

	private J9AVLTreeNodePointer AVL_SRP_GETNODE(IDATA srp, IDATAPointer srpEA)
	{
		return AVL_GETNODE(srp).notNull() ? J9AVLTreeNodePointer.cast(srpEA.addOffset(IDATA.cast(AVL_GETNODE(srp)))) : J9AVLTreeNodePointer.NULL;
	}

	private static J9AVLTreeNodePointer AVL_GETNODE(IDATA ptr)
	{
		return J9AVLTreeNodePointer.cast(new UDATA(ptr).bitAnd(~AVL_BALANCEMASK));
	}

	@Override
	public J9AVLTreeNodePointer search(UDATA searchValue) throws CorruptDataException
	{
		return findNode(searchValue);
	}
}
