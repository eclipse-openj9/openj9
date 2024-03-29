################################################################################
# Copyright IBM Corp. and others 2017
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
################################################################################

set(gc_vlhgc_sources
	AllocationContextBalanced.cpp
	AllocationContextTarok.cpp
	CardListFlushTask.cpp
	ClassLoaderRememberedSet.cpp
	CollectionSetDelegate.cpp
	CompactGroupManager.cpp
	CompactGroupPersistentStats.cpp
	CompressedCardTable.cpp
	ConfigurationIncrementalGenerational.cpp
	CopyForwardDelegate.cpp
	CopyForwardGMPCardCleaner.cpp
	CopyForwardNoGMPCardCleaner.cpp
	CopyForwardScheme.cpp
	CopyForwardSchemeTask.cpp
	CopyScanCacheChunkVLHGC.cpp
	CopyScanCacheChunkVLHGCInHeap.cpp
	CopyScanCacheListVLHGC.cpp
	CopyScanCacheVLHGC.cpp
	ContinuationObjectBufferVLHGC.cpp
	CycleStateVLHGC.cpp
	EnvironmentVLHGC.cpp
	GlobalAllocationManagerTarok.cpp
	GlobalCollectionCardCleaner.cpp
	GlobalCollectionNoScanCardCleaner.cpp
	GlobalMarkCardCleaner.cpp
	GlobalMarkCardScrubber.cpp
	GlobalMarkDelegate.cpp
	GlobalMarkingScheme.cpp
	GlobalMarkNoScanCardCleaner.cpp
	HeapRegionDataForAllocate.cpp
	HeapRegionDataForCompactVLHGC.cpp
	HeapRegionDescriptorVLHGC.cpp
	HeapRegionManagerVLHGC.cpp
	IncrementalCardTable.cpp
	IncrementalGenerationalGC.cpp
	InterRegionRememberedSet.cpp
	MarkMapManager.cpp
	MemorySubSpaceTarok.cpp
	OwnableSynchronizerObjectBufferVLHGC.cpp
	ParallelSweepSchemeVLHGC.cpp
	ProjectedSurvivalCollectionSetDelegate.cpp
	ReclaimDelegate.cpp
	ReferenceObjectBufferVLHGC.cpp
	RegionBasedOverflowVLHGC.cpp
	RegionListTarok.cpp
	RegionValidator.cpp
	RememberedSetCardBucket.cpp
	RememberedSetCardListBufferIterator.cpp
	RememberedSetCardListCardIterator.cpp
	RememberedSetCardList.cpp
	RuntimeExecManager.cpp
	SchedulingDelegate.cpp
	SweepHeapSectioningVLHGC.cpp
	SweepPoolManagerVLHGC.cpp
	UnfinalizedObjectBufferVLHGC.cpp
	VLHGCAccessBarrier.cpp
	WorkPacketsVLHGC.cpp
	WriteOnceCompactor.cpp
	WriteOnceFixupCardCleaner.cpp
)

j9vm_add_library(j9gcvlhgc STATIC
	${gc_vlhgc_sources}
)

target_link_libraries(j9gcvlhgc
	PRIVATE
		j9vm_interface

		omrgc
		j9gcstructs
		j9gcbase
)

if(OMR_MIXED_REFERENCES_MODE_STATIC)
	j9vm_add_library(j9gcvlhgc_full STATIC
		${gc_vlhgc_sources}
	)

	target_link_libraries(j9gcvlhgc_full
		PRIVATE
			j9vm_interface

			omrgc_full
			j9gcstructs_full
			j9gcbase_full
	)
endif()
