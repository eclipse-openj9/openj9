/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.StringTable.StringEqualFunction;
import com.ibm.j9ddr.vm29.j9.StringTable.StringHashFunction;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * PackageHashTable is used for accessing hash tables in J9Packages
 */

public class PackageHashTable extends HashTable_V1<J9PackagePointer>
{
	protected PackageHashTable(J9HashTablePointer hashTablePointer, boolean isInline, Class<J9PackagePointer> structType,
			HashEqualFunction<J9PackagePointer> equalFn,
			HashFunction<J9PackagePointer> hashFn) throws CorruptDataException 
	{
		super(hashTablePointer, isInline, structType, equalFn, hashFn);
	}

	/**
	 * Opens J9HashTable from J9HashTablePointer
	 * 
	 * @param structure
	 * 					the J9HashTablePointer
	 * 
	 * @throws CorruptDataException
	 * 					when fails to read from structure
	 */
	
	public static HashTable<J9PackagePointer> fromJ9HashTable(J9HashTablePointer structure) throws CorruptDataException 
	{
		return new PackageHashTable(structure, false, J9PackagePointer.class, new PackageHashEqualFn(), new PackageHashFn());
	}
	
	private static class PackageHashFn implements HashFunction<J9PackagePointer> 
	{
		PackageHashFn()
		{
			super();
		}

		private StringHashFunction<J9PackagePointer> stringHashFn = new StringTable.StringHashFunction<J9PackagePointer>();

		public UDATA hash(J9PackagePointer entry) throws CorruptDataException {
			return stringHashFn.hash(entry);
		}
	}
	
	private static class PackageHashEqualFn implements HashEqualFunction<J9PackagePointer> 
	{
		PackageHashEqualFn()
		{
			super();
		}

		private StringEqualFunction<J9PackagePointer> stringHashEqualFn = new StringEqualFunction<J9PackagePointer>();

		public boolean equal(J9PackagePointer entry1, J9PackagePointer entry2) throws CorruptDataException {
			return stringHashEqualFn.equal(entry1, entry2);
		}

	}
}
