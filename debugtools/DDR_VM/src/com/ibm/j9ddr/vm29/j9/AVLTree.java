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

import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreeNodePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreePointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * AVL tree wrapper
 * 
 * @author andhall
 *
 */
public abstract class AVLTree extends BaseAlgorithm implements IAlgorithm
{
	
	protected AVLTree(int vmMinorVersion, int algorithmVersion)
	{
		super(vmMinorVersion, algorithmVersion);
	}

	public static AVLTree fromJ9AVLTreePointer(final J9AVLTreePointer structure, final IAVLSearchComparator comparator) throws CorruptDataException
	{
		AlgorithmPicker<AVLTree> picker = new AlgorithmPicker<AVLTree>(AlgorithmVersion.AVL_TREE_VERSION){

			@Override
			protected Iterable<? extends AVLTree> allAlgorithms()
			{
				List<AVLTree> toReturn = new LinkedList<AVLTree>();
				
				toReturn.add(new AVLTree_29_V0(structure, comparator));
				
				return toReturn;
			}
		};

		return picker.pickAlgorithm();
	}
	
	public abstract J9AVLTreeNodePointer search(UDATA searchValue) throws CorruptDataException;
}
