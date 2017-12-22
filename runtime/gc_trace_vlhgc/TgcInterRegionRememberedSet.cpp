
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "Tgc.hpp"
#include "mmhook.h"

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "HeapRegionManager.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "InterRegionRememberedSet.hpp"
#include "RememberedSetCardListCardIterator.hpp"


/* Histogram buckets */
#define MAX_BUCKETS 18
struct Bucket {
	UDATA regionCount;
	UDATA duplicates;
	UDATA cardCount;
};

enum {
	DISTINCT_FLAG_ARRAY_SHIFT = 4 /* for duplicate/distinct card list count we used a hash table, this is a factor which tells how hash table is big relative to RSCL */
};

static UDATA
getUpperBoundDuplicatesCount(MM_EnvironmentVLHGC *env, MM_RememberedSetCardList *rscl)
{
	UDATA duplicateCount = 0;

	if (MM_GCExtensions::getExtensions(env)->tarokTgcEnableRememberedSetDuplicateDetection) {
		MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(MM_GCExtensions::getExtensions(env));

		/* If lazy initialize of _rsclDistinctFlagArray failed, we'll skip calculating duplicates */
		if (NULL != tgcExtensions->_rsclDistinctFlagArray) {
			UDATA cardListSize = rscl->getSize(env);
			UDATA distinctFlagArraySize = cardListSize << DISTINCT_FLAG_ARRAY_SHIFT;
			for (UDATA i = 0; i < distinctFlagArraySize; i++) {
				tgcExtensions->_rsclDistinctFlagArray[i] = 0;
			}

			GC_RememberedSetCardListCardIterator rsclCardIterator(rscl);
			void *cardAddress = NULL;
			while(NULL != (cardAddress = rsclCardIterator.nextReferencingCardHeapAddress(env))) {
				/* card addresses are CARD_SIZE bytes aligned, thus stripping low bits that are 0s (use the shift value corresponding to the size) */
				/* prime 100000007 should be larger than distinctFlagArraySize * 8bits */
				UDATA hash = ((UDATA)cardAddress >> CARD_SIZE_SHIFT) % 100000007;
				UDATA bit = hash & 0x7;
				UDATA index = (hash >> 3) % distinctFlagArraySize;
				tgcExtensions->_rsclDistinctFlagArray[index] |= (1 << bit);
			}

			UDATA distinctCount = 0;

			for (UDATA i = 0; i < distinctFlagArraySize; i++) {
				U_8 flag = tgcExtensions->_rsclDistinctFlagArray[i];
				if (0 != flag) {
					for (UDATA bitShift = 0; bitShift < 8; bitShift++) {
						U_8 bit = 1 << bitShift;
						if (bit == (tgcExtensions->_rsclDistinctFlagArray[i] & bit)) {
							distinctCount += 1;
						}
					}
				}
			}

			duplicateCount = cardListSize - distinctCount;
		}
	}

	return duplicateCount;

}

/**
 * Print a histogram of the objects using classList
 */
static void
calculateAndPrintHistogram(J9VMThread *vmThread, MM_HeapRegionManager *regionManager, const char *eventString, UDATA totalReferences, UDATA maxReferences)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	maxReferences = extensions->tarokRememberedSetCardListMaxSize;
	UDATA regionCount = regionManager->getTableRegionCount();
	double totalPercent = ((double)totalReferences / ((double)regionCount * (double)extensions->tarokRememberedSetCardListSize)) * 100.0;
	UDATA totalDuplicates = 0;

	tgcExtensions->printf("{RSCL: %zu (%.2f%%) total reference cards to regions; max %zu per region}\n", totalReferences, totalPercent, maxReferences);
	if (maxReferences > 0) {
		Bucket buckets[MAX_BUCKETS];
		UDATA maxForBucket[MAX_BUCKETS - 1];
		UDATA increment = (maxReferences + MAX_BUCKETS - 3) / (MAX_BUCKETS - 2);
		UDATA i = 0;
		for (i = 0; i < MAX_BUCKETS; i++) {
			buckets[i].regionCount = 0;
			buckets[i].duplicates = 0;
			buckets[i].cardCount = 0;
		}
		for (i = 0; i < MAX_BUCKETS - 1; i++) {
			maxForBucket[i] = i * increment;
		}

		tgcExtensions->printf("{RSCL: Histogram of reference cards TO regions after %s. Duplicates percentage in brackets left to the region count. }\n{RSCL: Refs     ", eventString);
		UDATA minForBucket = 0;
		for (i = 0; i < MAX_BUCKETS - 1; i++) {
			if (minForBucket < maxForBucket[i]) {
				if (100000 <= maxForBucket[i]) {
					tgcExtensions->printf("  <=%3zuK", maxForBucket[i]/1024);
				} else {
					tgcExtensions->printf(" <=%5zu", maxForBucket[i]);
				}
			} else {
				tgcExtensions->printf("  =%5zu", maxForBucket[i]);
			}
			minForBucket = maxForBucket[i] + 1;
		}
		tgcExtensions->printf("      OF");

		for (UDATA age = 0; age <= extensions->tarokRegionMaxAge; age++) {
			for (i = 0; i < regionCount; i++) {
				MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)regionManager->physicalTableDescriptorForIndex(i);
				if (age == region->getLogicalAge()) {
					UDATA cardCount = region->getRememberedSetCardList()->getSize(env);
					UDATA duplicates = getUpperBoundDuplicatesCount(env, region->getRememberedSetCardList());
					UDATA bucketIndex;
					if (region->getRememberedSetCardList()->isOverflowed()) {
						bucketIndex = MAX_BUCKETS - 1;
					} else {
						bucketIndex = (cardCount + increment - 1) / increment;
					}
					buckets[bucketIndex].regionCount += 1;
					buckets[bucketIndex].cardCount += cardCount;
					buckets[bucketIndex].duplicates += duplicates;
					totalDuplicates += duplicates;
				}
			}

			tgcExtensions->printf(" }\n{RSCL: age %2zu   ", age);
			for (i = 0; i < MAX_BUCKETS; i++) {
				UDATA duplicatePercentage = 0;
				if (0 != buckets[i].cardCount) {
					duplicatePercentage = 100 * buckets[i].duplicates / buckets[i].cardCount;
				}
				if (0 != duplicatePercentage) {
					tgcExtensions->printf(" (%2zu)%3zu", duplicatePercentage, buckets[i].regionCount);
				} else {
					tgcExtensions->printf(" %7zu", buckets[i].regionCount);
				}
				buckets[i].regionCount = 0;
				buckets[i].duplicates = 0;
				buckets[i].cardCount = 0;
			}
		}

		tgcExtensions->printf(" }\n");
		tgcExtensions->printf("{RSCL: Total duplicates: %zu%% }\n", totalReferences ? (totalDuplicates * 100 / totalReferences): (UDATA)0);
	}
}


