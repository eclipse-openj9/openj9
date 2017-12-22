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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.types.UDATA;


public abstract class HashTable<StructType extends AbstractPointer> implements IHashTable<StructType>
{
	protected J9HashTablePointer _table;	
	protected Class<StructType> _structType;
	protected HashEqualFunction<StructType> _equalFn;
	protected HashFunction<StructType> _hashFn;
	protected HashComparatorFunction<StructType> _comparatorFn;
	
	/**
	 * Encapsulation of the equal function needed by the HashTable
	 *
	 * @param <StructType>
	 */
	public static interface HashEqualFunction<StructType> 
	{
		public boolean equal(StructType entry1, StructType entry2) throws CorruptDataException;
	}
	
	/**
	 * Encapsulation of the hash function needed by the HashTable
	 *
	 * @param <StructType>
	 */
	public static interface HashFunction<StructType> 
	{
		public UDATA hash(StructType entry) throws CorruptDataException;
	}
	
	/**
	 * Encapsulation of the hash comparator function needed by the HashTable
	 *
	 * @param <StructType>
	 */	
	interface HashComparatorFunction<StructType extends AbstractPointer> {
		public int compare(StructType o1, StructType o2) throws CorruptDataException;
	}
	
	/* Do not instantiate. Use the factory */
	protected HashTable(J9HashTablePointer structure, Class<StructType> structType, HashEqualFunction<StructType> equalFn, HashFunction<StructType> hashFn, HashComparatorFunction<StructType> comparatorFn) throws CorruptDataException 
	{
		_table = structure;
		this._structType = structType;
		this._comparatorFn = comparatorFn;		
		this._equalFn = equalFn;
		this._hashFn = hashFn;
	}

	/**
	 * Factory method to construct an appropriate hashtable handler.
	 * @param <T>
	 * 
	 * @param structure the J9HashTable structure to use
	 * @param structType the element type
	 * @param equalFn the equal function
	 * @param hashFn the hash function
	 * 
	 * @return an instance of HashTable 
	 * @throws CorruptDataException 
	 */
	public static <T extends AbstractPointer> HashTable<T> fromJ9HashTable(
			J9HashTablePointer structure,
			boolean isInline,
			Class<T> structType, 
			HashEqualFunction<T> equalFn, 
			HashFunction<T> hashFn) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.HASH_TABLE_VERSION);
		switch (version.getAlgorithmVersion()) {			
			case 1:
				// Add case statements for new algorithm versions
			default:
				return new HashTable_V1<T>(structure, isInline, structType, equalFn, hashFn);
		}
	}
	
	/**
	 * Factory method to construct an appropriate hashtable handler.
	 * @param <T>
	 * 
	 * @param structure the J9HashTable structure to use
	 * @param structType the element type
	 * @param equalFn the equal function
	 * @param hashFn the hash function
	 * 
	 * @return an instance of HashTable 
	 * @throws CorruptDataException 
	 */
	public static <T extends AbstractPointer> HashTable<T> fromJ9HashTable(
			J9HashTablePointer structure,	
			boolean isInline,		
			Class<T> structType,			
			HashFunction<T> hashFn,			
			HashComparatorFunction<T> comparatorFn) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.HASH_TABLE_VERSION);
		switch (version.getAlgorithmVersion()) {
			case 1:
			// Add case statements for new algorithm versions
			default:
				return new HashTable_V1<T>(structure, isInline, structType, hashFn, comparatorFn);
		}
	}
	
	/**
	 *	Returns the name of the table
	 *
	 * @return the number of elements in the pool
	 */
	public abstract String getTableName();
	
	/**
	 *	Returns the number of elements in a given pool.
	 *
	 * @return the number of elements in the pool
	 */
	public abstract long getCount();
	
	/** 
	 * Find an entry in the hash table. 

	 * @param entry 
	 * @return NULL if entry is not present in the table; otherwise a pointer to the user-data
	 * @throws CorruptDataException 
	 */
	public abstract StructType find(StructType entry) throws CorruptDataException;
	
	
	/**
	 * Returns an iterator over the elements in the pool
	 * @return an Iterator 
	 */
	public abstract SlotIterator<StructType> iterator();
		
}
