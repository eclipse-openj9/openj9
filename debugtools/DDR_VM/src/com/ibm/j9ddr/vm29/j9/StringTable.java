/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.HashTable.HashEqualFunction;
import com.ibm.j9ddr.vm29.j9.HashTable.HashComparatorFunction;
import com.ibm.j9ddr.vm29.j9.HashTable.HashFunction;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_StringTablePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.structure.MM_StringTable;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class StringTable
{
	protected MM_StringTablePointer _stringTable;
	protected long _tableCount;
	StringHashFunction<PointerPointer> _hashFn;
	
	// Not intended for construction, use the factory
	protected StringTable(MM_StringTablePointer stringTable) throws CorruptDataException
	{
		_stringTable = stringTable;
		_tableCount = stringTable._tableCount().longValue();
		_hashFn = new StringHashFunction<PointerPointer>();
	}
	
	public static class StringEqualFunction<StructType extends AbstractPointer> implements HashEqualFunction<StructType>
	{
		public boolean equal(StructType left, StructType right)
		{
			try {
				J9ObjectPointer leftObject = J9ObjectPointer.cast(left);
				J9ObjectPointer rightObject = J9ObjectPointer.cast(right);
				String leftString = J9ObjectHelper.stringValue(leftObject);
				String rightString = J9ObjectHelper.stringValue(rightObject);
				return leftString.equals(rightString);
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error checking equality", e, true);
				return false;
			}
		}
	}

	public static class StringComparatorFunction<StructType extends AbstractPointer> implements HashComparatorFunction<StructType>
	{
		public int compare(StructType o1, StructType o2) throws CorruptDataException {
			J9ObjectPointer leftObject = J9ObjectPointer.cast(o1);			
			J9ObjectPointer rightObject = J9ObjectPointer.cast(o2);
			String leftString = J9ObjectHelper.stringValue(leftObject);
			String rightString = J9ObjectHelper.stringValue(rightObject);
			return leftString.compareTo(rightString);
		}
	}

	public static class StringHashFunction<StructType extends AbstractPointer> implements HashFunction<StructType>
	{
		public UDATA hash(StructType entry)
		{
			try {
				J9ObjectPointer entryObject = J9ObjectPointer.cast(entry);
				String stringValue = J9ObjectHelper.stringValue(entryObject);
				U32 hashCode = new U32(stringValue.hashCode());
				return new UDATA(hashCode);
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error calculating hash", e, false);
				return new UDATA(0);
			}
		}
	}	
	
	public static StringTable from() throws CorruptDataException
	{
		MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
		return new StringTable(extensions.stringTable());
	}
	
	public SlotIterator<J9ObjectPointer> iterator()
	{	
		return new SlotIterator<J9ObjectPointer>() 
		{
			private SlotIterator<PointerPointer> hashTableIterator = null;
			private long currentIndex = 0;


			public boolean hasNext()
			{
				while(true) {
					// Is the current iterator good?
					if (hashTableIterator != null) {
						// Are there still entries?
						if (hashTableIterator.hasNext()) {
							return true;
						}
						
						// Move to the next hash table
						currentIndex++;
					}
					
					// Are there more hash tables?
					if (currentIndex >= _tableCount) {
						hashTableIterator = null;
						return false;
					}
					
					// Get the next hash table iterator
					try {
						J9HashTablePointer hashTable = J9HashTablePointer.cast(_stringTable._table().at(currentIndex)); 
						
						HashTable<PointerPointer> currentHashTable = HashTable.fromJ9HashTable(
								hashTable, 
								false, 
								PointerPointer.class, 
								_hashFn, 
								new StringComparatorFunction<PointerPointer>());					
						hashTableIterator = currentHashTable.iterator();
					} catch (CorruptDataException e) {
						raiseCorruptDataEvent("Error getting next item", e, false);
						// Keep trying
					}
				}
			}

			public J9ObjectPointer next()
			{
				if (hasNext()) {
					PointerPointer next = hashTableIterator.next();
					return J9ObjectPointer.cast(next);
				} else {
					throw new NoSuchElementException("There are no more items available through this iterator");
				}
			}
			
			public VoidPointer nextAddress()
			{
				if (hasNext()) {
					return VoidPointer.cast(hashTableIterator.nextAddress());
				} else {
					throw new NoSuchElementException("There are no more items available through this iterator");
				}
			}

			public void remove()
			{
				throw new UnsupportedOperationException("The image is read only and cannot be modified.");
			}
		
		};
	}

	public SlotIterator<J9ObjectPointer> cacheIterator()
	{	
		return new SlotIterator<J9ObjectPointer>() 
		{
			private UDATA cacheTableIndex;
			private UDATA cacheSize;
			private PointerPointer cache;

			{
				// Instance initializer
				cacheTableIndex = new UDATA(0);
				try {
					cacheSize = getCacheSize();
					cache = _stringTable._cacheEA();
				} catch (CorruptDataException e) {
					raiseCorruptDataEvent("Error getting next item", e, false);
					cacheSize = new UDATA(0);
				}		
			}
			
			public boolean hasNext()
			{
				while(cacheTableIndex.lt(cacheSize)) {
					try {
						if (cache.at(cacheTableIndex).notNull()) {
							return true;
						}
					} catch (CorruptDataException e) {
						raiseCorruptDataEvent("Error getting next item", e, false);
					}
					cacheTableIndex = cacheTableIndex.add(1);
				}
				return false;
			}

			public J9ObjectPointer next()
			{
				if (hasNext()) {
					try {
						J9ObjectPointer cachedString = J9ObjectPointer.cast(cache.at(cacheTableIndex));
						cacheTableIndex = cacheTableIndex.add(1);
						return cachedString;
					} catch (CorruptDataException e) {
						raiseCorruptDataEvent("Error getting next item", e, false);
						return null;
					}
				} else {
					throw new NoSuchElementException("There are no more items available through this iterator");
				}
			}
			
			public VoidPointer nextAddress()
			{
				if (hasNext()) {
					VoidPointer address = VoidPointer.cast(cache.add(cacheTableIndex));
					cacheTableIndex = cacheTableIndex.add(1);
					return address;
				} else {
					throw new NoSuchElementException("There are no more items available through this iterator");
				}
			}

			public void remove()
			{
				throw new UnsupportedOperationException("The image is read only and cannot be modified.");
			}
		
		};
	}
	
	/** 
	 * @param hash value of a string
	 * @return tableIndex given a hash value 
	 */
	private UDATA getTableIndex(UDATA  hash) throws CorruptDataException 
	{
		return hash.mod(_stringTable._tableCount());
	}	
	
	/**
	 * @param tableIndex index of hash table into the array of sub-tables
	 * @return pointer to hash sub-table with provided index
	 * @throws CorruptDataException 
	 */
	private J9HashTablePointer getTable(UDATA tableIndex) throws CorruptDataException 
	{ 
		J9HashTablePointer hashTablePtr = J9HashTablePointer.cast(_stringTable._table().at(tableIndex));
		return hashTablePtr;
	}
	
	public J9ObjectPointer search(J9ObjectPointer objectPointer) throws CorruptDataException 
	{
		UDATA hashCode = _hashFn.hash(PointerPointer.cast(objectPointer));
		UDATA tableIndex = getTableIndex(hashCode);
		J9HashTablePointer hashTablePtr = getTable(tableIndex);
		
		HashTable<J9ObjectPointer> currentHashTable = HashTable.fromJ9HashTable(
				hashTablePtr, 
				false,
				J9ObjectPointer.class, 
				new StringTable.StringHashFunction<J9ObjectPointer>(), 
				new StringTable.StringComparatorFunction<J9ObjectPointer>());

		J9ObjectPointer result = currentHashTable.find(objectPointer);
		return result;
	}

	protected UDATA getCacheSize()
	{
		return new UDATA(MM_StringTable.cacheSize);
	}
}
