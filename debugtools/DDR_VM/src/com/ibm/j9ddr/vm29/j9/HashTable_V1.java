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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreeNodePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.structure.J9HashtableConstants;
import com.ibm.j9ddr.vm29.types.UDATA;

public class HashTable_V1<StructType extends AbstractPointer> extends HashTable<StructType> 
{
	private static final long AVL_TREE_TAG_BIT = J9HashtableConstants.J9HASH_TABLE_AVL_TREE_TAG_BIT;
	private boolean _isInline = false;
	private AVLTreeComparatorFunction _avlTreeComparatorFunction;
	private EqualFunctionWrapper _equalFunctionWrapper;
	private boolean _isSpaceOpt;
	
	private final class HashTableSlotIterator implements SlotIterator<StructType>
	{
		private SlotIterator<PointerPointer> listPoolIterator = null;
		private SlotIterator<J9AVLTreeNodePointer> treePoolIterator = null;
		private PointerPointer _spaceOptNode = null;
		private PointerPointer _tableTop;
		
		public HashTableSlotIterator() {

			/* Initialize internal iteration state */
			if (!isSpaceOpt()) {

				/* Listpool backed hashtable */
				try {
					listPoolIterator = Pool.fromJ9Pool(_table.listNodePool(), PointerPointer.class).iterator();
				} catch (CorruptDataException cde) {
					raiseCorruptDataEvent("Error getting ListNodePool iterator", cde, false);
				}

				/* Tree backed hashtable */
				try {
					J9PoolPointer treeNodePool = _table.treeNodePool();
					if (treeNodePool.notNull()) {
						treePoolIterator = Pool.fromJ9Pool(treeNodePool, J9AVLTreeNodePointer.class).iterator();
					} else {
						treePoolIterator = null;
					}
				} catch (CorruptDataException cde) {
					raiseCorruptDataEvent("Error getting TreeNodePool iterator", cde, false);
				}

			} else {
				/* Space Optimized HashTable, find the first element */
				try {
					_spaceOptNode = _table.nodes();
					_tableTop = _table.nodes().add(_table.tableSize());
					if (_spaceOptNode.isNull()) {
						throw new CorruptDataException("Hashtable nodes pointer is null");
					}
					nextSpaceOpt();
				} catch (CorruptDataException cde) {
					raiseCorruptDataEvent("Error initializing space optimized hashtable", cde, true);
				}
			}
		}

		public boolean hasNext()
		{
			boolean hasNext = false;
			if ((null != listPoolIterator) && listPoolIterator.hasNext()) {
				hasNext = true;
			} else if ((null != treePoolIterator) && treePoolIterator.hasNext()) {
				hasNext = true;
			} else if (isSpaceOpt() && (_spaceOptNode.lt(_tableTop))) {
				hasNext = true;
			}
			return hasNext;
		}

		/**
		 * We always return pointer to a node
		 */
		@SuppressWarnings("unchecked")
		public StructType next()
		{
			PointerPointer data = null;
			StructType result = null;
			
			if (!hasNext()) {
				throw new NoSuchElementException("There are no more available elements");
			}

			if (isSpaceOpt()) {
				data = _spaceOptNode;
				/* Add 1 so that it does not pick up the current item */
				_spaceOptNode = _spaceOptNode.add(1);
				try {
					nextSpaceOpt();
				} catch (CorruptDataException cde) {
					raiseCorruptDataEvent("Hashtable nodes corrupted", cde, true);
				}
			} else if (listPoolIterator.hasNext()) {
				data = listPoolIterator.next();
			} else if (treePoolIterator.hasNext()) {
				J9AVLTreeNodePointer node = treePoolIterator.next();
				data = PointerPointer.cast(avlNodeToData(node));
			}

			try {
				if (_isInline) {
					result = (StructType)DataType.getStructure(_structType.getSimpleName(), data.getAddress());
				} else {
					result = (StructType)DataType.getStructure(_structType.getSimpleName(), PointerPointer.cast(data).at(0).getAddress());
				}
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Pool element corrupted", cde, true);
			}

			return result;
		}

