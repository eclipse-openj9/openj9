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
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreeNodePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9AVLTreePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITHashTablePointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Mirror of util/jitlook.c
 * 
 * @author andhall
 *
 */
public class JITLook
{

	//#define DETERMINE_BUCKET(value, start, buckets) (((((UDATA)(value) - (UDATA)(start)) >> (UDATA) 9) * (UDATA)sizeof(UDATA)) + (UDATA)(buckets))
	private static PointerPointer DETERMINE_BUCKET(UDATA value, UDATA start, UDATAPointer buckets) 
	{
		return PointerPointer.cast(new UDATA(value.sub(start).rightShift(9).longValue() * UDATA.SIZEOF).add(UDATA.cast(buckets)));
	}
	
	public static J9JITExceptionTablePointer jit_artifact_search(J9AVLTreePointer tree, UDATA searchValue) throws CorruptDataException
	{
		/* find the right hash table to look in */
		AVLTree localTree = AVLTree.fromJ9AVLTreePointer(tree, new JITArtifactSearchCompare());
		J9JITHashTablePointer table = J9JITHashTablePointer.cast(localTree.search(searchValue));
		
		if (table.notNull()) {
			/* return the result of looking in the correct hash table */
			return hash_jit_artifact_search(table, searchValue);
		}
		return J9JITExceptionTablePointer.NULL;
	}

	public static J9JITExceptionTablePointer hash_jit_artifact_search(J9JITHashTablePointer table, UDATA searchValue) throws CorruptDataException
	{
		PointerPointer bucket;
		J9JITExceptionTablePointer entry;
		if (searchValue.gte(table.start()) && searchValue.lt(table.end())) {
			/* The search value is in this hash table */
			bucket = DETERMINE_BUCKET(searchValue, table.start(), table.buckets());
			
			if (bucket.at(0).notNull()) {
				/* The bucket for this search value is not empty */
				if (bucket.at(0).allBitsIn(1)) { //LOW_BIT_SET
					/* The bucket consists of a single low-tagged J9JITExceptionTable pointer */
					entry = J9JITExceptionTablePointer.cast(bucket.at(0));
				}
				else {
					/* The bucket consists of an array of J9JITExceptionTable pointers,
					 * the last of which is low-tagged */
					
					/* Search all but the last entry in the array */
					bucket = PointerPointer.cast(bucket.at(0));
					for ( ; ; bucket = bucket.add(1)) {
						entry = J9JITExceptionTablePointer.cast(bucket.at(0));
						if (entry.allBitsIn(1)) {
							break;
						}
						if (searchValue.gte(entry.startPC()) && searchValue.lt(entry.endWarmPC()))
							return entry;
						if ((entry.startColdPC().longValue() != 0) && searchValue.gte(entry.startColdPC()) && searchValue.lt(entry.endPC()))
							return entry; 
					}
				}
				/* Search the last (or only) entry in the bucket, which is low-tagged */
				entry = J9JITExceptionTablePointer.cast(UDATA.cast(entry).bitAnd(new UDATA(1).bitNot()));
				if (searchValue.gte(entry.startPC()) && searchValue.lt(entry.endWarmPC()))
					return entry;
				if ((entry.startColdPC().longValue() != 0) && searchValue.gte(entry.startColdPC()) && searchValue.lt(entry.endPC()))
					return entry; 
			}
		}
		return J9JITExceptionTablePointer.NULL;
	}
	

	/**
	 * Mirror of avl_jit_artifact_searchCompare
	 * @author andhall
	 *
	 */
	private static class JITArtifactSearchCompare implements IAVLSearchComparator
	{

		public int searchComparator(J9AVLTreePointer tree, UDATA searchValue,
				J9AVLTreeNodePointer node) throws CorruptDataException
		{
			J9JITHashTablePointer walkNode = J9JITHashTablePointer.cast(node);
			
			if (searchValue.gte(walkNode.end())) {
				return -1;
			}
			
			if (searchValue.lt(walkNode.start())) {
				return 1;
			}
			
			return 0;
		}
		
	}

}
