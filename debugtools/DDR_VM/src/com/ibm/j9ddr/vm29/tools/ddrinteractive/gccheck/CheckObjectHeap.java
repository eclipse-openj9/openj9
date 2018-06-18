/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.CorruptDataException;

import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCScavengerForwardedHeader;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.UDATA;
import static com.ibm.j9ddr.vm29.structure.J9MemorySegment.MEMORY_TYPE_NEW;

import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.CheckBase.*;

class CheckObjectHeap extends Check
{
	@Override
	public void check()
	{
		// Design diverges here from GC_CheckObjectHeap
		// Use iterators directly
		try {
			GCHeapRegionIterator regions = GCHeapRegionIterator.from();
			boolean midScavenge = _engine.isMidscavengeFlagSet();
			boolean isVLHGC = GCExtensions.isVLHGC();

			while (regions.hasNext()) {
				GCHeapRegionDescriptor region = GCHeapRegionDescriptor.fromHeapRegionDescriptor(regions.next());
				boolean isRegionTypeNew = region.getTypeFlags().allBitsIn(MEMORY_TYPE_NEW);
				
				GCObjectHeapIterator heapIterator = region.objectIterator(true, true);
				while(heapIterator.hasNext()) {
					J9ObjectPointer object = heapIterator.peek();

					if (midScavenge && (isVLHGC || isRegionTypeNew)) {
						GCScavengerForwardedHeader scavengerForwardedHeader = GCScavengerForwardedHeader.fromJ9Object(object);
						if (scavengerForwardedHeader.isForwardedPointer()) {
							//forwarded pointer is discovered
							//report it
							_engine.reportForwardedObject(object, scavengerForwardedHeader.getForwardedObject());
							
							//and skip it by advancing of iterator to the next object
							UDATA objectSize = scavengerForwardedHeader.getObjectSize();
							heapIterator.advance(objectSize);
							_engine.pushPreviousObject(object);
							continue;
						}
					}

					int result = _engine.checkObjectHeap(object, region);
					if(result != J9MODRON_SLOT_ITERATOR_OK) {
						break;
					}

					heapIterator.next();
					_engine.pushPreviousObject(object);
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "HEAP";
	}

	@Override
	public void print()
	{
		// This method intentionally left blank
	}

}