/**
 * Report RSCL histogram prior to a collection
 */
static void
tgcHookReportInterRegionRememberedSetHistogram(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* vmThread = NULL;
	const char *eventString = NULL;
	J9JavaVM *javaVM = (J9JavaVM *) userData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	MM_EnvironmentVLHGC env(javaVM);
	J9HookInterface** externalHookInterface = extensions->getHookInterface();
	J9HookInterface** privateHookInterface = extensions->getPrivateHookInterface();

	if ((J9HOOK_MM_PRIVATE_MARK_END == eventNum) && ( privateHookInterface == hookInterface)) {
		MM_MarkEndEvent* event = (MM_MarkEndEvent*)eventData;
		vmThread = static_cast<J9VMThread *>(event->currentThread->_language_vmthread);
		eventString = "Mark";
	} else if ((J9HOOK_MM_OMR_COMPACT_END == eventNum) && ( externalHookInterface == hookInterface)) {
		MM_CompactEndEvent* event = (MM_CompactEndEvent*)eventData;
		vmThread = (J9VMThread *)event->omrVMThread->_language_vmthread;
		eventString = "Compact";
	} else if (J9HOOK_MM_PRIVATE_COPY_FORWARD_END == eventNum && ( privateHookInterface == hookInterface)) {
		MM_CopyForwardEndEvent* event = (MM_CopyForwardEndEvent*)eventData;
		vmThread = static_cast<J9VMThread *>(event->currentThread->_language_vmthread);
		eventString = "CopyForward";
	} else {
		Assert_MM_unreachable();
	}

	/* Lazy allocate for _rsclDistinctFlagArray
	 * (count not be done in Initialize, since tarokRememberedSetCardListMaxSize might have not be set yet
	 */
	if (NULL == tgcExtensions->_rsclDistinctFlagArray) {
		tgcExtensions->_rsclDistinctFlagArray = (U_8 *)extensions->getForge()->allocate(extensions->tarokRememberedSetCardListMaxSize << DISTINCT_FLAG_ARRAY_SHIFT, MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	}

	MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
	UDATA regionCount = regionManager->getTableRegionCount();

	UDATA maxCardsToRegion = 0;
	UDATA totalCardsToRegions = 0;

	for (UDATA i = 0; i < regionCount; i++) {
		MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC *)regionManager->physicalTableDescriptorForIndex(i);
		maxCardsToRegion = OMR_MAX(maxCardsToRegion, region->getRememberedSetCardList()->getSize(&env));
		totalCardsToRegions += region->getRememberedSetCardList()->getSize(&env);
	}

	calculateAndPrintHistogram(vmThread, regionManager, eventString, totalCardsToRegions, maxCardsToRegion);
}


/**
 * Initialize inter region remembered set  tgc tracing.
 * Initializes the TgcInterRegionRememberedSetExtensions object associated with irrs tgc tracing. Attaches hooks
 * to the appropriate functions handling events used by irrs tgc tracing.
 */
bool
tgcInterRegionRememberedSetInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_MARK_END, tgcHookReportInterRegionRememberedSetHistogram, OMR_GET_CALLSITE(), javaVM);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_COMPACT_END, tgcHookReportInterRegionRememberedSetHistogram, OMR_GET_CALLSITE(), javaVM);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_END, tgcHookReportInterRegionRememberedSetHistogram, OMR_GET_CALLSITE(), javaVM);

	return result;
}

void
tgcInterRegionRememberedSetTearDown(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	if (NULL != tgcExtensions->_rsclDistinctFlagArray) {
		extensions->getForge()->free(tgcExtensions->_rsclDistinctFlagArray);
	}
}
