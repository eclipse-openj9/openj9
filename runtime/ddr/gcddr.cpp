/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "AllocateDescription.hpp"
#include "AllocationCategory.hpp"
#include "CompactScheme.hpp"
#include "ConcurrentCardTable.hpp"
#include "CopyScanCacheStandard.hpp"
#include "FreeHeapRegionList.hpp"
#include "GCExtensions.hpp"
#include "HeapIteratorAPI.h"
#include "HeapMap.hpp"
#include "IncrementalCardTable.hpp"
#include "MemoryPool.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "MemoryPoolHybrid.hpp"
#include "MemoryPoolSplitAddressOrderedList.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "ScavengerForwardedHeader.hpp"
#include "StringTable.hpp"
#include "SweepPoolManager.hpp"
#include "SweepPoolManagerVLHGC.hpp"

#if defined(J9VM_GC_FINALIZATION)
# include "FinalizeListManager.hpp"
#endif /* J9VM_GC_FINALIZATION */

#if defined(OMR_GC_SEGREGATED_HEAP)
# include "MemoryPoolSegregated.hpp"
# include "MemorySubSpaceSegregated.hpp"
# include "ObjectHeapIteratorSegregated.hpp"
# include "SegregatedGC.hpp"
#endif /* OMR_GC_SEGREGATED_HEAP */

#include "ddrhelp.h"

#define GC_DdrDebugLink(type) DdrDebugLink(gc, type)

GC_DdrDebugLink(J9ModronAllocateHint)
GC_DdrDebugLink(MM_AllocateDescription)
GC_DdrDebugLink(MM_AllocationCategory)
GC_DdrDebugLink(MM_ConcurrentCardTable)
GC_DdrDebugLink(MM_CopyScanCacheStandard)
GC_DdrDebugLink(MM_FreeHeapRegionList)
GC_DdrDebugLink(MM_GCExtensions)
GC_DdrDebugLink(MM_HeapLinkedFreeHeader)
GC_DdrDebugLink(MM_HeapMap)
GC_DdrDebugLink(MM_HeapRegionDescriptor)
GC_DdrDebugLink(MM_HeapRegionDescriptor::RegionType)
GC_DdrDebugLink(MM_IncrementalCardTable)
GC_DdrDebugLink(MM_LargeObjectAllocateStats)
GC_DdrDebugLink(MM_MemoryPoolAddressOrderedList)
GC_DdrDebugLink(MM_MemoryPoolHybrid)
GC_DdrDebugLink(MM_MemoryPoolSplitAddressOrderedList)
GC_DdrDebugLink(MM_RealtimeMarkingScheme)
GC_DdrDebugLink(MM_ScavengerForwardedHeader)
GC_DdrDebugLink(MM_StringTable)
GC_DdrDebugLink(MM_SweepPoolManagerVLHGC)

#if defined(J9VM_GC_FINALIZATION)
GC_DdrDebugLink(GC_FinalizeListManager)
#endif /* J9VM_GC_FINALIZATION */

#if defined(OMR_GC_SEGREGATED_HEAP)
GC_DdrDebugLink(GC_ObjectHeapIteratorSegregated)
GC_DdrDebugLink(MM_MemoryPoolSegregated)
GC_DdrDebugLink(MM_MemorySubSpaceSegregated)
GC_DdrDebugLink(MM_SegregatedGC)
#endif /* OMR_GC_SEGREGATED_HEAP */

/*
 * Suggest to compilers that they include fuller descriptions of certain types.
 */

class DDR_MM_AllocateDescription : public MM_AllocateDescription
{
public:
	MM_MemorySubSpace::AllocationType _ddrAllocationType;
	MM_MemorySubSpace::AllocationType getAllocationType();
};

MM_MemorySubSpace::AllocationType
DDR_MM_AllocateDescription::getAllocationType()
{
	return this->_ddrAllocationType;
}

class DDR_MM_HeapRegionDescriptor : public MM_HeapRegionDescriptor
{
public:
	MM_HeapRegionDescriptor::RegionType _ddrRegionType;
	MM_HeapRegionDescriptor::RegionType getRegionType();
};

MM_HeapRegionDescriptor::RegionType
DDR_MM_HeapRegionDescriptor::getRegionType()
{
	return this->_ddrRegionType;
}

class DDR_MM_MemoryPoolHybrid : public MM_MemoryPoolHybrid
{
public:
	const char * ddrHelper();
};

const char *
DDR_MM_MemoryPoolHybrid::ddrHelper()
{
	return this->_typeId;
}

class DDR_MM_HeapRegionList : public MM_HeapRegionList
{
public:
	MM_HeapRegionList::RegionListKind _ddrRegionListKind;
	MM_HeapRegionList::RegionListKind getRegionListKind();
};

MM_HeapRegionList::RegionListKind
DDR_MM_HeapRegionList::getRegionListKind()
{
	return this->_ddrRegionListKind;
}

#if defined(OMR_GC_MODRON_COMPACTION)

class DDR_CompactMemoryPoolState : public MM_CompactMemoryPoolState
{
public:
	const char * ddrHelper();
};

const char *
DDR_CompactMemoryPoolState::ddrHelper()
{
	return this->_typeId;
}

#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_SEGREGATED_HEAP)

class DDR_GC_ObjectHeapIteratorSegregated : public GC_ObjectHeapIteratorSegregated
{
public:
	MM_HeapRegionDescriptor::RegionType _ddrRegionType;
	MM_HeapRegionDescriptor::RegionType getRegionType();
};

MM_HeapRegionDescriptor::RegionType
DDR_GC_ObjectHeapIteratorSegregated::getRegionType()
{
	return this->_ddrRegionType;
}

class DDR_SegregatedGC : public MM_SegregatedGC
{
public:
	MM_SegregatedMarkingScheme *getMarkingScheme();
};

MM_SegregatedMarkingScheme *
DDR_SegregatedGC::getMarkingScheme()
{
	return this->_markingScheme;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
