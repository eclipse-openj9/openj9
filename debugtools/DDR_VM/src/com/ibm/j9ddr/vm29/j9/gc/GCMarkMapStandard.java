/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.gc;

import java.util.ArrayList;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapMapPointer;
import com.ibm.j9ddr.vm29.structure.MM_CompactScheme;
import com.ibm.j9ddr.vm29.structure.MM_HeapMap;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCMarkMapStandard extends GCMarkMap 
{
	public GCMarkMapStandard(MM_HeapMapPointer heapMap) throws CorruptDataException
	{
		super(heapMap);
	}
	
	@Override
	public int getPageSize(J9ObjectPointer object)
	{
		int result = super.getPageSize(object);
		try {
			if ((object != null) && object.gte(_heapBase) && object.lt(_heapTop)) {
				UDATA heapBaseOffset = UDATA.cast(object).sub(UDATA.cast(_heapBase));
				
				UDATA pageIndex = heapBaseOffset.div(MM_CompactScheme.sizeof_page).mult(2);	// pairs of UDATA, so *2
				UDATA compactTableEntry_addr = _heapMapBits.at(pageIndex);
				if(compactTableEntry_addr.allBitsIn(3)) {
					result = (int)MM_CompactScheme.sizeof_page;
				}
			}
		} catch (CorruptDataException cde) {
			// Ignore this particular error -- surely the calling code will hit it
		}
		
		return result;
	}
	
	@Override
	public MarkedObject[] queryRange(J9ObjectPointer base, J9ObjectPointer top) throws CorruptDataException
	{
		 /* Naive implementation; should work but slow -- ever more than the super.
		  * Add handling for compacted records in the mark map. */
		ArrayList<MarkedObject> results = new ArrayList<MarkedObject>();
		if (base.lt(_heapBase)) {
			base = J9ObjectPointer.cast(_heapBase);
		}
		if (top.gt(_heapTop)) {
			top = J9ObjectPointer.cast(_heapTop);
		}
		if (base.gt(top)) {
			base = top;
		}
		J9ObjectPointer cursor = base;
		while (cursor.lt(top)) {
			UDATA heapBaseOffset = UDATA.cast(cursor).sub(UDATA.cast(_heapBase));
						
			UDATA pageIndex = heapBaseOffset.div(MM_CompactScheme.sizeof_page).mult(2);	// pairs of UDATA, so *2
			UDATA compactTableEntry_addr = _heapMapBits.at(pageIndex);
			UDATA compactTableEntry_bits = _heapMapBits.at(pageIndex.add(1));

			// Horribly inefficient -- recomputing relocations every single time!
			if(compactTableEntry_addr.allBitsIn(3)) {
				// Object has been compacted -- assuming that the pointer was the pre-compacted one
				J9ObjectPointer newObject = J9ObjectPointer.cast(compactTableEntry_addr.bitAnd(~3));
				UDATA bits = compactTableEntry_bits;
				
				UDATA offset = heapBaseOffset.mod(MM_CompactScheme.sizeof_page).div(2 * MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * UDATA.SIZEOF);
				UDATA bitMask = new UDATA(1).leftShift(offset);
				if(bits.bitAnd(bitMask).eq(bitMask)) {
					long mask = 1; 
					int ordinal = 0;
					for (int i = offset.intValue(); i > 0; i--) {
						if (bits.allBitsIn(mask)) {
							ordinal += 1;
						}
						mask <<= 1;
					}
					for(int i = 0; i < ordinal; i++) {
						UDATA objectSize = ObjectModel.getConsumedSizeInBytesWithHeader(newObject);
						newObject = newObject.addOffset(objectSize);
					}
					
					results.add(new MarkedObject(cursor, _heapMapBits.add(pageIndex), newObject));
				}

				cursor = cursor.addOffset(getObjectGrain() * 2);

			} else {
				/* Same as super */
				UDATA[] indexAndMask = getSlotIndexAndMask(cursor);
				UDATAPointer slot = _heapMapBits.add(indexAndMask[0]);
				if (slot.at(0).bitAnd(indexAndMask[1]).gt(0)) {
					results.add(new MarkedObject(cursor, slot));
				}

				cursor = cursor.addOffset(getObjectGrain());
			}
		}
		
		return results.toArray(new MarkedObject[results.size()]);
	}
}
