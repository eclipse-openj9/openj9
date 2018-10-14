/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
GC_DdrDebugLink(MM_IncrementalCardTable)
GC_DdrDebugLink(MM_LargeObjectAllocateStats)
GC_DdrDebugLink(MM_MemoryPoolAddressOrderedList)
GC_DdrDebugLink(MM_MemoryPoolHybrid)
GC_DdrDebugLink(MM_MemoryPoolSplitAddressOrderedList)
GC_DdrDebugLink(MM_RealtimeMarkingScheme)
GC_DdrDebugLink(MM_ScavengerForwardedHeader)
GC_DdrDebugLink(MM_StringTable)
GC_DdrDebugLink(MM_SweepPoolManagerVLHGC)
GC_DdrDebugLink(MM_HeapRegionDescriptor::RegionType)

#if defined(J9VM_GC_FINALIZATION)
GC_DdrDebugLink(GC_FinalizeListManager)
#endif /* J9VM_GC_FINALIZATION */

#if defined(OMR_GC_SEGREGATED_HEAP)
GC_DdrDebugLink(MM_MemoryPoolSegregated)
GC_DdrDebugLink(MM_MemorySubSpaceSegregated)
GC_DdrDebugLink(MM_SegregatedGC)
#endif /* OMR_GC_SEGREGATED_HEAP */
