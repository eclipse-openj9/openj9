
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(HEAPREGIONDATAFORCOMPACTVLHGC_HPP)
#define HEAPREGIONDATAFORCOMPACTVLHGC_HPP

#include "j9.h"
#include "j9cfg.h"

#include "BaseNonVirtual.hpp"

class MM_AllocationContextTarok;
class MM_EnvironmentVLHGC;
class MM_HeapRegionDescriptor;
class MM_HeapRegionDescriptorVLHGC;
class MM_HeapRegionManager;
class MM_MemoryPool;

/**
 * The descriptor type that contains information specific to compact
 */
class MM_HeapRegionDataForCompactVLHGC : public MM_BaseNonVirtual
{
public:
	bool _shouldCompact;	/**< true if this region is to be compacted in the current cycle */
	bool _shouldFixup; /**< true if this region requires a fixup after compaction in the current cycle (a proper superset of _shouldCompact regions) */
	double _compactScore; /**< the score used to determine the value of compacting this region (normalized into the range [0..100] (where 100 is high)) */
	void * volatile _compactDestination;	/**< The address where new objects can begin being copied into this region during a compaction (NULL if there is no room) */
	MM_HeapRegionDescriptorVLHGC *_nextInWorkList;	/**< The next instance in the receiver's work list */
	MM_HeapRegionDescriptorVLHGC *_blockedList;	/**< The list of regions (connected using nextInWorkList links) which cannot be evacuated until some part of the receiver is */
	void * volatile _nextEvacuationCandidate;	/**< The address within the region where our next evacuation attempt must begin */
	void * volatile _nextRebuildCandidate;	/**< The address within the region where our next attempt at mark map rebuilding must begin */
    void *_nextMoveEventCandidate;	/**< The address within the region where our next attempt at object move event reporting must begin */
    bool _isCompactDestination;	/**< True if planning for the current compaction has decided to relocate objects from other regions into this one */
    UDATA _vineDepth;	/**< The longest path from this region to a region with no compaction prerequisites, following the prerequisite chain.  This is updated during planning and is used to select the optimal extra compaction region */
    MM_AllocationContextTarok *_previousContext; /**< Points to the previous context which did own a region prior to evacuation in the case where it is selected as a new compact destination prior to recycling */
    double _projectedLiveBytesRatio; /**< (estimate of live bytes in region) / (post move region size in bytes).  More or less the same thing as survival rate.  */
protected:
private:
	
public:
	
	MM_HeapRegionDataForCompactVLHGC(MM_EnvironmentVLHGC *env);

	bool initialize(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor);
	void tearDown(MM_EnvironmentVLHGC *env);
	
private:
};

#endif /* HEAPREGIONDATAFORCOMPACTVLHGC_HPP */
