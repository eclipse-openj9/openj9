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
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * ModuleHashTable is used for accessing hash tables in J9Modules (e.g. readAccessHashTable)  
 */

public class ModuleHashTable extends HashTable_V1<J9ModulePointer>
{
	protected ModuleHashTable(J9HashTablePointer hashTablePointer, boolean isInline, Class<J9ModulePointer> structType,
			HashEqualFunction<J9ModulePointer> equalFn,
			HashFunction<J9ModulePointer> hashFn) throws CorruptDataException 
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
	
	public static HashTable<J9ModulePointer> fromJ9HashTable(J9HashTablePointer structure) throws CorruptDataException 
	{
		return new ModuleHashTable(structure, false, J9ModulePointer.class, new ModuleHashEqualFn(), new ModuleHashFn());
	}
	
	private static class ModuleHashFn implements HashFunction<J9ModulePointer> 
	{
		ModuleHashFn()
		{
			super();
		}

		private StringHashFunction<J9ModulePointer> stringHashFn = new StringTable.StringHashFunction<J9ModulePointer>();

		public UDATA hash(J9ModulePointer entry) throws CorruptDataException {
			return stringHashFn.hash(entry);
		}
	}
	
	private static class ModuleHashEqualFn implements HashEqualFunction<J9ModulePointer> 
	{
		ModuleHashEqualFn()
		{
			super();
		}

		private StringEqualFunction<J9ModulePointer> stringHashEqualFn = new StringEqualFunction<J9ModulePointer>();

		public boolean equal(J9ModulePointer entry1, J9ModulePointer entry2) throws CorruptDataException {
			return stringHashEqualFn.equal(entry1, entry2);
		}

	}
}