		public VoidPointer nextAddress()
		{
			if(_isInline) {
				// Can't create PointerPointer for inlined hashtables
				throw new UnsupportedOperationException("Not supported."); 
			}

			if (!hasNext()) {
				throw new NoSuchElementException("There are no more available elements");
			}

			VoidPointer next = null;
			if (isSpaceOpt()){ 
				next = VoidPointer.cast(_spaceOptNode);
				/* Add 1 so that it does not pick up the current item */
				_spaceOptNode = _spaceOptNode.add(1);
				try {
					nextSpaceOpt();
				} catch (CorruptDataException cde) {
					raiseCorruptDataEvent("Hashtable nodes corrupted", cde, true);
				}
			}else if (listPoolIterator.hasNext()) {
				// TODO: this might be bogus, fix pool iterators
				next = listPoolIterator.nextAddress();
			} else if (treePoolIterator.hasNext()) {
				next = avlNodeToData(treePoolIterator.next());
			} else {
				throw new NoSuchElementException("There are no more available elements");
			}

			return next;
		}

		public void remove()
		{
			throw new UnsupportedOperationException("The J9HashTable is read only and cannot be modified.");
		}

		/**
		 * Get the next element for a space optimized hashtable.
		 * @throws CorruptDataException When it fails to read from the core image
		 */
		private void nextSpaceOpt() throws CorruptDataException
		{
			while (_spaceOptNode.lt(_tableTop) && _spaceOptNode.at(0).isNull()) {
				_spaceOptNode = _spaceOptNode.add(1);
			}
		}
	}

	/**
	 * Wrapper to make inline vs non-inline transparent for user provided EqualFunction
	 */
	private class EqualFunctionWrapper
	{
		public boolean equal(StructType entry1, StructType entry2) throws CorruptDataException 
		{	
			if (!_isInline) {
				entry1 = (StructType) DataType.getStructure(_structType, PointerPointer.cast(entry1).at(0).longValue());
			}
			return _equalFn.equal(entry1, entry2);
		}		
	}
	
	/**
	 * Wrapper to make inline vs non-inline transparent for user provided EqualFunction
	 */
	private class EqualFunctionFromComparator extends EqualFunctionWrapper
	{
		@Override
		public boolean equal(StructType entry1, StructType entry2) throws CorruptDataException 
		{
			if (!_isInline) {
				entry1 = (StructType) DataType.getStructure(_structType, PointerPointer.cast(entry1).at(0).longValue());				
			}
			return (0 == _comparatorFn.compare(entry1, entry2));
		}
	}
	
	/**
	 * Wrapper to make inline vs non-inline transparent for user provided ComparatorFunction
	 */
	private class AVLTreeComparatorFunction implements IAVLSearchComparator
	{
		public int searchComparator(J9AVLTreePointer tree, UDATA searchValue, J9AVLTreeNodePointer node) throws CorruptDataException 
		{
			VoidPointer data = avlNodeToData(node);
			StructType node1 = (StructType) DataType.getStructure(_structType, searchValue.longValue());
			StructType node2 = null;
			
			if (_isInline) {				
				node2 = (StructType) DataType.getStructure(_structType, data.longValue());				
			} else {				
				node2 = (StructType) DataType.getStructure(_structType, PointerPointer.cast(data).at(0).longValue());			
			}
			
			return _comparatorFn.compare(node1, node2);
		}		
	}
	
	protected HashTable_V1(J9HashTablePointer hashTablePointer, boolean isInline, Class<StructType> structType, HashEqualFunction<StructType> equalFn, HashFunction<StructType> hashFn) throws CorruptDataException 
	{
		super(hashTablePointer, structType, equalFn, hashFn, null);
		_equalFunctionWrapper = new EqualFunctionWrapper();
		_isInline = isInline;
		_isSpaceOpt = _table.listNodePool().isNull();
	}
	
	protected HashTable_V1(J9HashTablePointer hashTablePointer, boolean isInline, Class<StructType> structType, HashFunction<StructType> hashFn, HashComparatorFunction<StructType> comparatorFn) throws CorruptDataException 
	{
		super(hashTablePointer, structType, null, hashFn, comparatorFn);
		_avlTreeComparatorFunction = new AVLTreeComparatorFunction();
		_equalFunctionWrapper = new EqualFunctionFromComparator();		
		_isInline = isInline;
		_isSpaceOpt = _table.listNodePool().isNull();
	}
	
