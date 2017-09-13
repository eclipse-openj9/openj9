/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.j9.uma.util;

import java.util.TreeSet;
import java.util.Vector;

public class Tree<T> {
	
	class TreeNode<E> {
		E e;
		TreeNode<E> left = null;
		TreeNode<E> right = null;
		public TreeNode(E e) {
			this.e = e;
		}
		E getElement() { return e; }
		void setLeft( TreeNode<E> left ) {
			this.left = left;
		}
		TreeNode<E> getLeft() {
			return left;
		}
		void setRight( TreeNode<E> right ) {
			this.right = right;
		}
		TreeNode<E> getRight() {
			return right;
		}
	}
	
	
	TreeNode<T> sortAsTree(Vector<T> sortedCollection) {
		TreeNode<T> root = null;
		// Split the collection into three parts.  Find the largest 
		// pair complete balanced subtrees, distribute the remaining 
		// elements starting with the left size. Return the pivot element 
		// and the two subtrees.

		// basic cases
		if ( sortedCollection.isEmpty() ) {
			// do nothing
		} else if ( sortedCollection.size() == 1 ) {
			root = new TreeNode<T>(sortedCollection.get(0));
		} else if ( sortedCollection.size() == 2 ) {
			root = new TreeNode<T>(sortedCollection.get(1));
			root.setLeft(new TreeNode<T>(sortedCollection.get(0)));
		} else if ( sortedCollection.size() == 3 ) {
			root = new TreeNode<T>(sortedCollection.get(1));
			root.setLeft(new TreeNode<T>(sortedCollection.get(0)));
			root.setRight(new TreeNode<T>(sortedCollection.get(2)));
		} else {

			// Depth of the complete tree we could build
			double depth = Math.floor((Math.log(sortedCollection.size()) / Math.log(2)));
			// What's left
			double remainder = sortedCollection.size() - ((Math.pow(2, depth))-1);
			double leftIndex = Math.min(Math.pow(2, depth-1), remainder);
			leftIndex += Math.pow(2, depth-1) - 1;
			int left = new Double(leftIndex).intValue();
			
			root = new TreeNode<T>(sortedCollection.get(left));
			root.setLeft(sortAsTree(new Vector<T>(sortedCollection.subList(0, left))));
			root.setRight(sortAsTree(new Vector<T>(sortedCollection.subList(left+1, sortedCollection.size()))));
		}
		return root;

	}
	
	void copyIntoFlatTree( TreeNode<T> node, Vector<T> array, int index ) {
		if ( node == null || node.getElement() == null ) return;
		array.set(index-1, node.getElement());
		copyIntoFlatTree(node.getLeft(), array, index*2);
		copyIntoFlatTree(node.getRight(), array, (index*2)+1);
	}
	
	public Vector<T> sortAsFlatTree( Vector<T> vector ) {
		TreeSet<T> treeSet = new TreeSet<T>();
		for ( T t : vector ) {
			treeSet.add(t);
		}
		Vector<T> sortedCollection = new Vector<T>(vector.size());
		for( T t : treeSet ) {
			sortedCollection.add(t);
		}
		TreeNode<T> root = sortAsTree(sortedCollection);
		copyIntoFlatTree(root, sortedCollection, 1);
		return sortedCollection;
	}
}
