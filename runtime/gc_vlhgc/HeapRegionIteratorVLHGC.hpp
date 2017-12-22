
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

#if !defined(HEAPREGIONITERATORVLHGC_HPP)
#define HEAPREGIONITERATORVLHGC_HPP

#include "j9.h"
#include "j9cfg.h"

#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionIterator.hpp"

class GC_HeapRegionIteratorVLHGC : public GC_HeapRegionIterator {

private:
private:
protected:
	
public:
	
	/**
	 * Construct a HeapRegionIterator for the specified heap.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 */
	GC_HeapRegionIteratorVLHGC(MM_HeapRegionManager *manager) 
		: GC_HeapRegionIterator(manager)
		{ }

	/**
	 * Construct a HeapRegionIterator for the regions which belong to the specified memory space.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 * @param space the memory space whose regions should be walked
	 */
	GC_HeapRegionIteratorVLHGC(MM_HeapRegionManager *manager, MM_MemorySpace* space)
		: GC_HeapRegionIterator(manager, space)
		{ }
	
	/**
	 * @param manager The versions of the regions returned will come from this manager
	 * @param includedRegionsMask A bitmap of the HeapRegionDescriptor properties of the regions that should be included in the iterator
	 */
	GC_HeapRegionIteratorVLHGC(MM_HeapRegionManager *manager, U_32 includedRegionsMask)
		: GC_HeapRegionIterator(manager, includedRegionsMask)
		{}

	/**
	 * @return the next region in the heap, or NULL if there are no more regions
	 */
	MM_HeapRegionDescriptorVLHGC *nextRegion() 
	{
		return (MM_HeapRegionDescriptorVLHGC*)GC_HeapRegionIterator::nextRegion();
	}

};


#endif /* HEAPREGIONITERATORVLHGC_HPP */
