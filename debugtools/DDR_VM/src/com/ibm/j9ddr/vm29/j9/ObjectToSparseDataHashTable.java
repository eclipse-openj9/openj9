/*
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.j9;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SparseDataTableEntryPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * ObjectToSparseDataHashTable is used for accessing hash tables {@link HashTable}
 * in SparseDataTableEntry {@link MM_SparseDataTableEntryPointer} (e.g. iterator).
 */
public final class ObjectToSparseDataHashTable extends HashTable_V1<MM_SparseDataTableEntryPointer>
{
	private ObjectToSparseDataHashTable(
			J9HashTablePointer hashTablePointer,
			boolean isInline,
			Class<MM_SparseDataTableEntryPointer> structType,
			HashEqualFunction<MM_SparseDataTableEntryPointer> equalFn,
			HashFunction<MM_SparseDataTableEntryPointer> hashFn) throws CorruptDataException
	{
		super(hashTablePointer, isInline, structType, equalFn, hashFn);
	}

	/**
	 * Opens J9HashTable from J9HashTablePointer.
	 *
	 * @param structure   the J9HashTablePointer
	 * @throws CorruptDataException   when fails to read from structure
	 */
	public static HashTable<MM_SparseDataTableEntryPointer> fromJ9HashTable(J9HashTablePointer structure) throws CorruptDataException
	{
		return new ObjectToSparseDataHashTable(structure, false, MM_SparseDataTableEntryPointer.class, new SparseDataHashEqualFn(), new SparseDataHashFn());
	}

	private static final class SparseDataHashFn implements HashFunction<MM_SparseDataTableEntryPointer>
	{
		SparseDataHashFn()
		{
			super();
		}

		@Override
		public UDATA hash(MM_SparseDataTableEntryPointer entry) throws CorruptDataException
		{
			try {
				return UDATA.cast(entry._dataPtr());
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e.getMessage());
			}
		}
	}

	private static final class SparseDataHashEqualFn implements HashEqualFunction<MM_SparseDataTableEntryPointer>
	{
		SparseDataHashEqualFn()
		{
			super();
		}

		@Override
		public boolean equal(MM_SparseDataTableEntryPointer entry1, MM_SparseDataTableEntryPointer entry2) throws CorruptDataException
		{
			try {
				return entry1._dataPtr().getAddress() == entry2._dataPtr().getAddress();
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e.getMessage());
			}
		}
	}
}