	private VoidPointer avlNodeToData(J9AVLTreeNodePointer p) 
	{
		return VoidPointer.cast(p.add(1));
	}

	private J9AVLTreePointer avlTreeUntag(VoidPointer p) 
	{
		return J9AVLTreePointer.cast(p.untag(AVL_TREE_TAG_BIT));
	}

	private PointerPointer nextEA(VoidPointer p) throws CorruptDataException 
	{
		return PointerPointer.cast(p.addOffset(_table.listNodeSize()).subOffset(UDATA.SIZEOF));
	}

	private boolean isAVLTreeTagged(VoidPointer p) 
	{
		return p.allBitsIn(AVL_TREE_TAG_BIT);
	}

	/* Whether or not the HashTable is space optimized.
	 * Space optimized hash tables do not support address iteration.
	 * */
	public  boolean isSpaceOpt() {
		return _isSpaceOpt;
	}

	@Override
	public String getTableName() 
	{
		try {
			return _table.tableName().getCStringAtOffset(0);
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting name", e, true);
			return null;
		}
	}

	@Override
	public long getCount() 
	{
		try {
			return _table.numberOfNodes().longValue();
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting table count", e, true);
			return 0;
		}
	}	
	
	// find by pointer value
	@Override
	public StructType find(StructType entry)  throws CorruptDataException
	{
		UDATA hash = _hashFn.hash(entry).mod(_table.tableSize());
		PointerPointer head = _table.nodes().add(hash);
		VoidPointer findNode = VoidPointer.NULL;

		if (_table.listNodePool().isNull()) {
			PointerPointer node = hashTableFindNodeSpaceOpt(_table, entry, head);
			if (node.at(0).notNull()) {
				findNode = VoidPointer.cast(node);
			} else {
				findNode = VoidPointer.NULL;
			}
		} else if (head.at(0).isNull()) {
			findNode = VoidPointer.NULL;
		} else if (isAVLTreeTagged(head.at(0))) {
			findNode = hashTableFindNodeInTree(_table, entry, head);
		} else {
			findNode = hashTableFindNodeInList(_table, entry, head);
		}
		if (!_isInline && findNode.notNull()) {
			findNode = PointerPointer.cast(findNode).at(0);
		}
		StructType node = (StructType) DataType.getStructure(_structType, findNode.getAddress());
		return node;
	}

	private PointerPointer hashTableFindNodeSpaceOpt(J9HashTablePointer table, StructType entry, PointerPointer head) throws CorruptDataException 
	{
		PointerPointer node = head;
		StructType nodeStruct = (StructType) DataType.getStructure(_structType, node.getAddress());
		while ((node.at(0).notNull()) && (!_equalFunctionWrapper.equal(nodeStruct, entry))) {
			node = node.add(1);				
			if (node == table.nodes().add(table.tableSize())) {
				node = table.nodes();
			}
			nodeStruct = (StructType) DataType.getStructure(_structType, node.getAddress());
		}
		return node;
	}

	private VoidPointer hashTableFindNodeInTree(J9HashTablePointer table, StructType entry, PointerPointer head) throws CorruptDataException 
	{
		J9AVLTreePointer tree = avlTreeUntag(head.at(0));

		AVLTree avlTree = AVLTree.fromJ9AVLTreePointer(tree, _avlTreeComparatorFunction);
		J9AVLTreeNodePointer searchResult = avlTree.search(UDATA.cast(entry));

		if (searchResult.notNull()) {
			return avlNodeToData(searchResult);
		} else {
			return VoidPointer.NULL;
		}		
	}

	private VoidPointer hashTableFindNodeInList(J9HashTablePointer table, StructType entry, PointerPointer head) throws CorruptDataException 
	{
		/* Look through the list looking for the correct key */
		VoidPointer node = head.at(0);
		StructType currentStruct = (StructType) DataType.getStructure(_structType, node.getAddress());
		
		while ((!node.isNull()) && (!_equalFunctionWrapper.equal(currentStruct, entry))) {
			node = nextEA(node).at(0);
			currentStruct = (StructType) DataType.getStructure(_structType, node.getAddress());
		}
		return VoidPointer.cast(currentStruct);
	}

	@Override
	public SlotIterator<StructType> iterator() 
	{
		return new HashTableSlotIterator();
	}
}
